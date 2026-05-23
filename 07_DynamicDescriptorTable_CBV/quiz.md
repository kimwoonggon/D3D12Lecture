# 07_DynamicDescriptorTable_CBV 퀴즈

> 각 문제에는 힌트와 정답이 접기 형식(`<details>`)으로 제공된다.

---

## Q1. Shader-Visible Heap vs CPU-Only Heap

다음 중 `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE`을 붙인 heap에 대한 설명으로 **틀린** 것은?

1. GPU 셰이더가 직접 읽을 수 있다.
2. `SetDescriptorHeaps()`에 등록해서 사용할 수 있다.
3. `GetGPUDescriptorHandleForHeapStart()`를 호출할 수 있다.
4. CPU-only heap보다 CPU 쓰기 속도가 빠르다.
5. GPU 주소 공간에 매핑되어 추가 비용이 발생한다.

<details>
<summary>정답 및 해설</summary>

**정답: 4번**

`FLAG_SHADER_VISIBLE`을 붙이면 GPU 주소 공간에 매핑되기 때문에 오히려 CPU 쓰기 속도가 **느려진다**.  
CPU 쓰기가 빠른 것은 `FLAG_NONE`(CPU-only) heap이다.  
그래서 이 예제에서는 descriptor를 평소에 CPU-only heap(`CBPool::m_pCBVHeap`)에 보관하다가, Draw 직전에 `CopyDescriptorsSimple`로 shader-visible pool에 복사하는 구조를 사용한다.

</details>

---

## Q2. CopyDescriptorsSimple의 역할

아래 코드에서 `CopyDescriptorsSimple`이 하는 일을 한 문장으로 설명하고,  
`pCB->CBVHandle`과 `cbvDest`가 각각 어느 heap에 속하는지 답하라.

```cpp
pD3DDevice->CopyDescriptorsSimple(
    1,
    cbvDest,          // 목적지
    pCB->CBVHandle,   // 원본
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
);
```

<details>
<summary>정답 및 해설</summary>

**한 문장 설명:**  
CPU-only heap에 미리 만들어 둔 CBV descriptor를 shader-visible pool의 이번 Draw 전용 슬롯으로 복사한다.

| 핸들 | 소속 heap | Flags |
|------|-----------|-------|
| `pCB->CBVHandle` | `CSimpleConstantBufferPool::m_pCBVHeap` | `FLAG_NONE` (CPU-only) |
| `cbvDest` | `CDescriptorPool::m_pDescritorHeap` | `FLAG_SHADER_VISIBLE` |

CPU는 non-visible heap에만 안전하게 쓸 수 있고, GPU 셰이더는 shader-visible heap만 읽을 수 있기 때문에 이 복사 단계가 반드시 필요하다.

</details>

---

## Q3. AllocDescriptorTable 내부 동작

`CDescriptorPool::AllocDescriptorTable()`은 내부적으로 어떤 작업을 수행하는가?  
아래 보기에서 **실제로 일어나는 일**을 모두 고르라.

1. 새로운 `ID3D12DescriptorHeap`을 `CreateDescriptorHeap`으로 생성한다.
2. `m_cpuDescriptorHandle`에 오프셋을 더해 CPU 핸들을 계산한다.
3. `m_gpuDescriptorHandle`에 오프셋을 더해 GPU 핸들을 계산한다.
4. `m_AllocatedDescriptorCount`를 `DescriptorCount`만큼 증가시킨다.
5. Upload Buffer에 메모리를 `malloc`으로 새로 할당한다.

<details>
<summary>정답 및 해설</summary>

**정답: 2, 3, 4번**

`AllocDescriptorTable`은 `new`/`malloc`/`CreateDescriptorHeap` 없이, 이미 Initialize에서 통째로 생성된 heap 내에서 **포인터 계산(오프셋 이동)과 카운터 증가**만 수행한다.

```
*pOutCPUDescriptor = m_cpuDescriptorHandle + (N × srvDescriptorSize);  // 2
*pOutGPUDescriptor = m_gpuDescriptorHandle + (N × srvDescriptorSize);  // 3
m_AllocatedDescriptorCount += DescriptorCount;                          // 4
```

