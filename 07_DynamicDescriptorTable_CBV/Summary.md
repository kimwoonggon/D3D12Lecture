# 07_DynamicDescriptorTable_CBV 학습 요약

## 1. 핵심 구조 개요

```
CD3D12Renderer
  ├─ CDescriptorPool       (shader-visible heap, 프레임 공유)
  │    [  0  ][  1  ][ ... ][ 255 ]   ← Reset() 시 재사용
  └─ CSimpleConstantBufferPool (Upload Heap, 프레임 공유)
       [CB_0][CB_1][ ... ][CB_255]   ← Reset() 시 재사용
```

---

## 2. Descriptor Heap 두 종류

| 종류 | 누가 접근? | 용도 |
|------|-----------|------|
| **CPU-only** (`FLAG_NONE`) | CPU만 | descriptor 생성/보관용 |
| **Shader-visible** (`FLAG_SHADER_VISIBLE`) | CPU + GPU | 실제 렌더링에서 셰이더가 읽는 곳 |

- GPU 셰이더는 **shader-visible heap만** 읽을 수 있다.
- CPU-only heap에 있는 descriptor를 shader-visible heap으로 복사하는 것이 `CopyDescriptorsSimple`.
- D3D11까지는 드라이버가 자동으로 해줬지만, D3D12는 개발자가 직접 관리한다.

---

## 3. 등장인물 비유 (요리사 비유)

| 코드 | 비유 |
|------|------|
| GPU | 주방 요리사. **주문 창구(shader-visible heap)만** 볼 수 있음 |
| shader-visible heap (DescriptorPool) | 요리사가 보는 **주문 창구 칠판** |
| CPU-only heap (CBPool 내부) | 주방장만 보는 **사무실 메모장** |
| Constant Buffer (Upload Heap) | 실제 데이터가 든 **냉장고 칸** |
| descriptor (CBVHandle) | "냉장고 몇 번 칸에 재료 있어" 라는 **쪽지** |

---

## 4. 주요 변수 정체

### `pCB->CBVHandle`
- `SimpleConstantBufferPool::Initialize()` 에서 생성
- **CPU-only heap** 안의 descriptor 주소 (GPU 접근 불가)
- `CreateDescriptorHeap(FLAG_NONE)` 으로 만든 heap의 슬롯

```cpp
// 초기화 시 1회
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;  // CPU-only
pD3DDevice->CreateConstantBufferView(&cbvDesc, heapHandle);
m_pCBContainerList[i].CBVHandle = heapHandle;  // ← pCB->CBVHandle의 원형
```

### `cpuDescriptorTable` / `gpuDescriptorTable`
- **같은 물리 슬롯**을 CPU/GPU 각각의 시점으로 부르는 이름
- `AllocDescriptorTable()` 이 반환하는 두 주소
- `cpuDescriptorTable` = 내가 그 슬롯에 쓸 때 사용
- `gpuDescriptorTable` = GPU가 그 슬롯을 읽을 때 사용

### `cbvDest`
- `cpuDescriptorTable`에서 특정 인덱스로 파생된 주소
- 이 예제에서 `BASIC_MESH_DESCRIPTOR_INDEX_CBV = 0` 이므로 사실상 `cpuDescriptorTable`과 동일

---

## 5. `AllocDescriptorTable` 동작 원리 (선형 할당자)

```cpp
// heap은 Initialize()에서 미리 통째로 생성됨
// AllocDescriptorTable은 포인터 계산만 수행 (new/malloc 없음)
*pOutCPUDescriptor = m_cpuDescriptorHandle + (N × size);
*pOutGPUDescriptor = m_gpuDescriptorHandle + (N × size);
m_AllocatedDescriptorCount += DescriptorCount;  // 카운터 전진
```

```
[shader-visible heap]
 ┌────┬────┬────┬────┬────┐
 │    │    │    │    │    │
 └────┴────┴────┴────┴────┘
  ↑              ↑
  시작           m_AllocatedDescriptorCount (Draw마다 전진)

프레임 끝 → Reset() → 카운터 0으로 (메모리 해제 없음)
```

---

## 6. `m_pDescritorHeap` → `gpuDescriptorTable` 연결 경로

```
m_pDescritorHeap (heap 객체, Initialize에서 생성)
    │
    │ GetGPUDescriptorHandleForHeapStart()
    ↓
m_gpuDescriptorHandle  (heap의 GPU 시작 주소)
    │
    │ + (N × srvDescriptorSize)  ← AllocDescriptorTable
    ↓
gpuDescriptorTable  (N번 슬롯의 GPU 주소)
    │
    ├─ SetDescriptorHeaps(m_pDescritorHeap)  ← "이 heap 쓸게"
    └─ SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)  ← "이 슬롯 읽어"
```

