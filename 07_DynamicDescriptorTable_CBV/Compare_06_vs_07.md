# 코드 비교: 06_Texture_ConstantBuffer vs 07_DynamicDescriptorTable_CBV

## 개요

| 항목 | 06_Texture_ConstantBuffer | 07_DynamicDescriptorTable_CBV |
|------|--------------------------|-------------------------------|
| 핵심 목표 | 오브젝트별 고정 Descriptor Heap + Constant Buffer | 프레임별 Dynamic Descriptor Pool + CBV Pool |
| Descriptor Heap | 오브젝트마다 개별 Heap 보유 (per-object, static) | 렌더러가 공유 Pool 보유 (per-frame, dynamic) |
| Constant Buffer | 오브젝트가 직접 Resource 생성 및 Map 유지 | Pool에서 draw 시마다 동적 할당 |
| 텍스처 | SRV(Texture2D) + CBV를 하나의 Descriptor Table에 묶어 사용 | CBV만 사용 (텍스처 제거, 이번 예제의 핵심 단순화) |
| Draw 격리성 | 오브젝트 단위로 격리 (동시 draw 다수일 때 문제 발생 가능) | draw call 단위로 완전 격리 |

---

## 신규 추가된 파일

### `DescriptorPool.h` / `DescriptorPool.cpp` ← **신규**

```
07_DynamicDescriptorTable_CBV/
  DescriptorPool.h      ← 신규
  DescriptorPool.cpp    ← 신규
```

#### 클래스: `CDescriptorPool`

렌더러가 하나의 큰 CBV_SRV_UAV Descriptor Heap을 미리 할당해두고, 각 draw call 이 그 안에서 슬롯을 순차적으로 꺼내 쓰는 구조. 프레임 끝에 `Reset()`으로 전체를 재사용한다.

**주요 멤버 변수**

| 멤버 | 타입 | 설명 |
|------|------|------|
| `m_pDescritorHeap` | `ID3D12DescriptorHeap*` | SHADER_VISIBLE Heap — GPU가 직접 접근 가능 |
| `m_MaxDescriptorCount` | `UINT` | 최대 슬롯 수 (초기화 시 결정) |
| `m_AllocatedDescriptorCount` | `UINT` | 현재까지 할당된 슬롯 수 (Reset 시 0으로 리셋) |
| `m_cpuDescriptorHandle` | `D3D12_CPU_DESCRIPTOR_HANDLE` | Heap 시작 CPU 핸들 (CopyDescriptorsSimple 대상) |
| `m_gpuDescriptorHandle` | `D3D12_GPU_DESCRIPTOR_HANDLE` | Heap 시작 GPU 핸들 (SetGraphicsRootDescriptorTable 인자) |
| `m_srvDescriptorSize` | `UINT` | 디스크립터 하나의 바이트 크기 |

**메서드**

---

#### `BOOL Initialize(ID3D12Device5* pD3DDevice, UINT MaxDescriptorCount)`

> 파일: `DescriptorPool.cpp`, line 15

| 인자 | 타입 | 설명 |
|------|------|------|
| `pD3DDevice` | `ID3D12Device5*` | D3D12 디바이스 |
| `MaxDescriptorCount` | `UINT` | 풀이 담을 총 디스크립터 슬롯 수. `MAX_DRAW_COUNT_PER_FRAME * DESCRIPTOR_COUNT_FOR_DRAW`로 전달됨 |

- `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE` 플래그로 Heap 생성
- CPU/GPU 시작 핸들 캐시

---

#### `BOOL AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount)`

> 파일: `DescriptorPool.cpp`, line 41

| 인자 | 타입 | 설명 |
|------|------|------|
| `pOutCPUDescriptor` | `D3D12_CPU_DESCRIPTOR_HANDLE*` | 할당된 슬롯의 CPU 핸들 (CopyDescriptorsSimple 대상) |
| `pOutGPUDescriptor` | `D3D12_GPU_DESCRIPTOR_HANDLE*` | 할당된 슬롯의 GPU 핸들 (SetGraphicsRootDescriptorTable 인자) |
| `DescriptorCount` | `UINT` | 이번 draw에서 필요한 디스크립터 개수 |