이처럼 실제 메모리 할당 없이 카운터만 이동시키는 방식을 **선형 할당자(Linear Allocator)** 패턴이라고 한다.

</details>

---

## Q4. Reset() 타이밍의 중요성

`Present()` 내부에서 아래 순서로 코드가 실행된다.

```cpp
Fence();                        // (A)
WaitForFenceValue();            // (B)
m_pConstantBufferPool->Reset(); // (C)
m_pDescriptorPool->Reset();     // (D)
```

만약 **(B) 없이 (A) 직후 바로 (C)(D)를 실행**하면 어떤 문제가 발생하는가?

<details>
<summary>정답 및 해설</summary>

**GPU가 아직 읽고 있는 슬롯을 CPU가 덮어쓰는 Race Condition이 발생한다.**

```
GPU: DescriptorPool 슬롯[0]에서 CBV 읽는 중...
CPU: Reset() → m_AllocatedDescriptorCount = 0
CPU: 다음 프레임 Draw() → 슬롯[0]에 새 값 덮어씀  ← 버그!
```

`WaitForFenceValue()`는 CPU가 GPU의 작업 완료를 기다리는 동기화 포인트다.  
GPU가 현재 프레임의 모든 명령을 처리 완료한 뒤에만 Reset을 호출해야  
이전 프레임 데이터가 완전히 소비된 상태에서 안전하게 재사용할 수 있다.

</details>

---

## Q5. pSystemMemAddr vs CBVHandle

`CB_CONTAINER` 구조체에는 같은 Upload Buffer 슬롯을 가리키는 두 멤버가 있다.

```cpp
struct CB_CONTAINER {
    D3D12_CPU_DESCRIPTOR_HANDLE CBVHandle;
    D3D12_GPU_VIRTUAL_ADDRESS   pGPUMemAddr;
    UINT8*                      pSystemMemAddr;
};
```

`pSystemMemAddr`에 `offset.x`, `offset.y`를 쓰면 데이터는 GPU 메모리에 즉시 반영된다.  
그렇다면 **`CBVHandle`은 왜 별도로 필요한가?**

<details>
<summary>정답 및 해설</summary>

`pSystemMemAddr`은 CPU가 **데이터를 쓰는 통로**이고,  
`CBVHandle`은 GPU가 **그 데이터를 찾아오는 주소록**이다.

GPU 셰이더가 상수 버퍼를 읽으려면 Root Signature가 정의한 descriptor table 체계를 통해야 한다.  
즉, `"b0 레지스터에는 descriptor table의 N번 슬롯을 봐라"` → `"슬롯 N에는 이 CBV가 있어"` → `"CBV가 가리키는 Upload Buffer 주소에 데이터가 있어"` 의 경로가 필요하다.

`pSystemMemAddr`로 데이터를 써도, `CBVHandle`→`CopyDescriptorsSimple`→`SetGraphicsRootDescriptorTable` 경로가 없으면 셰이더는 그 데이터를 찾지 못한다.

만약 `SetGraphicsRootConstantBufferView()`(inline root descriptor 방식)를 사용한다면 CBV descriptor heap 자체가 불필요해진다. 이 예제는 **descriptor table 방식**이라서 이 경로가 필요한 것이다.

</details>

---

## Q6. 왜 Draw마다 새 슬롯을 쓰는가

07 예제에서는 같은 `CBasicMeshObject` 하나를 프레임마다 **2회** Draw한다.  
만약 `CDescriptorPool`이 없고 슬롯 하나를 재사용한다면 어떤 버그가 발생하는가?  
Draw 순서에 따라 구체적으로 서술하라.

<details>
<summary>정답 및 해설</summary>

```
Draw#1 기록: 슬롯[0]에 CBV_A (offset.x = 0.5) 복사
Draw#2 기록: 슬롯[0]에 CBV_B (offset.y = 0.3) 덮어씀

→ GPU 실행 시
  Draw#1도 슬롯[0]을 읽음 = CBV_B (offset.y = 0.3)
  Draw#2도 슬롯[0]을 읽음 = CBV_B (offset.y = 0.3)

결과: 두 삼각형이 같은 위치에 그려짐
```

