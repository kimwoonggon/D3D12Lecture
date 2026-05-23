# 코드 비교: 07_DynamicDescriptorTable_CBV vs 08_DynamicDescriptorTable_CBV_SRV

## 개요

| 항목 | 07_DynamicDescriptorTable_CBV | 08_DynamicDescriptorTable_CBV_SRV |
|------|-------------------------------|-----------------------------------|
| 핵심 목표 | Dynamic Descriptor Pool + CBV만 사용 | CBV + **SRV(텍스처)** 동시 바인딩 |
| Descriptor Table 구성 | `[CBV]` (1 slot per draw) | `[CBV][SRV]` (2 slots per draw) |
| 텍스처 | 없음 | **CPU에서 동적 생성** (타일 패턴) |
| SRV 관리 방식 | 없음 | **`CSingleDescriptorAllocator`** 로 영구 할당/해제 |
| `Draw()` 시그니처 | `Draw(pCommandList, pPos)` | `Draw(pCommandList, pPos, srv)` |
| `RenderMeshObject()` 시그니처 | `(pHandle, x, y)` | `(pHandle, x, y, pTexHandle)` |
| 텍스처 핸들 | 없음 | `TEXTURE_HANDLE { pTexResource, srv }` |
| Pool 총 슬롯 수 | `256 × 1 = 256` | `256 × 2 = 512` |
| 신규 파일 | 없음 | `SingleDescriptorAllocator.h/.cpp` |

---

## 신규 추가된 파일

### `SingleDescriptorAllocator.h` / `SingleDescriptorAllocator.cpp` ← **신규**

```
08_DynamicDescriptorTable_CBV_SRV/
  SingleDescriptorAllocator.h      ← 신규
  SingleDescriptorAllocator.cpp    ← 신규
```

#### 배경: 왜 새 Allocator가 필요한가

07에서 `CDescriptorPool`은 **프레임 단위**로 Reset되는 임시 할당자다.  
Draw call 도중 슬롯을 소비하고, 프레임 끝 `Reset()`으로 전부 재사용한다.

텍스처 SRV는 성격이 전혀 다르다.  
**텍스처 생성 시 1회 할당 → 앱 종료 또는 명시적 해제 시까지 유지**.  
프레임 단위 Reset이 일어나면 안 된다.

따라서 **영구 개별 할당/해제가 가능한 별도 Allocator**가 필요하고,  
그것이 `CSingleDescriptorAllocator`다.

```
CDescriptorPool               CSingleDescriptorAllocator
──────────────────────────    ──────────────────────────
per-frame 임시 할당           영구 개별 할당/해제
Draw마다 소비                 생성 시 1회 할당
Reset()으로 일괄 재사용       Free()로 개별 반납
선형 할당자                   인덱스 기반 재사용 할당자 (CIndexCreator)
shader-visible                CPU-only (FLAG_NONE)
```

---

#### 클래스: `CSingleDescriptorAllocator`

> 파일: `SingleDescriptorAllocator.h`

**멤버 변수**

| 멤버 | 타입 | 설명 |
|------|------|------|
| `m_pD3DDevice` | `ID3D12Device*` | D3D12 디바이스 참조 |
| `m_pHeap` | `ID3D12DescriptorHeap*` | CPU-only Descriptor Heap (`FLAG_NONE`) |
| `m_IndexCreator` | `CIndexCreator` | 빈 인덱스 관리 (할당/해제 추적) |
| `m_DescriptorSize` | `UINT` | 디스크립터 한 개의 바이트 크기 |

---

#### `BOOL Initialize(ID3D12Device* pDevice, DWORD dwMaxCount, D3D12_DESCRIPTOR_HEAP_FLAGS Flags)`

> 파일: `SingleDescriptorAllocator.cpp`, line 12

| 인자 | 값 (렌더러에서 전달) | 설명 |
|------|----------------------|------|
| `pDevice` | `m_pD3DDevice` | D3D12 디바이스 |
| `dwMaxCount` | `MAX_DESCRIPTOR_COUNT = 4096` | 최대 디스크립터 슬롯 수 |
| `Flags` | `D3D12_DESCRIPTOR_HEAP_FLAG_NONE` | CPU-only heap (GPU 접근 불가) |

