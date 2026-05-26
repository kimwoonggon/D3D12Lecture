# Chapter 13 - Sprite 렌더링 코드 요약

## 학습 목표

- 2D 스프라이트를 D3D12 파이프라인 위에서 렌더링하는 방법 이해
- 텍스처 아틀라스(Sprite Atlas)에서 특정 RECT 구간을 샘플링하는 기법
- 3D와 2D 렌더링이 공존하는 구조 설계
- 상수 버퍼를 렌더 타입별로 관리하는 `CConstantBufferManager` 패턴

---

## 프로젝트 구조

```
13_Sprite/
├── main.cpp                   # 앱 진입점 / 게임 루프
├── D3D12Renderer.h/.cpp       # 렌더러 (3D + 2D Sprite API 포함)
├── Renderer_typedef.h         # 스왑체인/프레임 상수
├── typedef.h                  # 공용 구조체 및 열거형
├── BasicMeshObject.h/.cpp     # 3D 인덱스 메시 (Ch12에서 유지)
├── SpriteObject.h/.cpp        # [신규] 2D 스프라이트 렌더링 클래스
├── ConstantBufferManager.h/.cpp # [신규] CB 타입별 풀 관리자
├── SimpleConstantBufferPool.h/.cpp
├── DescriptorPool.h/.cpp
├── SingleDescriptorAllocator.h/.cpp
├── D3D12ResourceManager.h/.cpp
├── Shaders/
│   ├── shBasicMesh.hlsl       # 3D 메시 셰이더 (유지)
│   └── shSprite.hlsl          # [신규] 2D 스프라이트 셰이더
└── sprite_1024x1024.dds       # [신규] 스프라이트 아틀라스 텍스처
```

---

## 핵심 데이터 구조

### CONSTANT_BUFFER_SPRITE (`typedef.h`)
```cpp
struct CONSTANT_BUFFER_SPRITE
{
    XMFLOAT2 ScreenRes;       // 백버퍼 해상도 (픽셀)
    XMFLOAT2 Pos;             // 스프라이트 좌상단 위치 (픽셀)
    XMFLOAT2 Scale;           // 스케일 배율
    XMFLOAT2 TexSize;         // 텍스처 전체 크기 (픽셀)
    XMFLOAT2 TexSampePos;     // 아틀라스 내 샘플 시작 좌표 (픽셀)
    XMFLOAT2 TexSampleSize;   // 샘플 영역 크기 (픽셀)
    float    Z;               // Depth (0.0 ~ 1.0)
    float    Alpha;           // 투명도
    float    Reserved0;
    float    Reserved1;
};
```

### CONSTANT_BUFFER_TYPE 열거형 (`typedef.h`)
```cpp
enum CONSTANT_BUFFER_TYPE
{
    CONSTANT_BUFFER_TYPE_DEFAULT,   // 3D 메시용
    CONSTANT_BUFFER_TYPE_SPRITE,    // 2D 스프라이트용
    CONSTANT_BUFFER_TYPE_COUNT
};
```

---

## CSpriteObject 클래스 (`SpriteObject.h/.cpp`)

### 설계 패턴: 공유 리소스 + 레퍼런스 카운팅
스프라이트 인스턴스가 여러 개여도 RootSignature, PSO, VB, IB는 **모든 인스턴스가 공유**한다.  
첫 번째 인스턴스가 생성될 때 공유 리소스를 초기화하고, 마지막 인스턴스가 소멸될 때 해제한다.

```cpp
// 모든 인스턴스 공유 (static)
static ID3D12RootSignature* m_pRootSignature;
static ID3D12PipelineState* m_pPipelineState;
static ID3D12Resource*      m_pVertexBuffer;
static ID3D12Resource*      m_pIndexBuffer;
static DWORD                m_dwInitRefCount;   // 참조 카운트
```

### 초기화 흐름

```
CSpriteObject::Initialize()
    └─ InitCommonResources()        ← m_dwInitRefCount 가 0이면 실행
           ├─ InitRootSignature()   ← CBV + SRV 2-range Descriptor Table 생성
           ├─ InitPipelineState()   ← shSprite.hlsl 컴파일 & PSO 생성
           └─ InitMesh()            ← 단위 사각형(Quad) VB/IB GPU 업로드
```

### 루트 시그니처 구성
```
DescriptorTable (Slot 0):
  ├── Range[0]: CBV  b0  ← CONSTANT_BUFFER_SPRITE
  └── Range[1]: SRV  t0  ← 2D 텍스처
StaticSampler s0: POINT filter
```

### 단위 Quad 버텍스 (`InitMesh`)
```
(0,1)──────(1,1)      ← 좌상단이 (0,1), UV도 Y 반전
  │  \       │         (스크린 좌표: 좌상단 원점, Y 아래↓)
  │    \     │
(0,0)──────(1,0)
```