- 할당 카운터를 `DescriptorCount`만큼 증가시키고 해당 위치의 핸들 반환
- 용량 초과 시 `FALSE` 반환

---

#### `void Reset()`

> 파일: `DescriptorPool.cpp`, line 57

- `m_AllocatedDescriptorCount = 0`으로만 초기화 — 실제 메모리 해제 없음
- 매 프레임 `Present()` 후 호출됨

---

### `SimpleConstantBufferPool.h` / `SimpleConstantBufferPool.cpp` ← **신규**

```
07_DynamicDescriptorTable_CBV/
  SimpleConstantBufferPool.h      ← 신규
  SimpleConstantBufferPool.cpp    ← 신규
```

#### 구조체: `CB_CONTAINER` ← **신규**

> 파일: `SimpleConstantBufferPool.h`, line 3

```cpp
struct CB_CONTAINER {
    D3D12_CPU_DESCRIPTOR_HANDLE CBVHandle;       // non-shader-visible CBV 디스크립터 핸들
    D3D12_GPU_VIRTUAL_ADDRESS   pGPUMemAddr;     // GPU 메모리 주소
    UINT8*                      pSystemMemAddr;  // CPU(시스템 메모리) 매핑 주소
};
```

draw call이 할당받는 단위. CPU에서 상수값을 `pSystemMemAddr`에 쓰면, GPU는 같은 영역을 `pGPUMemAddr`로 읽는다.

#### 클래스: `CSimpleConstantBufferPool`

하나의 Upload Heap(`D3D12_HEAP_TYPE_UPLOAD`) Resource를 미리 생성하고, 이를 `MaxCBVNum`개의 슬롯으로 분할한다. 각 슬롯마다 non-shader-visible CBV descriptor를 생성해두고, draw 시 `AllocDescriptorTable`로 할당받은 shader-visible 슬롯으로 복사(`CopyDescriptorsSimple`)하여 사용한다.

**메서드**

---

#### `BOOL Initialize(ID3D12Device* pD3DDevice, UINT SizePerCBV, UINT MaxCBVNum)`

> 파일: `SimpleConstantBufferPool.cpp`, line 12

| 인자 | 타입 | 설명 |
|------|------|------|
| `pD3DDevice` | `ID3D12Device*` | D3D12 디바이스 |
| `SizePerCBV` | `UINT` | 슬롯 하나의 크기 (256바이트 정렬 필수, `AlignConstantBufferSize`로 맞춤) |
| `MaxCBVNum` | `UINT` | 최대 CBV 슬롯 수 (`MAX_DRAW_COUNT_PER_FRAME`으로 전달됨) |

- `SizePerCBV * MaxCBVNum` 크기의 Upload Heap Buffer 한 개 생성
- non-shader-visible CBV Descriptor Heap 생성
- 전체를 Map하여 `m_pSystemMemAddr` 캐시
- 루프로 각 슬롯에 대해 `CreateConstantBufferView` 호출, `CB_CONTAINER` 배열 초기화

---

#### `CB_CONTAINER* Alloc()`

> 파일: `SimpleConstantBufferPool.cpp`, line 74

- `m_AllocatedCBVNum`번 슬롯의 `CB_CONTAINER` 포인터 반환 후 카운터 증가
- 용량 초과 시 `nullptr` 반환

---

#### `void Reset()`

> 파일: `SimpleConstantBufferPool.cpp`, line 83

- `m_AllocatedCBVNum = 0` — 매 프레임 `Present()` 후 호출됨

---

## 변경된 파일

### `D3D12Renderer.h`

#### 추가된 forward declaration

```cpp
// 06: 없음
// 07: 추가
class CDescriptorPool;
class CSimpleConstantBufferPool;
```

> 파일: `D3D12Renderer.h`, line 5-6