- 지정된 크기의 CPU-only heap 생성
- `CIndexCreator.Initialize(dwMaxCount)`로 빈 인덱스 풀 초기화
- `GetDescriptorHandleIncrementSize`로 `m_DescriptorSize` 캐시

---

#### `BOOL AllocDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUHandle)`

> 파일: `SingleDescriptorAllocator.cpp`, line 43

```cpp
DWORD dwIndex = m_IndexCreator.Alloc();  // 빈 슬롯 인덱스 획득
CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
    m_pHeap->GetCPUDescriptorHandleForHeapStart(), dwIndex, m_DescriptorSize);
*pOutCPUHandle = handle;
```

- `CIndexCreator`에서 사용 가능한 인덱스 하나를 꺼낸다
- 해당 인덱스 위치의 CPU 핸들을 반환
- 슬롯 고갈 시 `FALSE` 반환

---

#### `void FreeDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle)`

> 파일: `SingleDescriptorAllocator.cpp`, line 70

```cpp
DWORD dwIndex = (DescriptorHandle.ptr - base.ptr) / m_DescriptorSize;
m_IndexCreator.Free(dwIndex);
```

- CPU 핸들 주소에서 인덱스를 역산하여 `CIndexCreator`에 반납
- 반납된 슬롯은 이후 `AllocDescriptorHandle`에서 재사용 가능

---

#### `CDescriptorPool` vs `CSingleDescriptorAllocator` 비교

| | `CDescriptorPool` | `CSingleDescriptorAllocator` |
|---|---|---|
| **Heap Flags** | `FLAG_SHADER_VISIBLE` | `FLAG_NONE` (CPU-only) |
| **수명** | 프레임 단위 (Reset 후 재사용) | 앱 수명과 동일 (Free 시까지) |
| **할당 방식** | 선형 (카운터만 전진) | 인덱스 풀 (`CIndexCreator`) |
| **해제 방식** | `Reset()` (일괄) | `FreeDescriptorHandle()` (개별) |
| **GPU 접근** | 가능 | 불가 |
| **용도** | Draw 시 shader-visible table | 텍스처 SRV 영구 보관 |
| **최대 수** | `256 × DESCRIPTOR_COUNT_FOR_DRAW` | `4096` |

---

## 변경된 파일

### `typedef.h`

#### 추가된 구조체: `TEXTURE_HANDLE`

```cpp
// 07: 없음

// 08: 추가
struct TEXTURE_HANDLE
{
    ID3D12Resource*             pTexResource;   // 텍스처 GPU 리소스
    D3D12_CPU_DESCRIPTOR_HANDLE srv;            // SRV 핸들 (SingleDescriptorAllocator에서 할당)
};
```

> 파일: `typedef.h`

렌더러가 외부(main.cpp)에 텍스처를 `void*`로 반환할 때 사용하는 핸들 구조체.  
`pTexResource`는 GPU 텍스처 Resource이고, `srv`는 `CSingleDescriptorAllocator`에서 받은 CPU 핸들이다.  
`RenderMeshObject()`에 `void* pTexHandle`로 전달되며, 내부에서 `TEXTURE_HANDLE*`로 캐스팅된다.

---

### `D3D12Renderer.h`

#### 추가된 forward declaration

```cpp
// 07: 없음
// 08: 추가
class CSingleDescriptorAllocator;
```

> 파일: `D3D12Renderer.h`, line 6

#### 추가된 상수

```cpp
// 07: 없음
// 08: 추가
static const UINT MAX_DESCRIPTOR_COUNT = 4096;
```

> 파일: `D3D12Renderer.h`, line 13

`CSingleDescriptorAllocator`의 최대 슬롯 수.  
`MAX_DRAW_COUNT_PER_FRAME(256)`과 별개로 텍스처 등 영구 리소스를 위한 상한값이다.