| 버텍스 | Position        | TexCoord |
|--------|-----------------|----------|
| 0      | (0, 1, 0)       | (0, 1)   |
| 1      | (0, 0, 0)       | (0, 0)   |
| 2      | (1, 0, 0)       | (1, 0)   |
| 3      | (1, 1, 0)       | (1, 1)   |

인덱스: `{0,1,2}, {0,2,3}` (두 삼각형)

### Draw 메서드
```cpp
// 등록한 텍스처+RECT로 렌더링
void Draw(ID3D12GraphicsCommandList*, const XMFLOAT2* pPos,
          const XMFLOAT2* pScale, float Z);

// 외부 텍스처+RECT를 직접 전달 (아틀라스 활용)
void DrawWithTex(ID3D12GraphicsCommandList*, const XMFLOAT2* pPos,
                 const XMFLOAT2* pScale, const RECT* pRect,
                 float Z, TEXTURE_HANDLE* pTexHandle);
```

`DrawWithTex`는 매 호출마다:
1. `CDescriptorPool`에서 렌더용 Descriptor Table 슬롯 할당
2. `CSimpleConstantBufferPool`(SPRITE 타입)에서 CB 할당
3. CB에 화면 해상도, 위치, 스케일, 텍스처 정보 기입
4. CPU → GPU Descriptor 복사 후 Draw 호출

---

## CConstantBufferManager (`ConstantBufferManager.h/.cpp`)

Ch12에서는 `CSimpleConstantBufferPool` 하나만 사용했으나,  
Ch13에서는 셰이더 타입별로 독립적인 풀이 필요해 매니저 클래스를 도입했다.

```cpp
class CConstantBufferManager
{
    CSimpleConstantBufferPool* m_ppConstantBufferPool[CONSTANT_BUFFER_TYPE_COUNT];
public:
    BOOL   Initialize(ID3D12Device*, DWORD dwMaxCBVNum);
    void   Reset();
    CSimpleConstantBufferPool* GetConstantBufferPool(CONSTANT_BUFFER_TYPE);
};
```

- `CONSTANT_BUFFER_TYPE_DEFAULT` → `sizeof(CONSTANT_BUFFER_DEFAULT)` 크기 풀
- `CONSTANT_BUFFER_TYPE_SPRITE`  → `sizeof(CONSTANT_BUFFER_SPRITE)` 크기 풀

---

## shSprite.hlsl (스프라이트 셰이더)

### 상수 버퍼
```hlsl
cbuffer CONSTANT_BUFFER_SPRITE : register(b0)
{
    float2 g_ScreenRes;      // 화면 해상도
    float2 g_Pos;            // 픽셀 좌표
    float2 g_Scale;          // 스케일
    float2 g_TexSize;        // 텍스처 크기
    float2 g_TexSampePos;    // 아틀라스 샘플 시작점
    float2 g_TexSampleSize;  // 아틀라스 샘플 크기
    float  g_Z;
    float  g_Alpha;
    ...
};
```

### 정점 셰이더 - 좌표 변환
```hlsl
PSInput VSMain(VSInput input)
{
    // 픽셀 좌표 → NDC 변환 (행렬 없이 상수버퍼만으로 처리)
    float2 scale  = (g_TexSize / g_ScreenRes) * g_Scale;
    float2 offset = (g_Pos / g_ScreenRes);
    float2 Pos    = input.Pos.xy * scale + offset;
    // NDC: X[-1,1] Y[-1,1], Y 반전 (스크린 Y↓ → NDC Y↑)
    result.position = float4(Pos.xy * float2(2, -2) + float2(-1, 1), g_Z, 1);

    // 아틀라스 UV 변환
    float2 tex_scale  = g_TexSampleSize / g_TexSize;
    float2 tex_offset = g_TexSampePos  / g_TexSize;
    result.TexCoord   = input.TexCoord * tex_scale + tex_offset;
}
```

**픽셀 → NDC 변환 공식 설명:**
$$NDC_x = \left(\frac{pos_x}{screenW}\right) \times 2 - 1$$
$$NDC_y = \left(\frac{pos_y}{screenH}\right) \times (-2) + 1$$

단위 Quad의 X∈[0,1], Y∈[0,1] 좌표를  
`scale`과 `offset`으로 텍스처 비율/위치에 맞춘 뒤 NDC로 변환한다.

---

## D3D12Renderer - Sprite API