---

## 7. Draw() 전체 흐름

### 단계별 설명

| 단계 | 코드 | 실제 데이터 이동? |
|------|------|---------|
| 칠판 칸 예약 | `AllocDescriptorTable` | 없음 (주소 계산만) |
| 냉장고 칸 할당 | `pConstantBufferPool->Alloc()` | 없음 (카운터만 증가) |
| 재료 넣기 | `pConstantBufferDefault->offset = pPos` | CPU → Upload Buffer |
| **쪽지 칠판에 옮겨 쓰기** | **`CopyDescriptorsSimple`** | **CPU-only → shader-visible heap** |
| 요리사에게 칠판 칸 지시 | `SetGraphicsRootDescriptorTable` | 없음 (명령 등록만) |
| 실제 요리 | `DrawInstanced` | GPU가 실행 |

### Root Signature와의 관계

| | 역할 | 언제 결정? |
|---|---|---|
| Root Signature | `"slot[0]은 CBV 1개짜리 table"` 규격 | 초기화 시 1회 |
| `SetGraphicsRootDescriptorTable` | `"slot[0]의 실제 GPU 주소는 X"` | Draw마다 |

Root Signature = **형식**, `SetGraphicsRootDescriptorTable` = **값**.

---

## 8. 전체 데이터 흐름 다이어그램

```
[SimpleConstantBufferPool 초기화]
  GPU Upload Buffer ──CreateCBV()──► CPU-only heap
                   └──Map()──────► pSystemMemAddr (CPU 쓰기 포인터)

[Draw() 호출]

  1. AllocDescriptorTable()
     shader-visible heap의 슬롯[N] 예약
     → cpuDescriptorTable (CPU 주소)
     → gpuDescriptorTable (GPU 주소)  ← 같은 물리 슬롯

  2. pConstantBufferPool->Alloc()
     → pCB->pSystemMemAddr 에 offset.x, offset.y 쓰기
     → pCB->CBVHandle (CPU-only heap 슬롯 주소)

  3. cbvDest = cpuDescriptorTable + index(0)
     CopyDescriptorsSimple(cbvDest, pCB->CBVHandle)
     CPU-only heap ──────────────────► shader-visible heap 슬롯[N]

  4. SetDescriptorHeaps(m_pDescritorHeap)
     SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)

[GPU 실행]
  slot[0] → 슬롯[N] → CBV → Upload Buffer → offset.x, offset.y
  → VSMain: b0.offset 으로 삼각형 위치 결정
```

---

## 9. 왜 Draw마다 새 슬롯을 쓰는가

```
프레임 안에서 Draw 3번 호출:

shader-visible heap:  [Draw#1용] [Draw#2용] [Draw#3용] ...
Upload Buffer:        [CB#1]     [CB#2]     [CB#3]    ...

명령 리스트 기록 완료 후 GPU가 실행할 때
  Draw#1 → 슬롯[0], Draw#2 → 슬롯[1], Draw#3 → 슬롯[2]
  → 서로 덮어쓸 위험 없음
```

만약 같은 슬롯을 재사용하면:
```
Draw#1 기록: 슬롯[0]에 CBV_A 복사
Draw#2 기록: 슬롯[0]에 CBV_B 덮어씀  ← Draw#1도 CBV_B를 보게 됨!
```

프레임 끝에 GPU Fence 완료 후 `Reset()` 으로 카운터만 0으로 되돌린다.

---

## 10. CDescriptorPool vs CSimpleConstantBufferPool 관계

두 클래스는 **역할이 완전히 다른 별개의 풀**이다.

### 각자의 책임

| | CSimpleConstantBufferPool | CDescriptorPool |
|---|---|---|
| **무엇을 관리** | 실제 데이터 + CPU-only 쪽지 | shader-visible 칠판 슬롯 |
| **heap 종류** | Upload Buffer (데이터) + CPU-only Heap (쪽지) | shader-visible Heap (칠판) |
| **GPU가 직접 접근** | 불가 | 가능 |
| **용도** | 상수 데이터 보관 | GPU 렌더링용 descriptor 제공 |

### 내부 구조 비교

