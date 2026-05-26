# Chapter 13 vs Chapter 12 비교

## 개요

| 항목 | Ch12 (MultiMaterial) | Ch13 (Sprite) |
|------|----------------------|---------------|
| 주요 기능 | 멀티 머티리얼 3D 메시 렌더링 | 2D 스프라이트 렌더링 추가 |
| 새 클래스 | - | `CSpriteObject`, `CConstantBufferManager` |
| 새 셰이더 | - | `shSprite.hlsl` |
| CB 관리 방식 | `CSimpleConstantBufferPool` (단일) | `CConstantBufferManager` (타입별 복수 풀) |

---

## 신규 추가 파일 (Ch13에만 존재)

| 파일 | 역할 |
|------|------|
| `SpriteObject.h/.cpp` | 2D 스프라이트 렌더링 클래스 |
| `ConstantBufferManager.h/.cpp` | CB 타입(DEFAULT/SPRITE)별 풀 관리 |
| `Renderer_typedef.h` | `SWAP_CHAIN_FRAME_COUNT`, `MAX_PENDING_FRAME_COUNT` 상수 분리 |
| `Shaders/shSprite.hlsl` | 2D 스프라이트 전용 VS/PS |
| `sprite_1024x1024.dds` | 스프라이트 아틀라스 텍스처 |

---

## typedef.h 변경 사항

### Ch12
- `CONSTANT_BUFFER_DEFAULT` 구조체만 존재
- `TEXTURE_HANDLE` 구조체

### Ch13 추가
```cpp
// 스프라이트 전용 상수 버퍼 구조체
struct CONSTANT_BUFFER_SPRITE {
    XMFLOAT2 ScreenRes;       // 화면 해상도
    XMFLOAT2 Pos;             // 픽셀 단위 위치
    XMFLOAT2 Scale;           // 스케일
    XMFLOAT2 TexSize;         // 텍스처 전체 크기
    XMFLOAT2 TexSampePos;     // 아틀라스 내 샘플 시작 위치
    XMFLOAT2 TexSampleSize;   // 아틀라스 내 샘플 크기
    float Z; float Alpha;
    float Reserved0; float Reserved1;
};

// 상수 버퍼 타입 열거형
enum CONSTANT_BUFFER_TYPE {
    CONSTANT_BUFFER_TYPE_DEFAULT,
    CONSTANT_BUFFER_TYPE_SPRITE,
    CONSTANT_BUFFER_TYPE_COUNT
};

struct CONSTANT_BUFFER_PROPERTY {
    CONSTANT_BUFFER_TYPE type;
    UINT Size;
};
```

---

## D3D12Renderer.h 변경 사항

### 멤버 변수

| Ch12 | Ch13 |
|------|------|
| `CSimpleConstantBufferPool* m_ppConstantBufferPool[MAX_PENDING_FRAME_COUNT]` | `CConstantBufferManager* m_ppConstBufferManager[MAX_PENDING_FRAME_COUNT]` |
| (상수 헤더 직접 정의) | `#include "Renderer_typedef.h"` 로 분리 |

### 추가된 Public 메서드

```cpp
// Ch13 신규
void*  CreateSpriteObject();
void*  CreateSpriteObject(const WCHAR* wchTexFileName, int PosX, int PosY, int Width, int Height);
void   DeleteSpriteObject(void* pSpriteObjHandle);
void   RenderSpriteWithTex(void* pSprObjHandle, int iPosX, int iPosY, float fScaleX, float fScaleY,
                           const RECT* pRect, float Z, void* pTexHandle);
void   RenderSprite(void* pSprObjHandle, int iPosX, int iPosY, float fScaleX, float fScaleY, float Z);
DWORD  INL_GetScreenWidth() const;
DWORD  INL_GetScreenHeigt() const;
CSimpleConstantBufferPool* GetConstantBufferPool(CONSTANT_BUFFER_TYPE type);  // 타입 인자 추가
```

### 제거된 메서드

```cpp
// Ch12에만 있었음
CSimpleConstantBufferPool* INL_GetConstantBufferPool();
```

---

## D3D12Renderer.cpp 변경 사항

### 상수 버퍼 풀 초기화

**Ch12:**
```cpp
m_ppConstantBufferPool[i] = new CSimpleConstantBufferPool;
m_ppConstantBufferPool[i]->Initialize(m_pD3DDevice, sizeof(CONSTANT_BUFFER_DEFAULT), MAX_DRAW_COUNT_PER_FRAME);
```

**Ch13:**
```cpp
m_ppConstBufferManager[i] = new CConstantBufferManager;
m_ppConstBufferManager[i]->Initialize(m_pD3DDevice, MAX_DRAW_COUNT_PER_FRAME);
// CConstantBufferManager 내부에서 DEFAULT, SPRITE 두 타입 풀을 동시에 관리
```