#### 추가된 멤버 변수

```cpp
// 07: 없음
// 08: 추가
CSingleDescriptorAllocator* m_pSingleDescriptorAllocator = nullptr;
```

> 파일: `D3D12Renderer.h`, line 17

#### 변경된 public 메서드

```cpp
// 07
void RenderMeshObject(void* pMeshObjHandle, float x_offset, float y_offset);
// 07에는 텍스처 관련 메서드 없음

// 08
void* CreateTiledTexture(UINT TexWidth, UINT TexHeight, DWORD r, DWORD g, DWORD b);   // ← 추가
void  DeleteTexture(void* pTexHandle);                                                  // ← 추가
void  RenderMeshObject(void* pMeshObjHandle, float x_offset, float y_offset, void* pTexHandle); // ← 시그니처 변경
```

> 파일: `D3D12Renderer.h`

#### 추가된 public inline accessor

```cpp
// 08에서 추가
CSingleDescriptorAllocator* INL_GetSingleDescriptorAllocator() { return m_pSingleDescriptorAllocator; }
```

> 파일: `D3D12Renderer.h`, line 82

`CBasicMeshObject::CreateMesh()`에서 `CSingleDescriptorAllocator`에 접근하기 위해 사용된다.  
*(실제로 08에서 `CreateMesh()`는 SDA를 획득만 하고 직접 사용하지 않으므로, 향후 확장을 위한 준비 코드다.)*

---

### `D3D12Renderer.cpp`

#### 추가된 include

```cpp
// 07: 없음
// 08: 추가
#include "SingleDescriptorAllocator.h"
```

> 파일: `D3D12Renderer.cpp`, line 11

---

#### `BOOL Initialize(...)` — 추가된 초기화 코드

**변경점: `CSingleDescriptorAllocator` 생성**

```cpp
// 07: 없음

// 08: 추가 (CBPool, DescriptorPool 생성 이후)
m_pSingleDescriptorAllocator = new CSingleDescriptorAllocator;
m_pSingleDescriptorAllocator->Initialize(
    m_pD3DDevice,
    MAX_DESCRIPTOR_COUNT,                   // 4096
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE         // CPU-only
);
```

| 인자 | 값 | 설명 |
|------|-----|------|
| `pDevice` | `m_pD3DDevice` | |
| `dwMaxCount` | `4096` | 텍스처 SRV 등 영구 리소스 용량 |
| `Flags` | `D3D12_DESCRIPTOR_HEAP_FLAG_NONE` | GPU 접근 불필요, CPU-only |

**`DescriptorPool` 슬롯 수 변화 (암시적 변경)**

```cpp
// 07: 256 × 1 = 256 슬롯
m_pDescriptorPool->Initialize(m_pD3DDevice, MAX_DRAW_COUNT_PER_FRAME * CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW);
// = 256 × 1 = 256

// 08: 256 × 2 = 512 슬롯 (코드는 동일, DESCRIPTOR_COUNT_FOR_DRAW 값이 달라짐)
m_pDescriptorPool->Initialize(m_pD3DDevice, MAX_DRAW_COUNT_PER_FRAME * CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW);
// = 256 × 2 = 512
```

`Initialize()` 코드 자체는 동일하지만, `CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW`가  
`1`(07) → `2`(08)로 바뀌었기 때문에 Pool 크기가 자동으로 2배가 된다.

---

#### `void RenderMeshObject(...)` — 시그니처 및 구현 변경

```cpp
// 07
void CD3D12Renderer::RenderMeshObject(void* pMeshObjHandle, float x_offset, float y_offset)
{
    CBasicMeshObject* pMeshObj = (CBasicMeshObject*)pMeshObjHandle;
    XMFLOAT2 Pos = { x_offset, y_offset };
    pMeshObj->Draw(m_pCommandList, &Pos);
}

// 08
void CD3D12Renderer::RenderMeshObject(void* pMeshObjHandle, float x_offset, float y_offset, void* pTexHandle)
{
    D3D12_CPU_DESCRIPTOR_HANDLE srv = {};
    CBasicMeshObject* pMeshObj = (CBasicMeshObject*)pMeshObjHandle;
    if (pTexHandle)
    {
        srv = ((TEXTURE_HANDLE*)pTexHandle)->srv;   // ← TEXTURE_HANDLE에서 SRV 핸들 추출
    }
    XMFLOAT2 Pos = { x_offset, y_offset };
    pMeshObj->Draw(m_pCommandList, &Pos, srv);       // ← srv 전달 추가
}
```