```
CSimpleConstantBufferPool
  ├─ m_pResource  (Upload Buffer - 냉장고)
  │    [데이터0][데이터1][데이터2]...
  │         ↑ Map()으로 CPU가 직접 씀 (pSystemMemAddr)
  └─ m_pCBVHeap  (CPU-only Heap - 사무실 메모장)
       [쪽지0→데이터0][쪽지1→데이터1]...
            ↑ pCB->CBVHandle

CDescriptorPool
  └─ m_pDescritorHeap  (shader-visible Heap - 주문 창구 칠판)
       [슬롯0][슬롯1][슬롯2]...
            ↑ Draw마다 비어있는 슬롯 할당 (cpuDescriptorTable / gpuDescriptorTable)
```

### Draw()에서 두 풀이 만나는 순간

```
CSimpleConstantBufferPool         CDescriptorPool
  pCB->CBVHandle                    cpuDescriptorTable
  (사무실 메모장의 쪽지)             (칠판의 빈 슬롯 CPU주소)
          │                                │
          └──────── CopyDescriptorsSimple ─┘
                    쪽지 내용을 칠판에 옮겨 적음
                            │
                    gpuDescriptorTable
                    (칠판의 그 슬롯 GPU주소)
                            │
                   SetGraphicsRootDescriptorTable
                            │
                          GPU 렌더링
```

### 한 줄 요약

> `CSimpleConstantBufferPool`은 **"무슨 데이터인지"** 를 갖고 있고,  
> `CDescriptorPool`은 **"GPU에게 보여줄 자리"** 를 제공한다.  
> `CopyDescriptorsSimple`이 두 풀을 연결하는 유일한 다리다.

---

## 11. CBPool에도 descriptor heap이 있는 이유

### 혼란 포인트

```
CSimpleConstantBufferPool
  ├─ m_pResource  ← 실제 데이터 (당연함)
  └─ m_pCBVHeap   ← 왜 얘도 descriptor heap이 있지?
```

### 이유: 사전 제작된 CBV 원본 보관함

`m_pCBVHeap`은 초기화 시 256개 CBV를 **한 번에 미리 만들어 두는 캐비닛**이다.

```cpp
// Initialize()에서 1회만 실행
for (DWORD i = 0; i < m_MaxCBVNum; i++)
{
    // "Upload Buffer의 i번 구간을 가리키는 CBV" 미리 생성
    pD3DDevice->CreateConstantBufferView(&cbvDesc, heapHandle);
    // CPU-only heap에 보관 → 이것이 pCB->CBVHandle
    m_pCBContainerList[i].CBVHandle = heapHandle;
}
```

Draw할 때마다 `CreateConstantBufferView`를 새로 호출하는 것이 아니라,  
**미리 만들어진 것을 꺼내서 복사**하는 구조다.

### 두 heap의 수명 비교

| | `CBPool::m_pCBVHeap` | `DescriptorPool::m_pDescritorHeap` |
|---|---|---|
| 비유 | 원본 서류 캐비닛 | GPU 앞 공개 게시판 |
| 내용 변경 | 앱 실행 내내 **불변** | 프레임마다 **Reset** |
| 목적 | 미리 만들어 빠르게 복사 | GPU 렌더링용 슬롯 제공 |

---

## 12. 두 heap의 `D3D12_DESCRIPTOR_HEAP_DESC` 비교

### 실제 코드

```cpp
// CSimpleConstantBufferPool::Initialize()
heapDesc.NumDescriptors = m_MaxCBVNum;
heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;          // ← CPU-only
```

```cpp
// CDescriptorPool::Initialize()
commonHeapDesc.NumDescriptors = m_MaxDescriptorCount;
commonHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
commonHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // ← GPU 접근 가능
```

### `Flags` 하나의 차이가 만드는 결과

| | `FLAG_NONE` (CBPool) | `FLAG_SHADER_VISIBLE` (DescriptorPool) |
|---|---|---|
| GPU 접근 | 불가 | 가능 |
| CPU 쓰기 속도 | 빠름 | 느림 |
| `GetGPUDescriptorHandleForHeapStart()` | 호출 불가 (에러) | 가능 |
| `SetDescriptorHeaps()`에 등록 | 불가 | 가능 |
| GPU 주소 공간 매핑 | 없음 | 있음 (비용 발생) |

`FLAG_SHADER_VISIBLE`을 붙이는 순간 GPU 주소 공간에 매핑되어 제약과 비용이 생긴다.  
그래서 평소엔 `FLAG_NONE`에 보관하다가 Draw 직전에 `CopyDescriptorsSimple`로 복사한다.

---

## 13. Chapter 06 vs 07 핵심 차이

| | 06 | 07 |
|---|---|---|
| 오브젝트 수 | 1개 | 1개 (동일) |
| Draw 횟수/프레임 | 1회 | **2회** (같은 오브젝트) |
| CBV 필요 수 | 1개면 충분 | **Draw마다 독립된 CBV 필요** |
| DescriptorPool | 불필요 | **필요** |

