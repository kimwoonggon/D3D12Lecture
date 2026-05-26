# Chapter 12 — MultiMaterial 객관식 퀴즈

> 아래 퀴즈는 `12_MultiMaterial` 프로젝트의 실제 코드를 기반으로 출제되었습니다.  
> 각 문제의 보기 중 **정답 하나**를 고르고, 아래 **▶ 정답 및 해설** 을 클릭하면 확인할 수 있습니다.

---

## Q1. Descriptor 테이블 레이아웃

`dwRequiredDescriptorCount = DESCRIPTOR_COUNT_PER_OBJ + (m_dwTriGroupCount * DESCRIPTOR_COUNT_PER_TRI_GROUP)`  
이 수식에서 두 상수의 값과 의미로 올바른 것은?

| 보기 | 내용 |
|-----|------|
| A | 둘 다 2 — CBV 2개, SRV 2개 |
| B | `DESCRIPTOR_COUNT_PER_OBJ = 1` (CBV 1개), `DESCRIPTOR_COUNT_PER_TRI_GROUP = 1` (SRV 1개) |
| C | `DESCRIPTOR_COUNT_PER_OBJ = 2` (CBV+SRV), `DESCRIPTOR_COUNT_PER_TRI_GROUP = 0` |
| D | `DESCRIPTOR_COUNT_PER_OBJ = 0`, `DESCRIPTOR_COUNT_PER_TRI_GROUP = 2` (CBV+SRV) |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

- `DESCRIPTOR_COUNT_PER_OBJ = 1` : 오브젝트 전체가 공유하는 **CBV 1개** (World/View/Proj 행렬)
- `DESCRIPTOR_COUNT_PER_TRI_GROUP = 1` : 트라이그룹마다 다른 **SRV 1개** (텍스처)

박스(6면) 기준 `1 + 6×1 = 7`개, 최대(8면) `1 + 8×1 = 9`개.

```
| CBV | SRV[0] | SRV[1] | ... | SRV[N] |
  ←1→  ←─────────── triGroupCount ────────→
```

</details>

---

## Q2. `InitCommonResources`의 static ref count

`InitCommonResources()`에서 `m_dwInitRefCount`를 체크하는 이유로 올바른 것은?

| 보기 | 내용 |
|-----|------|
| A | 동시에 렌더링할 수 있는 오브젝트 수를 제한하기 위해서다 |
| B | `m_pRootSignature`와 `m_pPipelineState`가 static 멤버라 모든 인스턴스가 공유하므로, 최초 1번만 생성하고 중복 생성을 막기 위해서다 |
| C | 멀티스레드 환경에서 레이스 컨디션을 방지하기 위한 뮤텍스 대용이다 |
| D | GPU 메모리 할당 실패 시 재시도 횟수를 추적하기 위해서다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

```cpp
static ID3D12RootSignature* m_pRootSignature;  // 모든 인스턴스 공유
static ID3D12PipelineState* m_pPipelineState;  // 모든 인스턴스 공유
static DWORD                m_dwInitRefCount;

if (m_dwInitRefCount)
    goto lb_true;  // 이미 생성됨 → 건너뜀

InitRootSignature();   // 비싼 GPU 리소스 — 딱 1번만
InitPipelineState();
```

박스든 쿼드든 셰이더·파이프라인이 동일하므로 오브젝트가 100개가 되어도 RootSignature/PSO는 1개만 존재한다.

</details>

---

## Q3. `CopyDescriptorsSimple`이 복사하는 대상

`pD3DDevice->CopyDescriptorsSimple(1, Dest, pTexHandle->srv, ...)` 에서 실제로 복사되는 것은?

| 보기 | 내용 |
|-----|------|
| A | 텍스처 픽셀 데이터를 Upload Heap → Default Heap으로 복사한다 |
| B | Descriptor(리소스를 어떻게 읽을지 기술한 ~64bytes 메타데이터)를 non-shader-visible heap → shader-visible heap으로 복사한다 |
| C | GPU Virtual Address를 CPU 메모리로 복사한다 |
| D | 텍스처 파일을 디스크에서 VRAM으로 직접 복사한다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`CopyDescriptorsSimple`은 텍스처 픽셀 데이터와 무관하다. 복사 대상은 **Descriptor** — 포맷, 크기, 슬롯 등 "이 리소스를 어떻게 읽을지" 기술한 작은 구조체다.