> 파일: `D3D12Renderer.cpp`

`pTexHandle`이 `nullptr`이면 `srv.ptr == 0`으로 전달되고, `Draw()` 내부에서 `srv.ptr` 체크로 분기한다.  
텍스처 없이 Draw가 가능한 구조를 유지하고 있다.

---

#### `void* CreateTiledTexture(...)` ← **신규 메서드**

> 파일: `D3D12Renderer.cpp`

CPU에서 체커보드 패턴 픽셀 데이터를 생성하고, GPU 텍스처 Resource와 SRV를 만들어 `TEXTURE_HANDLE`로 반환하는 메서드.

**전체 흐름**

```
1. CPU 이미지 생성 (malloc → 체커보드 픽셀 채우기)
2. CD3D12ResourceManager::CreateTexture()
   └─ GPU Default Heap에 Texture2D Resource 생성
   └─ Upload Heap 경유 텍셀 데이터 전송 (GPU Upload 완료까지 Wait)
3. CSingleDescriptorAllocator::AllocDescriptorHandle(&srv)
   └─ CPU-only heap에서 빈 슬롯 획득
4. CreateShaderResourceView(pTexResource, &SRVDesc, srv)
   └─ 슬롯에 SRV 기록
5. TEXTURE_HANDLE { pTexResource, srv } 생성 → void* 반환
6. free(pImage) ← CPU 임시 이미지 해제
```

**시그니처**

```cpp
void* CD3D12Renderer::CreateTiledTexture(UINT TexWidth, UINT TexHeight, DWORD r, DWORD g, DWORD b)
```

| 인자 | 설명 |
|------|------|
| `TexWidth`, `TexHeight` | 텍스처 해상도 (픽셀) |
| `r`, `g`, `b` | 밝은 타일의 RGB 색상 (어두운 타일은 항상 검정) |

**체커보드 생성 로직**

```cpp
BOOL bFirstColorIsWhite = TRUE;
for (UINT y = 0; y < TexHeight; y++) {
    for (UINT x = 0; x < TexWidth; x++) {
        RGBA* pDest = ...;
        if ((bFirstColorIsWhite + x) % 2)
            // 밝은 타일 → (r, g, b, 255)
        else
            // 어두운 타일 → (0, 0, 0, 255)
    }
    bFirstColorIsWhite++;
    bFirstColorIsWhite %= 2;  // 행마다 시작 색상 반전 → 체커 패턴
}
```

**SRV Desc**

```cpp
D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
SRVDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
SRVDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
SRVDesc.Texture2D.MipLevels     = 1;
```

---

#### `void DeleteTexture(void* pHandle)` ← **신규 메서드**

> 파일: `D3D12Renderer.cpp`

```cpp
void CD3D12Renderer::DeleteTexture(void* pHandle)
{
    TEXTURE_HANDLE* pTexHandle = (TEXTURE_HANDLE*)pHandle;

    pTexHandle->pTexResource->Release();                                // GPU Resource 해제
    m_pSingleDescriptorAllocator->FreeDescriptorHandle(pTexHandle->srv); // SRV 슬롯 반납
    delete pTexHandle;                                                   // 핸들 구조체 해제
}
```

`CreateTiledTexture`의 역순으로 정확히 세 가지를 해제한다.

---

#### `void Cleanup()` — 추가된 해제 코드

```cpp
// 08에서 추가
if (m_pSingleDescriptorAllocator)
{
    delete m_pSingleDescriptorAllocator;
    m_pSingleDescriptorAllocator = nullptr;
}
```