### 07에서 같은 오브젝트를 2번 그리는 코드 (main.cpp)

```cpp
// 오브젝트는 1개
g_pMeshObj = g_pRenderer->CreateBasicMeshObject();

// 매 프레임 2번 Draw (위치만 다르게)
g_pRenderer->RenderMeshObject(g_pMeshObj, g_fOffsetX, 0.0f);  // 좌우 이동
g_pRenderer->RenderMeshObject(g_pMeshObj, 0.0f,      g_fOffsetY); // 상하 이동
```

### 위치 업데이트 (Update)

```cpp
void Update()
{
    g_fOffsetX += g_fSpeedX;  // X축 독립적으로 움직임
    if (g_fOffsetX >  0.75f) g_fSpeedX *= -1.0f;
    if (g_fOffsetX < -0.75f) g_fSpeedX *= -1.0f;

    g_fOffsetY += g_fSpeedY;  // Y축 독립적으로 움직임
    if (g_fOffsetY >  0.75f) g_fSpeedY *= -1.0f;
    if (g_fOffsetY < -0.75f) g_fSpeedY *= -1.0f;
}
```

만약 CBV 슬롯을 재사용하면 두 번째 Draw가 첫 번째 값을 덮어써서  
두 삼각형이 같은 위치에 그려지는 버그가 발생한다.  
**Pool 구조가 필요한 근본적인 이유가 바로 이것이다.**

---

## 14. Pool 카운터의 증가와 Reset 타이밍

### 카운터는 어디서 증가하는가?

```cpp
// SimpleConstantBufferPool::Alloc() 내부
pCB = m_pCBContainerList + m_AllocatedCBVNum;  // 현재 슬롯 반환
m_AllocatedCBVNum++;                            // ← 여기서 증가

// DescriptorPool::AllocDescriptorTable() 내부
*pOutCPUDescriptor = ...(m_AllocatedDescriptorCount, ...);  // 현재 슬롯 반환
*pOutGPUDescriptor = ...(m_AllocatedDescriptorCount, ...);
m_AllocatedDescriptorCount += DescriptorCount;              // ← 여기서 증가
```

`Draw()`를 호출하는 쪽에서 카운터를 직접 건드리는 것이 아니라,  
`Alloc()` / `AllocDescriptorTable()` 함수 내부에서 자동으로 증가한다.

### 프레임 생명주기

```
프레임 시작
  RenderMeshObject #1 → Draw() → CBPool 카운터 0→1, DescPool 카운터 0→1
  RenderMeshObject #2 → Draw() → CBPool 카운터 1→2, DescPool 카운터 1→2
  ...
  EndRender()
  Present()
    ├─ Fence()              ← GPU에게 "이 시점까지 작업 완료 신호 보내"
    ├─ WaitForFenceValue()  ← GPU가 다 읽을 때까지 CPU 대기
    ├─ CBPool.Reset()       → m_AllocatedCBVNum = 0
    └─ DescriptorPool.Reset() → m_AllocatedDescriptorCount = 0

다음 프레임 시작
  RenderMeshObject #1 → Draw() → 카운터 0→1  (처음부터 다시)
  ...
```

### 왜 WaitForFenceValue() 이후에 Reset 하는가?

```
만약 GPU가 아직 읽는 도중에 Reset() 하면:

  GPU: DescriptorPool 슬롯[0]에서 CBV 읽는 중...
  CPU: Reset() → m_AllocatedDescriptorCount = 0
  CPU: 다음 프레임 Draw() → 슬롯[0]에 새 값 덮어씀  ← 버그!
```

`WaitForFenceValue()`로 GPU 작업 완료를 확인한 뒤에 Reset해야  
이전 프레임 데이터가 안전하게 덮어써진다.

---

## 15. pSystemMemAddr과 CBVHandle의 관계 (Upload Buffer 연결)

### 혼란 포인트

```
pCB->pSystemMemAddr  에 offset.x, offset.y 쓰면 바로 GPU로 가는 거 아닌가?
그러면 CBVHandle은 왜 필요한가?
```

### 둘은 같은 Upload Buffer를 다른 시점으로 본 것

`SimpleConstantBufferPool::Initialize()`에서 연결됩니다:

```cpp
// 1. Upload Buffer 생성 (CPU+GPU 공유 메모리)
CreateCommittedResource(D3D12_HEAP_TYPE_UPLOAD, ..., &m_pResource)

// 2. CPU 포인터 획득
m_pResource->Map(..., &m_pSystemMemAddr)
//  └─ Upload Buffer의 CPU측 포인터

// 3. GPU 주소를 CBV에 기록
cbvDesc.BufferLocation = m_pResource->GetGPUVirtualAddress()
//  └─ Upload Buffer의 GPU측 주소

// 4. 슬롯마다 두 포인터를 같은 오프셋으로 저장
m_pCBContainerList[i].pSystemMemAddr = pSystemMemPtr;   // CPU 포인터
m_pCBContainerList[i].CBVHandle      = heapHandle;      // GPU주소 담긴 descriptor
cbvDesc.BufferLocation += m_SizePerCBV;  // 다음 슬롯 (동시에)
pSystemMemPtr          += m_SizePerCBV;  // 다음 슬롯 (동시에)
```

```
Upload Buffer (D3D12_HEAP_TYPE_UPLOAD = CPU+GPU 공유 메모리)
  ┌──────────┬──────────┬──────────┐
  │ 슬롯[0]  │ 슬롯[1]  │ 슬롯[2]  │
  └──────────┴──────────┴──────────┘
       ↑ CPU에서 보면           ↑ GPU에서 보면
  pSystemMemAddr[0]        cbvDesc.BufferLocation[0]
  (CPU 포인터, 쓸 때 사용)  (GPU 가상주소, CBVHandle에 기록됨)
```

### 그러면 CBVHandle은 왜 필요한가?

`pSystemMemAddr`에 쓰면 데이터는 GPU 메모리에 즉시 반영된다. 맞다.  
하지만 GPU가 셰이더에서 읽으려면 **Root Signature 체계를 통한 공식 선언(CBV)** 이 필요하다.

```
pSystemMemAddr → Upload Buffer에 데이터 씀          (CPU의 역할)
CBVHandle      → "GPU야, 이 주소에 CB 있어"         (GPU에게 알리는 역할)
                  Root Signature → descriptor table → CBV → Upload Buffer
```

### 두 역할 정리

| | `pSystemMemAddr` | `CBVHandle` |
|---|---|---|
| 타입 | CPU 포인터 (`UINT8*`) | CPU-only heap 슬롯 주소 |
| 용도 | CPU가 데이터를 **쓸 때** | GPU가 데이터를 **찾을 때** |
| 가리키는 것 | Upload Buffer의 CPU 주소 | Upload Buffer의 GPU 주소를 담은 descriptor |
| 독립적? | **같은 Upload Buffer 슬롯을 각자 역할에 맞게 본 것** |

### 만약 descriptor table 대신 inline 방식을 쓴다면?

`SetGraphicsRootConstantBufferView()`를 쓰면 GPU 주소를 직접 넘길 수 있어  
CBV heap 자체가 불필요해진다. 이 예제는 **descriptor table 방식**이기 때문에  
CBV descriptor를 통하는 우회 경로가 필요한 것이다.

---

## 16. descriptor 크기 관련

### CBV/SRV/UAV descriptor가 ~32 bytes인 이유

descriptor는 **실제 데이터가 아니라 메타데이터(포인터+설명)** 만 담기 때문이다.

#### CBV descriptor 내용

```
D3D12_CONSTANT_BUFFER_VIEW_DESC
  ├─ BufferLocation : UINT64  (8 bytes) ← GPU 버퍼의 가상 주소
  └─ SizeInBytes    : UINT    (4 bytes) ← 버퍼 크기
```

#### SRV descriptor 내용 (1024×1024 텍스처 예시)

```
D3D12_SHADER_RESOURCE_VIEW_DESC (~28 bytes)
  ├─ Format         : DXGI_FORMAT   (4 bytes) ← R8G8B8A8_UNORM 등
  ├─ ViewDimension  : SRV_DIMENSION (4 bytes) ← TEXTURE2D
  ├─ Shader4ComponentMapping        (4 bytes)
  └─ Texture2D:
       ├─ MostDetailedMip           (4 bytes)
       ├─ MipLevels                 (4 bytes)
       └─ ...

실제 텍스처 픽셀 데이터 (4MB) = GPU 메모리 어딘가에 따로 존재
→ descriptor에는 단 한 픽셀도 없음
```

#### 비유

```
descriptor = 도서관 카드 목록 (위치, 종류, 두께만 적힘)
실제 데이터 = 서가에 꽂힌 책 (내용이 있음, 수 MB)
```

### descriptor 크기는 GPU가 결정

```cpp
m_srvDescriptorSize = m_pD3DDevice->GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
);
// GPU 아키텍처마다 다름 (NVIDIA/AMD/Intel 각기 다를 수 있음)
// 하드코딩 불가, 반드시 런타임에 물어봐야 함
```