#### 추가된 멤버 변수

| 멤버 | 06 | 07 | 설명 |
|------|----|----|------|
| `m_pDescriptorPool` | 없음 | `CDescriptorPool*` | 전역 Descriptor Pool |
| `m_pConstantBufferPool` | 없음 | `CSimpleConstantBufferPool*` | 전역 CBV Pool |
| `m_srvDescriptorSize` | 없음 | `UINT` | SRV/CBV/UAV 디스크립터 단위 크기 캐시 |

> 파일: `D3D12Renderer.h`, line 17-18, 36

#### 추가된 public inline accessor

```cpp
// 07에서 추가
CDescriptorPool*           INL_GetDescriptorPool()       { return m_pDescriptorPool; }
CSimpleConstantBufferPool* INL_GetConstantBufferPool()   { return m_pConstantBufferPool; }
UINT                       INL_GetSrvDescriptorSize()    { return m_srvDescriptorSize; }
```

> 파일: `D3D12Renderer.h`, line 70-72

`CBasicMeshObject::Draw()` 내부에서 풀 포인터와 디스크립터 크기를 얻기 위해 사용된다.

---

### `D3D12Renderer.cpp`

#### 추가된 include

```cpp
// 07에서 추가
#include "DescriptorPool.h"
#include "SimpleConstantBufferPool.h"
```

> 파일: `D3D12Renderer.cpp`, line 10-11

---

#### `BOOL Initialize(...)` — 추가된 초기화 코드

> 파일: `D3D12Renderer.cpp`

**변경점 1: `m_srvDescriptorSize` 초기화**

```cpp
// 06: 없음
// 07: RTV 생성 루프 직후 추가
m_srvDescriptorSize = m_pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
```

CBV/SRV/UAV 디스크립터 하나의 바이트 크기를 한 번만 조회해 캐싱. `CopyDescriptorsSimple` 시 offset 계산에 사용됨.

**변경점 2: `CDescriptorPool` 생성**

```cpp
// 07: 추가
m_pDescriptorPool = new CDescriptorPool;
m_pDescriptorPool->Initialize(m_pD3DDevice, MAX_DRAW_COUNT_PER_FRAME * CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW);
```

| 인자 | 값 | 설명 |
|------|-----|------|
| `pD3DDevice` | `m_pD3DDevice` | |
| `MaxDescriptorCount` | `256 * 1 = 256` | 한 프레임 최대 draw 수 × draw당 필요 descriptor 수 |

**변경점 3: `CSimpleConstantBufferPool` 생성**

```cpp
// 07: 추가
m_pConstantBufferPool = new CSimpleConstantBufferPool;
m_pConstantBufferPool->Initialize(m_pD3DDevice,
    AlignConstantBufferSize(sizeof(CONSTANT_BUFFER_DEFAULT)),
    MAX_DRAW_COUNT_PER_FRAME);
```

| 인자 | 값 | 설명 |
|------|-----|------|
| `pD3DDevice` | `m_pD3DDevice` | |
| `SizePerCBV` | `AlignConstantBufferSize(sizeof(CONSTANT_BUFFER_DEFAULT))` | 256바이트 단위 정렬된 CBV 크기 |
| `MaxCBVNum` | `256` | 한 프레임 최대 draw 수 |

---

#### `void Present()` — Reset 추가

```cpp
// 06: Fence/Wait 이후 아무것도 없음
// 07: 풀 초기화 추가
Fence();
WaitForFenceValue();

m_pConstantBufferPool->Reset();   // ← 추가
m_pDescriptorPool->Reset();       // ← 추가
```

> 파일: `D3D12Renderer.cpp`

GPU가 해당 프레임 렌더링을 완료한 것을 Fence로 확인한 뒤, 다음 프레임을 위해 두 풀의 할당 카운터를 0으로 리셋한다. 실제 메모리는 해제되지 않으므로 오버헤드가 극히 낮다.

---

#### `void Cleanup()` — 풀 해제 추가