> 파일: `D3D12Renderer.cpp`

---

### `BasicMeshObject.h`

#### `DESCRIPTOR_COUNT_FOR_DRAW` 값 변경

| 항목 | 07 | 08 |
|------|----|----|
| 값 | `1` | `2` |
| 의미 | CBV 1개 | CBV 1개 + SRV 1개 |

> 파일: `BasicMeshObject.h`, line 14

이 값 하나가 바뀜으로써 `CDescriptorPool` 총 슬롯 수가 자동으로 256 → 512로 증가한다.

#### `BASIC_MESH_DESCRIPTOR_INDEX` 열거형 — SRV 인덱스 추가

```cpp
// 07
enum BASIC_MESH_DESCRIPTOR_INDEX
{
    BASIC_MESH_DESCRIPTOR_INDEX_CBV = 0
};

// 08
enum BASIC_MESH_DESCRIPTOR_INDEX
{
    BASIC_MESH_DESCRIPTOR_INDEX_CBV = 0,
    BASIC_MESH_DESCRIPTOR_INDEX_TEX = 1   // ← 추가
};
```

> 파일: `BasicMeshObject.h`, line 3-7

Draw 시 `cpuDescriptorTable`에서 SRV 슬롯의 오프셋을 계산할 때 사용된다.

#### `Draw()` 시그니처 변경

```cpp
// 07
void Draw(ID3D12GraphicsCommandList* pCommandList, const XMFLOAT2* pPos);

// 08
void Draw(ID3D12GraphicsCommandList* pCommandList, const XMFLOAT2* pPos, D3D12_CPU_DESCRIPTOR_HANDLE srv);
```

> 파일: `BasicMeshObject.h`, line 34

`srv`는 `CSingleDescriptorAllocator`에서 받은 CPU 핸들.  
`srv.ptr == 0`이면 텍스처 복사 단계를 건너뛰는 NULL-safe 설계다.

---

### `BasicMeshObject.cpp`

#### 추가된 include

```cpp
// 07: 없음
// 08: 추가
#include "SingleDescriptorAllocator.h"
```

> 파일: `BasicMeshObject.cpp`, line 8

---

#### `BOOL InitRootSignature()` — Descriptor Range 확장

```cpp
// 07: CBV 1개
CD3DX12_DESCRIPTOR_RANGE ranges[1] = {};
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // b0

// 08: CBV + SRV
CD3DX12_DESCRIPTOR_RANGE ranges[2] = {};
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // b0
ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // t0  ← 추가
```

> 파일: `BasicMeshObject.cpp`

Root Parameter 개수(1)는 동일하지만, 단일 Descriptor Table이 포함하는 Range가 1개 → 2개로 늘었다.  
GPU 입장에서는 "slot[0]의 table에는 CBV 1개 + SRV 1개가 연속으로 있다"고 해석한다.

```
[Descriptor Table at slot[0]]
  ┌──────────────┬──────────────┐
  │  ranges[0]   │  ranges[1]   │
  │  CBV (b0)    │  SRV (t0)    │
  └──────────────┴──────────────┘
  cpuDescriptorTable+0    cpuDescriptorTable+1
```

---

#### `BOOL CreateMesh()` — 미세 변경 (SDA 포인터 획득 추가)

```cpp
// 07
BOOL CBasicMeshObject::CreateMesh()
{
    ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();
    CD3D12ResourceManager* pResourceManager = m_pRenderer->INL_GetResourceManager();
    // vertex color: RGB 삼원색
    BasicVertex Vertices[] = {
        { ... , { 1.0f, 0.0f, 0.0f, 1.0f }, ... },   // 빨강
        { ... , { 0.0f, 1.0f, 0.0f, 1.0f }, ... },   // 초록
        { ... , { 0.0f, 0.0f, 1.0f, 1.0f }, ... }    // 파랑
    };
    ...
}

// 08
BOOL CBasicMeshObject::CreateMesh()
{
    ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();
    UINT srvDescriptorSize = m_pRenderer->INL_GetSrvDescriptorSize();              // ← 추가
    CD3D12ResourceManager* pResourceManager = m_pRenderer->INL_GetResourceManager();
    CSingleDescriptorAllocator* pSingleDescriptorAllocator =
        m_pRenderer->INL_GetSingleDescriptorAllocator();                           // ← 추가 (미사용)
    // vertex color: 흰색 (텍스처 색상이 그대로 반영되도록)
    BasicVertex Vertices[] = {
        { ... , { 1.0f, 1.0f, 1.0f, 1.0f }, ... },   // 흰색
        { ... , { 1.0f, 1.0f, 1.0f, 1.0f }, ... },   // 흰색
        { ... , { 1.0f, 1.0f, 1.0f, 1.0f }, ... }    // 흰색
    };
    ...
}
```