```
텍스처 픽셀 데이터 → Default Heap에 그대로 (움직이지 않음)
Descriptor(메타데이터) → non-shader-visible → shader-visible heap (CopyDescriptorsSimple)
```

CPU가 직접 shader-visible heap에 쓸 수 없는 D3D12 제약 때문에 이 단계가 필요하다.

</details>

---

## Q4. non-shader-visible heap이 따로 존재하는 이유

텍스처 SRV를 처음부터 shader-visible heap(DescriptorPool)에 직접 넣지 않고, non-shader-visible heap에 보관하다가 매 프레임 복사하는 이유는?

| 보기 | 내용 |
|-----|------|
| A | shader-visible heap은 GPU 드라이버만 접근할 수 있어 CPU에서 쓸 수 없기 때문이다 |
| B | DescriptorPool은 매 프레임 `Reset()`으로 초기화되므로, 텍스처처럼 영구 보존이 필요한 SRV를 거기에 넣으면 다음 프레임에 날아가기 때문이다 |
| C | non-shader-visible heap이 GPU 읽기 속도가 더 빠르기 때문이다 |
| D | D3D12 사양상 SRV는 반드시 non-shader-visible에만 생성할 수 있기 때문이다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

```cpp
void CDescriptorPool::Reset()
{
    m_AllocatedDescriptorCount = 0;  // 매 프레임 Present() 후 호출 → 슬롯 전부 재사용 가능 상태
}
```

shader-visible heap(DescriptorPool)은 **1프레임짜리 임시 작업공간**이다.  
텍스처는 씬이 끝날 때까지 살아있어야 하므로 non-shader-visible에 영구 보관하고, 렌더링 직전에 필요한 것만 골라 복사한다.

</details>

---

## Q5. `cpuDescriptorTable`과 `gpuDescriptorTable`의 관계

`AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, N)` 호출 후 두 핸들의 관계로 올바른 것은?

| 보기 | 내용 |
|-----|------|
| A | 서로 다른 두 개의 힙에서 각각 할당된 독립적인 핸들이다 |
| B | 같은 shader-visible 힙의 같은 슬롯을 CPU 관점/GPU 관점으로 각각 가리키는 핸들이다 |
| C | cpuDescriptorTable은 Upload Heap, gpuDescriptorTable은 Default Heap을 가리킨다 |
| D | cpuDescriptorTable은 쓰기 전용, gpuDescriptorTable은 읽기 전용 힙에서 할당된다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

```cpp
// AllocDescriptorTable 내부
*pOutCPUDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cpuHandle, offset, size);
*pOutGPUDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuHandle, offset, size);
// 같은 offset → 같은 슬롯
```

하나의 `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE` 힙이 CPU 주소와 GPU 주소를 동시에 가진다.  
CPU 핸들로 `CopyDescriptorsSimple`을 쓰면 GPU 핸들이 가리키는 슬롯에 즉시 반영된다.

</details>

---

## Q6. `Dest` 핸들의 역할

```cpp
CD3DX12_CPU_DESCRIPTOR_HANDLE Dest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_PER_OBJ_CBV, srvDescriptorSize);
CopyDescriptorsSimple(1, Dest, pCB->CBVHandle, ...);
Dest.Offset(1, srvDescriptorSize);
```
여기서 `Dest`의 역할로 가장 정확한 것은?

| 보기 | 내용 |
|-----|------|
| A | GPU에게 다음 드로우콜의 시작 주소를 알려주는 포인터다 |
| B | shader-visible 힙의 현재 쓸 슬롯 위치를 가리키는 쓰기 커서다 |
| C | 텍스처 픽셀 데이터를 CPU에서 읽기 위한 매핑 포인터다 |
| D | Fence 대기 완료 여부를 확인하는 이벤트 핸들이다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`Dest`는 할당받은 슬롯 블록에서 **지금 쓸 위치를 가리키는 커서**다.  
`CopyDescriptorsSimple`로 현재 위치에 Descriptor를 복사하고,  
`Dest.Offset(1, srvDescriptorSize)`로 다음 슬롯으로 전진한다.

```
슬롯: [ CBV | SRV[0] | SRV[1] | ... ]
Dest:   ↑→Offset→↑→Offset→↑
```

</details>

---

## Q7. `pCB->CBVHandle`의 정체

`CB_CONTAINER` 구조체의 `CBVHandle` 필드가 가리키는 것은?