```cpp
// 07: 추가
if (m_pConstantBufferPool) {
    delete m_pConstantBufferPool;
    m_pConstantBufferPool = nullptr;
}
if (m_pDescriptorPool) {
    delete m_pDescriptorPool;
    m_pDescriptorPool = nullptr;
}
```

> 파일: `D3D12Renderer.cpp`

---

### `BasicMeshObject.h`

#### `DESCRIPTOR_COUNT_FOR_DRAW` 접근성 및 값 변경

| 항목 | 06 | 07 |
|------|----|----|
| 접근 지정자 | `private` (클래스 내부 전용) | `public` (렌더러에서 Pool 크기 계산에 사용) |
| 값 | `2` (CBV + SRV/TEX) | `1` (CBV만) |

> 파일: `BasicMeshObject.h`, line 12-13

렌더러의 `Initialize()`에서 `CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW`를 직접 참조하기 때문에 `public`으로 변경되었다.

#### `BASIC_MESH_DESCRIPTOR_INDEX` 열거형 변경

```cpp
// 06
enum BASIC_MESH_DESCRIPTOR_INDEX {
    BASIC_MESH_DESCRIPTOR_INDEX_CBV = 0,
    BASIC_MESH_DESCRIPTOR_INDEX_TEX = 1   // ← 제거됨
};

// 07
enum BASIC_MESH_DESCRIPTOR_INDEX {
    BASIC_MESH_DESCRIPTOR_INDEX_CBV = 0
};
```

> 파일: `BasicMeshObject.h`, line 3-6

텍스처를 제거함에 따라 SRV 슬롯 인덱스가 삭제됨.

#### 제거된 멤버 변수

| 제거 멤버 | 타입 | 역할 |
|-----------|------|------|
| `m_pTexResource` | `ID3D12Resource*` | 텍스처 Resource |
| `m_pConstantBuffer` | `ID3D12Resource*` | per-object CBV Resource (Upload Heap) |
| `m_pSysConstBufferDefault` | `CONSTANT_BUFFER_DEFAULT*` | CPU 매핑 주소 |
| `m_srvDescriptorSize` | `UINT` | 디스크립터 크기 (렌더러로 이동) |
| `m_pDescritorHeap` | `ID3D12DescriptorHeap*` | per-object shader-visible Heap |

#### 제거된 메서드

| 제거 메서드 | 역할 |
|-------------|------|
| `CreateDescriptorTable()` | per-object Descriptor Heap 생성 |

---

### `BasicMeshObject.cpp`

#### 추가된 include

```cpp
// 07에서 추가
#include "SimpleConstantBufferPool.h"
#include "DescriptorPool.h"
```

> 파일: `BasicMeshObject.cpp`, line 7-8

---

#### `BOOL InitRootSinagture()` — Descriptor Range 축소

```cpp
// 06: ranges[2] — CBV + SRV
CD3DX12_DESCRIPTOR_RANGE ranges[2] = {};
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // b0
ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // t0 ← 제거

// 07: ranges[1] — CBV만
CD3DX12_DESCRIPTOR_RANGE ranges[1] = {};
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // b0
```

> 파일: `BasicMeshObject.cpp`

텍스처 SRV가 Root Signature에서 제거됨. Root Parameter 개수(1)는 동일하나, Descriptor Table이 참조하는 Range가 1개로 줄었다.

---

#### `BOOL CreateMesh()` — 대폭 단순화

**06**: 버텍스 버퍼 + **텍스처 생성** + **Descriptor Heap 생성** + **SRV 생성** + **CBV 생성 및 Map**  
**07**: 버텍스 버퍼만 생성

```cpp
// 07 CreateMesh() — 핵심 부분
if (FAILED(pResourceManager->CreateVertexBuffer(...)))
    goto lb_return;
bResult = TRUE;
// ↑ 이게 전부. 텍스처/CBV/DescriptorHeap 생성 코드 전체 제거
```

> 파일: `BasicMeshObject.cpp`