> 파일: `BasicMeshObject.cpp`

**vertex color가 흰색으로 바뀐 이유:**  
07에서는 `PSMain`이 `return input.color`였기 때문에 버텍스 색상이 삼각형 색상을 결정했다.  
08에서는 `PSMain`이 `return texColor * input.color`이므로, 버텍스 색상이 `(1,1,1,1)`이어야  
텍스처 색상이 곱셈 결과에서 그대로 유지된다. 색상을 섞지 않겠다는 의도다.

---

#### `void Draw(...)` — SRV 복사 단계 추가

이것이 이번 예제의 핵심 변경점이다.

**07 방식 (CBV만 복사)**

```cpp
// 07 Draw() 핵심 부분
CD3DX12_CPU_DESCRIPTOR_HANDLE cbvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_CBV, srvDescriptorSize);
pD3DDeivce->CopyDescriptorsSimple(1, cbvDest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
```

**08 방식 (CBV + SRV 복사)**

```cpp
// 08 Draw() 핵심 부분
// CBV 복사 (07과 동일)
CD3DX12_CPU_DESCRIPTOR_HANDLE cbvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_CBV, srvDescriptorSize);
pD3DDeivce->CopyDescriptorsSimple(1, cbvDest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

// SRV 복사 (새로 추가)
if (srv.ptr)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_TEX, srvDescriptorSize);
    pD3DDeivce->CopyDescriptorsSimple(1, srvDest, srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
```

> 파일: `BasicMeshObject.cpp`

**`CopyDescriptorsSimple` 2회 호출 비교**

| 호출 | 원본 (`Src`) | 목적지 (`Dest`) |
|------|--------------|-----------------|
| CBV 복사 | `pCB->CBVHandle` (CBPool CPU-only heap) | `cpuDescriptorTable + 0` (DescriptorPool shader-visible) |
| SRV 복사 | `srv` (SingleDescriptorAllocator CPU-only heap) | `cpuDescriptorTable + 1` (DescriptorPool shader-visible) |

```
[DescriptorPool shader-visible heap — 이번 Draw의 슬롯]
  cpuDescriptorTable
  ┌──────────────────────┬──────────────────────┐
  │  슬롯[N+0] = CBV    │  슬롯[N+1] = SRV    │
  │  (b0 레지스터)       │  (t0 레지스터)       │
  └──────────────────────┴──────────────────────┘
          ↑                        ↑
    CopyDescriptorsSimple    CopyDescriptorsSimple
    from CBPool                from SingleDescAllocator
```

---

### `Shaders/shaders.hlsl`

#### 07 → 08 변경

**07: 텍스처 없음**

```hlsl
// 선언부 — 텍스처/샘플러 없음
cbuffer CONSTANT_BUFFER_DEFAULT : register(b0) { float4 g_Offset; };

// PSMain — vertex color만 반환
float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
```

**08: Texture2D + Sampler 추가**

```hlsl
// 선언부 — 추가됨
Texture2D    texDiffuse    : register(t0);
SamplerState samplerDiffuse : register(s0);

cbuffer CONSTANT_BUFFER_DEFAULT : register(b0) { float4 g_Offset; };

// PSMain — 텍스처 샘플링 후 vertex color와 곱
float4 PSMain(PSInput input) : SV_TARGET
{
    float4 texColor = texDiffuse.Sample(samplerDiffuse, input.TexCoord);
    return texColor * input.color;
}
```