| 보기 | 내용 |
|-----|------|
| A | Upload Heap 리소스의 GPU Virtual Address |
| B | shader-visible DescriptorPool 힙의 CBV 슬롯 주소 |
| C | `CSimpleConstantBufferPool`이 관리하는 non-shader-visible 힙의 CBV 슬롯 주소 |
| D | CPU 시스템 메모리에서 행렬 데이터를 쓰는 포인터 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

```cpp
struct CB_CONTAINER
{
    D3D12_CPU_DESCRIPTOR_HANDLE  CBVHandle;      // non-shader-visible 힙의 CPU 핸들
    D3D12_GPU_VIRTUAL_ADDRESS    pGPUMemAddr;    // Upload Heap GPU 주소
    UINT8*                       pSystemMemAddr; // CPU write 포인터
};
```

`CBVHandle`은 `CSimpleConstantBufferPool`의 `m_pCBVHeap`(non-shader-visible)에 있다.  
`CopyDescriptorsSimple(1, Dest, pCB->CBVHandle, ...)` 로 shader-visible 힙으로 복사한 뒤 GPU가 읽는다.

</details>

---

## Q8. `SetDescriptorHeaps` 호출 타이밍

`pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap)`을 `SetGraphicsRootDescriptorTable` **이전에** 반드시 호출해야 하는 이유는?

| 보기 | 내용 |
|-----|------|
| A | 힙 생성 시 자동으로 바인딩되지 않으므로 명시적으로 등록해야 GPU가 GPU 핸들 주소를 올바르게 해석할 수 있기 때문이다 |
| B | `SetDescriptorHeaps` 없이는 `CopyDescriptorsSimple`이 실패하기 때문이다 |
| C | DescriptorPool의 `Reset()`이 `SetDescriptorHeaps` 이후에만 동작하기 때문이다 |
| D | D3D12 디버그 레이어 요구사항이며 Release 빌드에서는 생략 가능하다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: A**

`SetGraphicsRootDescriptorTable`에 넘기는 GPU 핸들은 특정 힙 안의 주소다.  
GPU는 어느 힙에서 해당 주소를 찾아야 하는지 알아야 하므로,  
먼저 `SetDescriptorHeaps`로 "이 커맨드리스트가 사용할 힙은 이것"이라고 등록해야 한다.

</details>

---

## Q9. 왜 면마다 `DrawIndexedInstanced`를 따로 호출하나?

박스 6면을 한 번의 `DrawIndexedInstanced`로 그리지 않고 면마다 나눠서 호출하는 핵심 이유는?

| 보기 | 내용 |
|-----|------|
| A | `DrawIndexedInstanced`는 최대 6개의 인덱스만 처리할 수 있기 때문이다 |
| B | 셰이더의 `t0`(SRV 슬롯)은 한 드로우콜 안에서 1개의 텍스처만 바인딩되므로, 면마다 다른 텍스처를 쓰려면 드로우콜을 분리해야 한다 |
| C | 인덱스 버퍼를 면마다 따로 만들었기 때문에 GPU가 연속으로 읽지 못한다 |
| D | CommandList는 드로우콜을 한 프레임에 6번 이상 호출할 수 없다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

루트 시그니처에서 RootParam[1]은 **SRV 1개짜리 테이블**로 선언됐다:

```cpp
rangesPerTriGroup[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 = SRV 1개
```

하나의 드로우콜 안에서 t0은 고정된 1개의 텍스처만 가리킨다.  
면마다 다른 텍스처를 사용하려면 `SetGraphicsRootDescriptorTable(1, ...)`로 t0을 교체하고 드로우콜을 분리해야 한다.

</details>

---

## Q10. for 루프의 두 단계 분리

`Draw()` 내부에 루프가 두 개 있다 — ① SRV를 shader-visible heap에 **복사**하는 루프, ② `SetGraphicsRootDescriptorTable`+`DrawIndexedInstanced`를 호출하는 루프.  
왜 두 루프가 분리되어 있나?

| 보기 | 내용 |
|-----|------|
| A | D3D12 API 제약상 CopyDescriptorsSimple과 DrawIndexedInstanced를 같은 루프 안에 섞을 수 없다 |
| B | 복사 단계는 CPU 작업이고 드로우 단계는 GPU CommandList 기록이라 의미적으로 분리하되, 실제로는 같은 루프에 합쳐도 동작한다 |
| C | shader-visible heap의 모든 슬롯을 먼저 다 채워야 GPU가 CommandList 실행을 시작할 수 있기 때문이다 |
| D | 복사 루프와 드로우 루프는 서로 다른 스레드에서 실행된다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