명령 리스트(Command List)는 **기록(record)** 단계와 **실행(execute)** 단계가 분리되어 있다.  
기록 도중 슬롯[0]을 덮어쓰면, 나중에 GPU가 실행할 때는 마지막으로 기록된 값만 남아있게 된다.  
따라서 Draw마다 고유한 슬롯을 할당해야 각 Draw가 서로 다른 상수값을 독립적으로 참조할 수 있다.

</details>

---

## Q7. CDescriptorPool vs CSimpleConstantBufferPool 역할 구분

아래 표의 빈칸을 채워라.

| 질문 | CSimpleConstantBufferPool | CDescriptorPool |
|------|--------------------------|-----------------|
| 관리하는 것 | (①) | (②) |
| Heap Flags | (③) | (④) |
| GPU가 직접 접근? | (⑤) | (⑥) |
| Draw 후 어떻게 재사용? | (⑦) | (⑦) |

<details>
<summary>정답</summary>

| | CSimpleConstantBufferPool | CDescriptorPool |
|---|---|---|
| ① | Upload Buffer(실제 데이터) + CPU-only heap(CBV 원본) | shader-visible heap 슬롯 |
| ② | (좌측 참고) | |
| ③ | `D3D12_DESCRIPTOR_HEAP_FLAG_NONE` | `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE` |
| ⑤ | 불가 | 가능 |
| ⑦ | 프레임 끝 `WaitForFenceValue()` 후 `Reset()`으로 카운터만 0으로 초기화 |

> **핵심:** `CSimpleConstantBufferPool`은 "무슨 데이터인지"를 갖고,  
> `CDescriptorPool`은 "GPU에게 보여줄 자리"를 제공한다.  
> `CopyDescriptorsSimple`이 두 풀을 연결하는 유일한 다리다.

</details>

---

## Q8. CBPool에도 Descriptor Heap이 있는 이유

`CSimpleConstantBufferPool`은 실제 데이터 저장용 Upload Buffer 외에,  
**CPU-only `m_pCBVHeap`도 가지고 있다.**  
이 heap이 왜 필요한지, 그리고 언제 생성되는지 설명하라.

<details>
<summary>정답 및 해설</summary>

**이유:** `CopyDescriptorsSimple`의 **원본(source)** 핸들이 반드시 descriptor heap의 CPU 핸들이어야 하기 때문이다.

`CreateConstantBufferView()`는 descriptor를 descriptor heap의 특정 슬롯에 기록하며,  
그 슬롯의 CPU 핸들(`D3D12_CPU_DESCRIPTOR_HANDLE`)이 `CBVHandle`이 된다.  
이 핸들을 `CopyDescriptorsSimple`의 `SrcDescriptorRangeStart`로 전달하여  
shader-visible pool로 복사한다.

**생성 시점:** `CSimpleConstantBufferPool::Initialize()` 내부 루프에서 **딱 1회** 모든 슬롯에 대해 미리 생성된다.

```cpp
// Initialize() 내부 — Draw마다 실행되는 것이 아님
for (DWORD i = 0; i < m_MaxCBVNum; i++) {
    pD3DDevice->CreateConstantBufferView(&cbvDesc, heapHandle);
    m_pCBContainerList[i].CBVHandle = heapHandle;  // 원본 보관
}
```

Draw 시점에는 `CreateConstantBufferView`를 다시 호출하지 않고,  
이미 만들어진 원본을 꺼내 shader-visible pool로 복사(`CopyDescriptorsSimple`)한다.  
이것이 초기화 비용을 Draw 루프 밖으로 끌어내는 **핵심 성능 설계**다.

</details>

---

## Q9. gpuDescriptorTable 연결 경로 추적

`SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)`에서 전달하는  
`gpuDescriptorTable`이 생성되는 경로를 순서대로 나열하라.

> 힌트: `m_pDescritorHeap` → … → `gpuDescriptorTable`

<details>
<summary>정답 및 해설</summary>