> 파일: `Shaders/shaders.hlsl`

| 변경 | 07 | 08 |
|------|----|----|
| `Texture2D texDiffuse : register(t0)` | 없음 | 추가 |
| `SamplerState samplerDiffuse : register(s0)` | 없음 | 추가 |
| `PSMain` 반환값 | `input.color` | `texDiffuse.Sample(...) * input.color` |

Root Signature의 `ranges[1]`에 SRV Range가 추가됨에 따라 HLSL에서 `t0` 레지스터에 텍스처가 바인딩된다.  
`Static Sampler`는 Root Signature 생성 시 이미 s0에 등록되어 있으므로 HLSL에서 `register(s0)`으로 참조만 하면 된다.

---

### `main.cpp`

#### 전역 변수 변경

```cpp
// 07: 텍스처 핸들 없음
void* g_pMeshObj = nullptr;

// 08: 텍스처 핸들 2개 추가
void* g_pMeshObj    = nullptr;
void* g_pTexHandle0 = nullptr;   // ← 추가: 16×16 파란색 계열 타일
void* g_pTexHandle1 = nullptr;   // ← 추가: 32×32 초록색 계열 타일
```

> 파일: `main.cpp`

#### `wWinMain()` — 텍스처 생성/해제 추가

**생성 (07 → 08)**

```cpp
// 07
g_pMeshObj = g_pRenderer->CreateBasicMeshObject();

// 08
g_pMeshObj = g_pRenderer->CreateBasicMeshObject();
g_pTexHandle0 = g_pRenderer->CreateTiledTexture(16, 16, 192, 128, 255); // ← 추가: 파란 계열
g_pTexHandle1 = g_pRenderer->CreateTiledTexture(32, 32, 128, 255, 192); // ← 추가: 초록 계열
```

**해제 (07 → 08)**

```cpp
// 08에서 추가
if (g_pTexHandle0) { g_pRenderer->DeleteTexture(g_pTexHandle0); g_pTexHandle0 = nullptr; }
if (g_pTexHandle1) { g_pRenderer->DeleteTexture(g_pTexHandle1); g_pTexHandle1 = nullptr; }
```

#### `RunGame()` — RenderMeshObject 시그니처 변경

```cpp
// 07
g_pRenderer->RenderMeshObject(g_pMeshObj, g_fOffsetX, 0.0f);
g_pRenderer->RenderMeshObject(g_pMeshObj, 0.0f, g_fOffsetY);

// 08
g_pRenderer->RenderMeshObject(g_pMeshObj, g_fOffsetX, 0.0f, g_pTexHandle0); // ← tex0 전달
g_pRenderer->RenderMeshObject(g_pMeshObj, 0.0f, g_fOffsetY, g_pTexHandle1); // ← tex1 전달
```

같은 `g_pMeshObj`를 두 번 Draw하되, 텍스처는 각자 다른 것을 사용한다.  
Draw#1은 16×16 파란 타일, Draw#2는 32×32 초록 타일이 입혀진다.

---

## 변경 없는 파일

| 파일 | 이유 |
|------|------|
| `DescriptorPool.h` / `.cpp` | 선형 할당자 구조 동일. 슬롯 수는 외부에서 결정됨 |
| `SimpleConstantBufferPool.h` / `.cpp` | CBV 풀링 구조 동일 |
| `D3D12ResourceManager.h` / `.cpp` | `CreateTexture()` 이미 07에서 존재. 변경 없음 |
| `pch.h` / `pch.cpp` | 동일 |

---

## 아키텍처 변화 요약