이론상 같은 루프에 합쳐도 동작한다. 코드상 분리는 **역할 구분**을 위한 것이다:
- 1단계(복사): "이번 드로우에 필요한 모든 Descriptor를 shader-visible heap에 준비"
- 2단계(드로우): "준비된 Descriptor 테이블을 가리키며 면마다 드로우"

순서가 보장되는 한 같은 루프로 합쳐도 결과는 동일하다.

</details>

---

## Q11. `DrawIndexedInstanced`의 첫 번째 인자

`pCommandList->DrawIndexedInstanced(pTriGroup->dwTriCount * 3, 1, 0, 0, 0)`  
첫 번째 인자 `dwTriCount * 3`이 나타내는 것은?

| 보기 | 내용 |
|-----|------|
| A | 버텍스 버퍼에서 읽을 버텍스의 총 개수 |
| B | 처리할 인덱스의 총 개수 (삼각형 1개 = 인덱스 3개) |
| C | 인스턴스 수 × 3 |
| D | 삼각형 1개당 픽셀 수 × 3 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

첫 번째 인자는 `IndexCountPerInstance` — **읽을 인덱스 개수**다.  
삼각형 1개 = 꼭짓점 3개 = 인덱스 3개이므로, `dwTriCount × 3`이 된다.

```
쿼드 1면: dwTriCount=2 → DrawIndexedInstanced(6, ...)  → 인덱스 6개
박스 1면: dwTriCount=2 → DrawIndexedInstanced(6, ...)  → 인덱스 6개
```

</details>

---

## Q12. `INDEXED_TRI_GROUP` 구조체의 역할

`INDEXED_TRI_GROUP`에 포함된 4개의 필드와 그 역할이 올바르게 짝지어진 것은?

| 보기 | 내용 |
|-----|------|
| A | `pVertexBuffer` (버텍스), `IndexBufferView` (인덱스 뷰), `dwTriCount` (삼각형 수), `pTexHandle` (텍스처) |
| B | `pIndexBuffer` (인덱스 버퍼 리소스), `IndexBufferView` (인덱스 뷰), `dwTriCount` (삼각형 수), `pTexHandle` (텍스처) |
| C | `pIndexBuffer` (인덱스 버퍼), `VertexBufferView` (버텍스 뷰), `dwVertexCount` (버텍스 수), `pTexHandle` (텍스처) |
| D | `pConstantBuffer` (상수 버퍼), `IndexBufferView` (인덱스 뷰), `dwTriCount` (삼각형 수), `pSRVHandle` (SRV) |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

```cpp
struct INDEXED_TRI_GROUP
{
    ID3D12Resource*         pIndexBuffer;    // GPU 인덱스 버퍼 리소스
    D3D12_INDEX_BUFFER_VIEW IndexBufferView; // 그 버퍼를 어떻게 읽을지 (포맷/크기)
    DWORD                   dwTriCount;      // 삼각형 개수
    TEXTURE_HANDLE*         pTexHandle;      // 이 면에 붙일 텍스처
};
```

버텍스 버퍼는 오브젝트 전체 공유(`m_pVertexBuffer`)이므로 TriGroup에 없다.

</details>

---

## Q13. `CreateBoxMeshObject`에서 인덱스 배열을 빈 채로 선언하는 이유

```cpp
// CreateBoxMeshObject
WORD pIndexList[36] = {};
DWORD dwVertexCount = CreateBoxMesh(&pVertexList, pIndexList, 36, 0.25f);

// CreateQuadMesh
WORD pIndexList[] = { 0,1,2, 0,2,3 };
```
박스는 빈 배열로 선언하고 쿼드는 직접 값을 쓰는 이유는?