```
1. CDescriptorPool::Initialize()
   └─ CreateDescriptorHeap(FLAG_SHADER_VISIBLE) → m_pDescritorHeap

2. m_pDescritorHeap->GetGPUDescriptorHandleForHeapStart()
   └─ m_gpuDescriptorHandle  (heap 전체의 GPU 시작 주소 캐시)

3. CDescriptorPool::AllocDescriptorTable() 호출 시
   └─ *pOutGPUDescriptor = m_gpuDescriptorHandle + (N × srvDescriptorSize)
      └─ gpuDescriptorTable  (N번째 슬롯의 GPU 주소)

4. Draw() 내부
   ├─ SetDescriptorHeaps(1, &m_pDescritorHeap)
   │   → GPU에게 "이 heap을 사용하겠다" 선언
   └─ SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)
       → Root Signature slot[0]에 "슬롯 N의 GPU 주소를 바인딩"
```

`SetDescriptorHeaps`와 `SetGraphicsRootDescriptorTable`을 **모두** 호출해야 한다.  
전자는 heap 등록, 후자는 그 heap 내의 특정 슬롯을 Root Parameter에 연결하는 역할이다.

</details>

---

## Q10. DESCRIPTOR_COUNT_FOR_DRAW와 Pool 크기의 관계

`CDescriptorPool`을 초기화할 때 아래 코드로 최대 슬롯 수를 결정한다.

```cpp
m_pDescriptorPool->Initialize(
    m_pD3DDevice,
    MAX_DRAW_COUNT_PER_FRAME * CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW
);
```

07 예제에서 `MAX_DRAW_COUNT_PER_FRAME = 256`, `DESCRIPTOR_COUNT_FOR_DRAW = 1`일 때:

1. `CDescriptorPool`의 총 슬롯 수는 얼마인가?
2. 08 예제에서 `DESCRIPTOR_COUNT_FOR_DRAW = 2`(CBV + SRV)가 되면 총 슬롯 수는?
3. `DESCRIPTOR_COUNT_FOR_DRAW`를 `public`으로 선언한 이유는 무엇인가?

<details>
<summary>정답 및 해설</summary>

1. **256개** (= 256 × 1)

2. **512개** (= 256 × 2)  
   Draw당 CBV 1개 + SRV 1개 = 2개 슬롯이 필요하므로 pool 크기가 2배가 된다.

3. `D3D12Renderer::Initialize()`에서 `CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW`를  
   **클래스 외부(렌더러)에서 직접 참조**하여 Pool 초기화 인자로 사용하기 때문이다.  
   만약 `private`이면 렌더러가 이 값에 접근할 수 없어 매직 넘버(1, 2 등)를 하드코딩해야 한다.  
   `public static const`로 선언함으로써 draw 당 필요 descriptor 수를 **단일 출처(single source of truth)** 로 관리한다.

</details>

---

## 보너스: 전체 흐름 재구성

아래 단계를 올바른 실행 순서로 정렬하라.

- (A) `DrawInstanced()` — GPU 명령 기록
- (B) `CopyDescriptorsSimple()` — CPU-only → shader-visible 복사
- (C) `WaitForFenceValue()` — GPU 완료 대기
- (D) `pConstantBufferPool->Alloc()` — CB 슬롯 할당 및 offset 값 기록
- (E) `pDescriptorPool->Reset()` — 카운터 0으로 초기화
- (F) `AllocDescriptorTable()` — shader-visible 슬롯 예약
- (G) `SetGraphicsRootDescriptorTable()` — GPU 핸들 바인딩
- (H) `Fence()` — GPU에게 완료 신호 요청

<details>
<summary>정답</summary>

**F → D → B → G → A → H → C → E**

| 단계 | 위치 | 설명 |
|------|------|------|
| F | Draw() | shader-visible 슬롯 주소 확보 |
| D | Draw() | CBV 슬롯 할당 + offset 데이터 기록 |
| B | Draw() | non-visible CBV → visible pool 복사 |
| G | Draw() | GPU에 슬롯 주소 전달 |
| A | Draw() | 실제 Draw 명령 기록 |
| H | Present() | GPU에 Fence 신호 요청 |
| C | Present() | GPU 작업 완료까지 CPU 대기 |
| E | Present() | 다음 프레임을 위한 pool 초기화 |

</details>