```
[07] per-frame pool + CBV only
──────────────────────────────────────────────────
CD3D12Renderer
  ├─ CDescriptorPool       (shader-visible, 256슬롯)
  │    [CBV_0][CBV_1]...
  └─ CSimpleConstantBufferPool (Upload Heap, 256슬롯)
       [CB_0][CB_1]...

Draw 시 Descriptor Table 레이아웃:
  [슬롯0: CBV] ← 1개

──────────────────────────────────────────────────

[08] per-frame pool + CBV + SRV / 영구 SRV 관리
──────────────────────────────────────────────────
CD3D12Renderer
  ├─ CDescriptorPool           (shader-visible, 512슬롯)
  │    [CBV_0][SRV_0][CBV_1][SRV_1]...
  ├─ CSimpleConstantBufferPool (Upload Heap, 256슬롯)
  │    [CB_0][CB_1]...
  └─ CSingleDescriptorAllocator (CPU-only, 4096슬롯)  ← 신규
       [SRV_tex0][SRV_tex1][ ... ]  (영구, Free까지 유지)

Draw 시 Descriptor Table 레이아웃:
  [슬롯0: CBV][슬롯1: SRV] ← 2개

텍스처 수명:
  CreateTiledTexture() → AllocDescriptorHandle → CreateSRV → TEXTURE_HANDLE 반환
  DeleteTexture()      → pTexResource->Release + FreeDescriptorHandle
```

---

## SRV 데이터 흐름 전체 경로

```
[CreateTiledTexture() — 앱 초기화 시 1회]

  CPU malloc(TexW * TexH * 4) → 체커보드 픽셀 채우기
          │
          └─► CD3D12ResourceManager::CreateTexture()
                  Upload Heap → GPU Default Heap (텍셀 복사 + GPU Wait)
                  ↓
              pTexResource (ID3D12Resource, Default Heap)

  CSingleDescriptorAllocator::AllocDescriptorHandle(&srv)
          │
          └─► CPU-only heap 슬롯[i] 획득
              pD3DDevice->CreateShaderResourceView(pTexResource, &SRVDesc, srv)
              ↓
          srv (D3D12_CPU_DESCRIPTOR_HANDLE, CPU-only heap 슬롯[i])

  TEXTURE_HANDLE { pTexResource, srv } → void* pTexHandle 반환

──────────────────────────────────────────────────

[Draw() — 매 프레임 매 Draw마다]

  CSingleDescriptorAllocator (CPU-only)          CDescriptorPool (shader-visible)
    srv (슬롯[i])                                    cpuDescriptorTable+1
         │                                                   │
         └──────────── CopyDescriptorsSimple ────────────────┘
                       CPU-only SRV → shader-visible SRV 슬롯

                       gpuDescriptorTable
                            │
                   SetGraphicsRootDescriptorTable(0, ...)
                            │
[GPU 셰이더 실행]
  t0 레지스터 → texDiffuse.Sample(samplerDiffuse, input.TexCoord)
  b0 레지스터 → g_Offset → 위치 이동
  PSMain      → texColor * input.color → 최종 픽셀 색상
```

---

## 핵심 패턴 정리

| 패턴 | 설명 |
|------|------|
| **두 종류의 Descriptor Allocator 공존** | 프레임 임시(`CDescriptorPool`) + 영구(`CSingleDescriptorAllocator`)를 역할에 따라 분리 |
| **SRV는 CPU-only heap에서 영구 보관** | `FLAG_NONE` heap에 SRV를 생성해두고 Draw마다 shader-visible pool로 복사 |
| **CopyDescriptorsSimple 2회 호출** | CBV 복사(index 0) + SRV 복사(index 1)를 같은 shader-visible 슬롯 블록에 연속으로 기록 |
| **Root Signature Range 확장** | `ranges[1]`(CBV만) → `ranges[2]`(CBV+SRV). 배열 크기 변경 한 줄로 Descriptor Table 구조 전체가 확장됨 |
| **DESCRIPTOR_COUNT_FOR_DRAW 단일 출처 관리** | 이 값을 `1→2`로 변경하는 것만으로 Pool 크기, Table 슬롯 수, Copy 횟수가 연쇄적으로 결정됨 |
| **NULL-safe SRV 처리** | `srv.ptr`가 0인 경우 SRV 복사를 건너뜀 → 텍스처 없이도 Draw 가능한 설계 유지 |