| 보기 | 내용 |
|-----|------|
| A | 박스 인덱스 36개를 코드에 하드코딩하면 가독성이 나빠지기 때문에 함수로 분리했다 |
| B | 박스는 8개 꼭짓점 공유 + UV 중복 제거 후 최종 인덱스가 달라지므로 `CreateBoxMesh()`의 `AddVertex()` 로직이 out parameter로 채워줘야 한다. 쿼드는 4정점이 단순해 직접 작성 가능하다 |
| C | D3D12 API가 박스 형태의 인덱스 배열을 직접 초기화하는 것을 금지하기 때문이다 |
| D | 박스의 36개 인덱스는 런타임 시 사용자 입력으로 결정된다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`CreateBoxMesh()` 내부의 `AddVertex()`는 position+UV 조합이 같으면 기존 인덱스를 재사용한다.  
8개 꼭짓점이 여러 면에서 공유되지만 UV가 면마다 달라 최종 버텍스가 몇 개가 될지, 각 인덱스가 무엇이 될지를 미리 알 수 없다. 그래서 빈 배열을 out parameter로 넘겨 함수가 채우게 한다.

</details>

---

## Q14. `AddVertex()`의 중복 검사 방법

`AddVertex()`가 버텍스 중복 여부를 판단하는 방식은?

| 보기 | 내용 |
|-----|------|
| A | position(x,y,z)만 비교한다 |
| B | position과 texCoord(UV)만 비교한다 |
| C | `memcmp`로 `BasicVertex` 구조체 전체(position + color + texCoord = 36바이트)를 비교한다 |
| D | GPU 버퍼 주소를 해시하여 비교한다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

```cpp
if (!memcmp(pVertexList + i, pVertex, sizeof(BasicVertex)))
    return i;   // 36바이트 전체가 같으면 중복
```

position만 같아도 UV가 다르면 다른 버텍스다. 셰이더가 텍스처 샘플링에 UV를 사용하기 때문이다.

```
박스 pos[3]:
  +z 면 → UV(0,0) → 버텍스 A
  +x 면 → UV(1,0) → 버텍스 B  (position 같지만 UV 달라 → 다른 버텍스)
  +y 면 → UV(1,0) → 버텍스 B  (B와 완전 동일 → 중복 처리)
```

</details>

---

## Q15. 박스 버텍스 최종 개수

`INDEX_COUNT = 36`임에도 `CreateBoxMesh()` 실행 후 `dwBasicVertexCount`가 **20**인 이유는?

| 보기 | 내용 |
|-----|------|
| A | 박스의 꼭짓점은 원래 20개이기 때문이다 |
| B | 면 내부 중복 12개(사각형→삼각형 2개 분할 시 공유 꼭짓점) + 면 간 중복 4개(다른 면에서 position+UV 우연 일치) = 16개 중복으로 `36 - 16 = 20`이다 |
| C | GPU가 자동으로 16개 버텍스를 최적화하여 제거하기 때문이다 |
| D | `BasicVertex` 구조체 크기가 36바이트이므로 36÷(36/20)=20이 된다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

| 중복 종류 | 개수 | 원인 |
|---|---|---|
| 면 내부 | 12개 | 사각형 1면 = 삼각형 2개 → 6인덱스 중 2개 공유. 6면 × 2 = 12 |
| 면 간 | 4개 | `-x면`↔`+y면`, `+x면`↔`-y면` 사이에서 position+UV가 우연히 동일한 꼭짓점 4쌍 |

```
36(총 인덱스) - 12(면 내부 중복) - 4(면 간 중복) = 20개 고유 버텍스
```

</details>

---

## Q16. `GetCurrentBackBufferIndex()` 호출 시점

`m_uiRenderTargetIndex = m_pSwapChain->GetCurrentBackBufferIndex()`에서  
render target이 2개일 때 반환값의 패턴과 올바른 호출 시점은?

| 보기 | 내용 |
|-----|------|
| A | 항상 0을 반환한다. `Present()` 전에 호출해야 한다 |
| B | 0, 1, 0, 1 … 로 교대 반환한다. `Present()` **이후**에 호출해야 다음 프레임의 올바른 인덱스를 얻는다 |
| C | 0, 1, 2, 0, 1, 2 … 로 순환한다. 호출 시점은 무관하다 |
| D | 랜덤하게 반환된다. `BeginRender()` 내부에서만 호출해야 한다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`Present()` 호출마다 스왑체인이 버퍼를 교체하므로 인덱스가 0→1→0→1로 교대한다.  
`Present()` **전**에 호출하면 현재 렌더링 중인 버퍼 인덱스가 반환되고,  
`Present()` **후**에 호출해야 다음 프레임에서 사용할 올바른 인덱스를 얻는다.

</details>