Constant Buffer와 Descriptor Heap은 이제 풀에서 동적으로 할당되므로 초기화 시 생성할 필요가 없다.

---

#### `void Draw(ID3D12GraphicsCommandList* pCommandList, const XMFLOAT2* pPos)` — 완전 재작성

이 메서드가 이번 예제의 핵심 변경점이다.

**06 방식 (per-object static heap)**

```cpp
// 06 Draw()
m_pSysConstBufferDefault->offset.x = pPos->x;  // 매핑된 메모리에 직접 쓰기
m_pSysConstBufferDefault->offset.y = pPos->y;

pCommandList->SetGraphicsRootSignature(m_pRootSignature);
pCommandList->SetDescriptorHeaps(1, &m_pDescritorHeap);  // 오브젝트 고유 Heap

CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable(m_pDescritorHeap->GetGPUDescriptorHandleForHeapStart());
pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
// ...
```

**07 방식 (dynamic pool allocation)**

```cpp
// 07 Draw() — 요약
// 1. 풀에서 이번 draw 전용 슬롯 할당
CDescriptorPool* pDescriptorPool = m_pRenderer->INL_GetDescriptorPool();
CSimpleConstantBufferPool* pConstantBufferPool = m_pRenderer->INL_GetConstantBufferPool();

CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};
CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};
pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, DESCRIPTOR_COUNT_FOR_DRAW);

// 2. CBV 슬롯 할당 및 상수값 기록
CB_CONTAINER* pCB = pConstantBufferPool->Alloc();
CONSTANT_BUFFER_DEFAULT* pConstantBufferDefault = (CONSTANT_BUFFER_DEFAULT*)pCB->pSystemMemAddr;
pConstantBufferDefault->offset.x = pPos->x;
pConstantBufferDefault->offset.y = pPos->y;

// 3. non-shader-visible CBV를 shader-visible Descriptor Table로 복사
pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);  // 풀의 공유 Heap
CD3DX12_CPU_DESCRIPTOR_HANDLE cbvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_CBV, srvDescriptorSize);
pD3DDeivce->CopyDescriptorsSimple(1, cbvDest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

// 4. GPU 핸들로 Descriptor Table 바인딩
pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
```

> 파일: `BasicMeshObject.cpp`

**`ID3D12Device5::CopyDescriptorsSimple` 상세**

```cpp
pD3DDeivce->CopyDescriptorsSimple(
    1,                                          // 복사할 디스크립터 수
    cbvDest,                                    // 목적지: shader-visible pool의 CPU 핸들
    pCB->CBVHandle,                             // 출처: non-shader-visible CBV Heap의 CPU 핸들
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV      // 힙 타입
);
```

| 인자 | 타입 | 설명 |
|------|------|------|
| `NumDescriptors` | `UINT` | 복사할 디스크립터 개수 |
| `DestDescriptorRangeStart` | `D3D12_CPU_DESCRIPTOR_HANDLE` | 복사 대상 (shader-visible heap의 슬롯) |
| `SrcDescriptorRangeStart` | `D3D12_CPU_DESCRIPTOR_HANDLE` | 복사 원본 (non-shader-visible CBV heap의 슬롯) |
| `DescriptorHeapsType` | `D3D12_DESCRIPTOR_HEAP_TYPE` | 힙 타입 |

> CPU 코드에서 Descriptor는 CPU 핸들로만 write 가능하다. GPU가 실제 읽을 수 있는 곳은 shader-visible heap이므로, non-shader-visible에 미리 만들어둔 CBV를 매 draw마다 shader-visible pool로 복사하는 방식을 사용한다.

---

#### `void Cleanup()` — 제거된 해제 코드

```cpp
// 06에는 존재, 07에서 제거
if (m_pConstantBuffer)  { m_pConstantBuffer->Release(); ... }
if (m_pDescritorHeap)   { m_pDescritorHeap->Release(); ... }
if (m_pTexResource)     { m_pTexResource->Release(); ... }
```