### 신규 Sprite API 구현 (Ch13)
- `RenderSpriteWithTex()` : 외부에서 텍스처 핸들을 받아 특정 RECT 영역만 샘플링
- `RenderSprite()` : 오브젝트 초기화 시 등록한 텍스처/RECT로 렌더링
- `CreateSpriteObject()` / `DeleteSpriteObject()` : 스프라이트 오브젝트 생성·삭제

---

## CSpriteObject 클래스 (Ch13 신규)

### 정적(공유) 리소스 패턴
```cpp
static ID3D12RootSignature* m_pRootSignature;  // 모든 인스턴스 공유
static ID3D12PipelineState* m_pPipelineState;  // 모든 인스턴스 공유
static ID3D12Resource*      m_pVertexBuffer;   // 모든 인스턴스 공유
static ID3D12Resource*      m_pIndexBuffer;    // 모든 인스턴스 공유
static DWORD                m_dwInitRefCount;  // 레퍼런스 카운팅으로 공유 리소스 수명 관리
```

### 두 가지 Draw 방식
| 메서드 | 설명 |
|--------|------|
| `Draw()` | 초기화 시 등록한 텍스처 + RECT로 렌더링 |
| `DrawWithTex()` | 외부에서 텍스처 핸들과 RECT를 직접 전달해 렌더링 (아틀라스 활용) |

### 루트 시그니처 구성
```
Descriptor Table [0] :
  range[0] : CBV  b0  (CONSTANT_BUFFER_SPRITE)
  range[1] : SRV  t0  (Diffuse Texture)
Static Sampler s0 : POINT filter
```

---

## shSprite.hlsl (Ch13 신규)

### 핵심 좌표 변환 로직
```hlsl
// 픽셀 좌표 → 정규화 좌표(NDC) 변환
float2 scale  = (g_TexSize / g_ScreenRes) * g_Scale;
float2 offset = (g_Pos / g_ScreenRes);
float2 Pos    = input.Pos.xy * scale + offset;
result.position = float4(Pos.xy * float2(2, -2) + float2(-1, 1), g_Z, 1);

// 텍스처 아틀라스 UV 변환
float2 tex_scale  = g_TexSampleSize / g_TexSize;
float2 tex_offset = g_TexSampePos  / g_TexSize;
result.TexCoord = input.TexCoord * tex_scale + tex_offset;
```
- 행렬 변환 없이 상수 버퍼의 스크린 해상도 정보만으로 NDC 변환 수행
- 아틀라스 텍스처의 특정 RECT 구간만 UV로 샘플링 가능

---

## main.cpp 변경 사항

### Ch12 전역 오브젝트
```cpp
void* g_pMeshObj0;
void* g_pMeshObj1;
```

### Ch13 전역 오브젝트 (추가)
```cpp
void* g_pSpriteObjCommon;   // 텍스처를 외부에서 주입하는 공용 스프라이트
void* g_pSpriteObj0 ~ 3;    // 아틀라스의 각 사분면을 담당하는 전용 스프라이트
void* g_pTexHandle0;         // 텍스처 핸들
```

### RunGame() 렌더링 흐름
**Ch12:** 3D 메시 렌더만 수행  
**Ch13:** 3D 메시 렌더 후 2D 스프라이트 렌더 추가
```cpp
// 3D
g_pRenderer->RenderMeshObject(g_pMeshObj0, &g_matWorld0);
g_pRenderer->RenderMeshObject(g_pMeshObj0, &g_matWorld1);
g_pRenderer->RenderMeshObject(g_pMeshObj1, &g_matWorld2);

// 2D (Ch13 신규)
g_pRenderer->RenderSpriteWithTex(g_pSpriteObjCommon, 0, 0, 0.5f, 0.5f, &rect, 0.0f, g_pTexHandle0);
g_pRenderer->RenderSprite(g_pSpriteObj0, 512+10, 0, 0.5f, 0.5f, 1.0f);
// ...
```

---

## 아키텍처 변화 요약

```
Ch12                              Ch13
─────────────────────────────     ──────────────────────────────────
CDescriptorPool (per frame)  →    CDescriptorPool (per frame)
CSimpleConstantBufferPool    →    CConstantBufferManager
  (DEFAULT CB만 관리)                ├── CSimpleConstantBufferPool (DEFAULT)
                                     └── CSimpleConstantBufferPool (SPRITE)

렌더 오브젝트:                    렌더 오브젝트:
  CBasicMeshObject (3D)      →      CBasicMeshObject (3D) ──────── 유지
                                     CSpriteObject   (2D) ──────── 신규
```