```cpp
// 공용 스프라이트 생성 (텍스처를 Draw 시 외부 주입)
void* CreateSpriteObject();

// 전용 스프라이트 생성 (텍스처 + RECT 내장)
void* CreateSpriteObject(const WCHAR* wchTexFileName,
                         int PosX, int PosY, int Width, int Height);

void  DeleteSpriteObject(void* pSpriteObjHandle);

// 외부 텍스처를 주입해 아틀라스 특정 구간 렌더링
void  RenderSpriteWithTex(void* pSprObjHandle,
                          int iPosX, int iPosY,
                          float fScaleX, float fScaleY,
                          const RECT* pRect, float Z,
                          void* pTexHandle);

// 오브젝트에 내장된 텍스처+RECT로 렌더링
void  RenderSprite(void* pSprObjHandle,
                   int iPosX, int iPosY,
                   float fScaleX, float fScaleY,
                   float Z);
```

---

## main.cpp - 스프라이트 생성 및 렌더링

### 초기화
```cpp
// 공용 스프라이트 (텍스처를 외부에서 매번 주입)
g_pTexHandle0      = g_pRenderer->CreateTextureFromFile(L"tex_00.dds");
g_pSpriteObjCommon = g_pRenderer->CreateSpriteObject();

// 아틀라스 전용 스프라이트 (1024x1024 아틀라스를 4분할)
g_pSpriteObj0 = g_pRenderer->CreateSpriteObject(L"sprite_1024x1024.dds",   0,   0, 512,  512);
g_pSpriteObj1 = g_pRenderer->CreateSpriteObject(L"sprite_1024x1024.dds", 512,   0,1024,  512);
g_pSpriteObj2 = g_pRenderer->CreateSpriteObject(L"sprite_1024x1024.dds",   0, 512, 512, 1024);
g_pSpriteObj3 = g_pRenderer->CreateSpriteObject(L"sprite_1024x1024.dds", 512, 512,1024, 1024);
```

### 매 프레임 렌더링 (RunGame)
```cpp
// ── 3D ──────────────────────────────────────────────
g_pRenderer->RenderMeshObject(g_pMeshObj0, &g_matWorld0);
g_pRenderer->RenderMeshObject(g_pMeshObj0, &g_matWorld1);
g_pRenderer->RenderMeshObject(g_pMeshObj1, &g_matWorld2);

// ── 2D Sprite (공용, RenderSpriteWithTex) ────────────
RECT rect = {0, 0, 256, 256};
g_pRenderer->RenderSpriteWithTex(g_pSpriteObjCommon, 0,   0,   0.5f,0.5f, &rect, 0.0f, g_pTexHandle0);
rect = {256,0,512,256};
g_pRenderer->RenderSpriteWithTex(g_pSpriteObjCommon, 261, 0,   0.5f,0.5f, &rect, 0.0f, g_pTexHandle0);
// ...

// ── 2D Sprite (전용, RenderSprite) ──────────────────
g_pRenderer->RenderSprite(g_pSpriteObj0, 522,   0,   0.5f,0.5f, 1.0f);
g_pRenderer->RenderSprite(g_pSpriteObj1, 788,   0,   0.5f,0.5f, 1.0f);
g_pRenderer->RenderSprite(g_pSpriteObj2, 522, 266,   0.5f,0.5f, 1.0f);
g_pRenderer->RenderSprite(g_pSpriteObj3, 788, 266,   0.5f,0.5f, 0.0f); // Z=0 (전면)
```

---

## 렌더링 파이프라인 전체 흐름

```
BeginRender()
    └── CommandAllocator/List Reset, RTV/DSV Set, Clear

RenderMeshObject() × N   (3D, shBasicMesh.hlsl)
RenderSpriteWithTex() × N (2D, shSprite.hlsl)
RenderSprite() × N        (2D, shSprite.hlsl)

EndRender()
    └── ResourceBarrier (RTV→Present), Close, ExecuteCommandLists

Present()
    └── Fence → WaitForFence(nextFrame)
        → ConstantBufferManager.Reset()
        → DescriptorPool.Reset()
        → 컨텍스트 인덱스 교체
```

---

## 핵심 개념 정리

| 개념 | 설명 |
|------|------|
| **스프라이트 아틀라스** | 여러 이미지를 하나의 텍스처에 패킹하고, RECT로 UV 구간을 지정해 샘플링 |
| **픽셀 → NDC 변환** | 행렬 없이 CB의 화면 해상도 정보로 셰이더에서 직접 변환 |
| **공유 PSO/VB/IB** | 인스턴스마다 동일한 파이프라인을 사용하므로 static으로 공유, RefCount로 수명 관리 |
| **CB 타입별 관리** | `CConstantBufferManager`가 DEFAULT/SPRITE CB 풀을 타입 인덱스로 구분해 반환 |
| **Draw 독립성** | 매 Draw 호출마다 DescriptorPool + CBPool에서 독립 슬롯 할당 → 다중 Draw 무결성 보장 |
| **Depth Z** | 스프라이트도 Depth Buffer에 기록되어 3D 오브젝트와 depth 비교 가능 (`LESS_EQUAL`) |