> 파일: `BasicMeshObject.cpp`

오브젝트가 자원을 소유하지 않으므로 해제할 것도 없다.

---

### `Shaders/shaders.hlsl`

#### 06 → 07 변경

```hlsl
// 06: Texture + Sampler 선언 존재
Texture2D texDiffuse : register(t0);
SamplerState samplerDiffuse : register(s0);

// 07: 제거됨
```

```hlsl
// 06 PSMain: 텍스처 샘플링
float4 PSMain(PSInput input) : SV_TARGET
{
    float4 texColor = texDiffuse.Sample(samplerDiffuse, input.TexCoord);
    return texColor * input.color;
}

// 07 PSMain: vertex color만 반환
float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
```

> 파일: `Shaders/shaders.hlsl`

Root Signature에서 SRV Range가 제거됨에 따라 HLSL에서도 Texture2D / SamplerState 선언 및 샘플링 코드가 제거됨.

---

## 변경 없는 파일

| 파일 | 이유 |
|------|------|
| `typedef.h` | `BasicVertex`, `RGBA`, `CONSTANT_BUFFER_DEFAULT` 구조체 동일 |
| `D3D12ResourceManager.h` / `.cpp` | Resource 생성 유틸리티 동일 |
| `main.cpp` | 진입점 동일 |
| `pch.h` / `pch.cpp` | 동일 |

---

## 아키텍처 변화 요약

```
[06] per-object 구조
──────────────────────────────
CBasicMeshObject (A)       CBasicMeshObject (B)
  ├─ ID3D12Resource (CBV)    ├─ ID3D12Resource (CBV)
  ├─ ID3D12Resource (TEX)    ├─ ID3D12Resource (TEX)
  └─ DescriptorHeap          └─ DescriptorHeap
       [CBV][SRV]                  [CBV][SRV]

→ Draw 시: 각 오브젝트 고유 Heap을 SetDescriptorHeaps로 바인딩
  문제: 동일 프레임 내 다수 Draw 시 Heap 교체 비용, 격리 보장 어려움

──────────────────────────────────────────────────────

[07] per-frame pool 구조
──────────────────────────────────────────────────────
CD3D12Renderer
  ├─ CDescriptorPool       (shader-visible, 프레임 공유)
  │    [  0  ][  1  ][ ... ][ 255 ]   ← Reset() 시 재사용
  └─ CSimpleConstantBufferPool (Upload Heap, 프레임 공유)
       [CB_0][CB_1][ ... ][CB_255]   ← Reset() 시 재사용

CBasicMeshObject::Draw() 호출 시:
  1. DescriptorPool에서 슬롯 1개 할당 → cpuHandle, gpuHandle
  2. CBVPool에서 슬롯 1개 할당 → CB_CONTAINER (pSystemMemAddr에 offset 기록)
  3. CopyDescriptorsSimple: non-shader-visible CBV → shader-visible pool 슬롯
  4. SetDescriptorHeaps(poolHeap)
  5. SetGraphicsRootDescriptorTable(gpuHandle)

→ GPU Fence 완료 후 Present() 내에서 양쪽 Pool.Reset()
```

---

## 핵심 패턴 정리

| 패턴 | 설명 |
|------|------|
| **Dynamic Descriptor Table** | 매 draw마다 shader-visible heap에서 슬롯을 순차 할당하여 draw 간 격리 보장 |
| **Non-Shader-Visible → Shader-Visible 복사** | `CopyDescriptorsSimple`로 non-visible CBV Heap의 디스크립터를 visible Pool에 복사; CPU는 non-visible만 쓰기 가능 |
| **Upload Heap 풀링** | CBV Resource를 미리 큰 Upload Heap 하나로 만들고 슬롯 분할; 매 프레임 Reset으로 재사용 |
| **Pool Reset** | GPU 완료 확인 후 `Reset()` 호출 — 실제 해제 없이 카운터만 0으로 리셋하여 오버헤드 최소화 |
