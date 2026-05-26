# Chapter 11 — OverlappedFrames 객관식 퀴즈

> 아래 퀴즈는 `11_OverlappedFrames` 프로젝트의 실제 코드를 기반으로 출제되었습니다.  
> 각 문제의 보기 중 **정답 하나**를 고르고, 아래 **▶ 정답 및 해설** 을 클릭하면 확인할 수 있습니다.

---

## Q1. Ch11에서 추가된 상수

Ch10에는 없고 Ch11에서 **새롭게 추가된** 상수는?

| 보기 | 내용 |
|-----|------|
| A | `SWAP_CHAIN_FRAME_COUNT = 2` 만 존재 |
| B | `SWAP_CHAIN_FRAME_COUNT = 3`, `MAX_PENDING_FRAME_COUNT = SWAP_CHAIN_FRAME_COUNT - 1` |
| C | `MAX_PENDING_FRAME_COUNT = 3`, `SWAP_CHAIN_FRAME_COUNT = MAX_PENDING_FRAME_COUNT - 1` |
| D | `MAX_PENDING_FRAME_COUNT = 2` 만 추가, SWAP_CHAIN_FRAME_COUNT는 그대로 2 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

Ch11은 트리플 버퍼링을 위해 `SWAP_CHAIN_FRAME_COUNT`를 3으로 늘리고,  
GPU에서 동시에 처리(pending)할 수 있는 최대 프레임 수를 `MAX_PENDING_FRAME_COUNT = SWAP_CHAIN_FRAME_COUNT - 1 = 2`로 정의한다.  
SwapChain 버퍼 하나는 Present 대기용이므로 실제 CPU/GPU가 동시에 쓸 수 있는 슬롯은 항상 1개 적다.

</details>

---

## Q2. 멤버 변수 배열화의 이유

Ch11에서 `m_pCommandAllocator` 하나를 `m_ppCommandAllocator[MAX_PENDING_FRAME_COUNT]` 배열로 바꾼 가장 중요한 이유는?

| 보기 | 내용 |
|-----|------|
| A | 코드 가독성을 위해 배열로 정리했다 |
| B | GPU가 Frame N의 CommandAllocator를 처리 중에 CPU가 같은 Allocator를 Reset하면 메모리 커맨드가 덮어씌워져 GPU 크래시가 발생하므로, 슬롯마다 독립된 Allocator가 필요하다 |
| C | D3D12 API가 CommandAllocator를 반드시 배열로 관리하도록 강제하기 때문이다 |
| D | SwapChain 버퍼 인덱스와 CommandAllocator 인덱스를 동기화하기 위해서이다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`CommandAllocator`는 GPU가 해당 Allocator의 커맨드를 **실행 중인 동안 절대 Reset할 수 없다**.  
Overlapped Frames 환경에서는 Frame N과 Frame N-1이 동시에 in-flight 상태가 될 수 있으므로,  
각 프레임 슬롯마다 독립된 Allocator / CommandList / DescriptorPool / CBPool이 필요하다.

</details>

---

## Q3. `m_pui64LastFenceValue` 배열의 역할

`m_pui64LastFenceValue[MAX_PENDING_FRAME_COUNT]`를 슬롯마다 따로 관리하는 이유는?

| 보기 | 내용 |
|-----|------|
| A | GPU의 총 처리 속도를 측정하기 위한 타이머 값이다 |
| B | 특정 슬롯을 재사용하기 전에 "그 슬롯을 마지막으로 사용한 프레임이 GPU에서 완료됐는지" 선별적으로 확인하기 위해서이다 |
| C | SwapChain Present 순서를 추적하기 위한 시퀀스 번호이다 |
| D | 디버그 레이어가 각 슬롯의 상태를 추적하도록 D3D12가 요구하는 값이다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`m_pui64LastFenceValue[i]`는 슬롯 i가 마지막으로 사용한 Fence 값을 기억한다.  
슬롯 재사용 전에 `WaitForFenceValue(m_pui64LastFenceValue[nextSlot])`을 호출해  
해당 슬롯의 이전 작업만 선택적으로 대기할 수 있다.  
전체 GPU 완료를 기다리지 않고 **필요한 슬롯의 작업만 대기**하는 것이 핵심이다.

</details>

---

## Q4. Ch11 `Fence()` 함수에서 추가된 코드

Ch10의 `Fence()`와 비교했을 때 Ch11에서 **추가된 코드 한 줄**은?

| 보기 | 내용 |
|-----|------|
| A | `m_ui64FenceVaule++;` |
| B | `m_pCommandQueue->Signal(m_pFence, m_ui64FenceVaule);` |
| C | `m_pui64LastFenceValue[m_dwCurContextIndex] = m_ui64FenceVaule;` |
| D | `WaitForFenceValue(m_ui64FenceVaule);` |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

Ch10의 `Fence()`는 카운터 증가 + Signal 두 줄뿐이었다.  
Ch11에서는 Signal 후 **현재 컨텍스트 인덱스(슬롯)에 이 Fence 값을 저장**하는  
`m_pui64LastFenceValue[m_dwCurContextIndex] = m_ui64FenceVaule;`가 추가됐다.  
이 한 줄이 슬롯별 선택적 대기를 가능하게 하는 핵심이다.

</details>

---

## Q5. `WaitForFenceValue` 시그니처 변경

Ch10의 `WaitForFenceValue()`가 Ch11에서 `WaitForFenceValue(UINT64 ExpectedFenceValue)`로 바뀐 이유는?

| 보기 | 내용 |
|-----|------|
| A | D3D12 API 업데이트로 인수가 필수가 됐기 때문이다 |
| B | Ch10은 항상 최신 Fence 값으로 전체 대기했지만, Ch11은 슬롯마다 다른 Fence 값을 선택적으로 지정해야 하기 때문이다 |
| C | UINT64 인수 없이는 컴파일 오류가 발생하기 때문이다 |
| D | 멀티스레드 안전성을 위해 인수를 복사 전달하도록 변경됐다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

Ch10의 `WaitForFenceValue()`는 `m_ui64FenceValue`(항상 최신값)를 내부에서 고정 사용했다.  
Ch11에서는 슬롯 0 재사용 전엔 `WaitForFenceValue(LFV[0])`을, 슬롯 1 재사용 전엔 `WaitForFenceValue(LFV[1])`을  
각각 호출해야 한다. 따라서 대기 목표값을 외부에서 명시적으로 전달하는 설계가 필요하다.

</details>

---

## Q6. Ch11 `Present()` — Fence() 호출 위치

Ch11의 `Present()`에서 `Fence()`를 `m_pSwapChain->Present()` **앞**에 호출하는 이유는?

| 보기 | 내용 |
|-----|------|
| A | Present가 실패할 경우 Fence 값이 오염되는 것을 방지하기 위해서다 |
| B | GPU 커맨드 큐에 "렌더링 완료 → Fence 마커 → Present" 순서를 보장하기 위해서다 |
| C | D3D12의 Present 함수가 내부적으로 Fence를 초기화하기 때문에 먼저 호출해야 한다 |
| D | Fence 번호를 SwapChain 버퍼 인덱스와 동기화하기 위해서다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`Present()` 자체도 GPU 커맨드 큐 기반으로 동작한다.  
`ExecuteCommandLists`로 렌더링 커맨드가 큐에 삽입된 상태이므로,  
Present **전에** Signal을 삽입해야 GPU 타임라인에  
"렌더링 완료 → Fence 마커 삽입 → Present 실행" 순서가 정확히 기록된다.

</details>

---

## Q7. `Present()` 내부 — Wait 대상

Frame 2의 `Present()`에서 `WaitForFenceValue(m_pui64LastFenceValue[dwNextContextIndex])`를 호출할 때,  
실제로 "무엇을 위해" 기다리는가?

| 보기 | 내용 |
|-----|------|
| A | Frame 2의 렌더링이 GPU에서 완료되길 기다린다 |
| B | Frame 3이 사용할 슬롯(슬롯 0)을 안전하게 Reset하기 위해, 슬롯 0을 마지막으로 사용한 Frame 1의 GPU 완료를 확인한다 |
| C | SwapChain이 Back Buffer를 교체하는 동안 CPU가 멈추는 것이다 |
| D | Frame 2의 Present 출력이 모니터에 표시될 때까지 기다린다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

Frame 2의 `Present()` 내부 Wait는 Frame 2를 위한 것이 **아니다**.  
Frame 3이 쓸 슬롯 0(`dwNextContextIndex = 0`)을 Reset하기 전에  
"슬롯 0의 이전 사용자인 Frame 1이 GPU에서 끝났는지"를 확인하는 것이다.  
Frame 2의 렌더링(`BeginRender`~`EndRender`)은 Frame 1과 **완전히 병렬**로 진행되며, 이 Wait와 무관하다.

</details>

---

## Q8. 프레임 4에서의 변수 상태

정상적인 4프레임 진행 후 Frame 4의 `Present()` 완료 직후 상태로 **올바른** 것은?

| 보기 | CurCtx | FenceVal | LFV[0] | LFV[1] |
|-----|:------:|:--------:|:------:|:------:|
| A | 1 | 4 | 3 | 4 |
| B | 0 | 4 | 4 | 2 |
| C | 1 | 3 | 3 | 4 |
| D | 0 | 4 | 4 | 4 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: A**

각 프레임의 진행:

| 프레임 | 사용 슬롯 | Fence() 후 FenceVal | LFV 변화 | nextIdx | CurCtx 갱신 |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 | 0 | 1 | LFV[0]=1 | 1 | 1 |
| 2 | 1 | 2 | LFV[1]=2 | 0 | 0 |
| 3 | 0 | 3 | LFV[0]=3 | 1 | 1 |
| 4 | 1 | 4 | LFV[1]=4 | 0 | 0 |

Frame 4 완료 후: `CurCtx=0`, `FenceVal=4`, `LFV[0]=3`, `LFV[1]=4`

</details>

---

## Q9. `DeleteBasicMeshObject()` 전체 대기 이유

Ch11의 `DeleteBasicMeshObject()`에서 삭제 전에 **모든 슬롯**의 `WaitForFenceValue`를 루프로 호출하는 이유는?

| 보기 | 내용 |
|-----|------|
| A | delete 연산자가 D3D12 리소스에 적합하지 않기 때문에 사전 정리가 필요하다 |
| B | Overlapped 환경에서는 여러 슬롯이 동시에 in-flight일 수 있으므로, 어느 슬롯의 CommandList가 해당 MeshObject를 참조하고 있을지 모른다. 즉시 삭제하면 GPU가 이미 해제된 메모리를 읽는 use-after-free가 발생한다 |
| C | D3D12 Validation Layer가 삭제 전 Flush를 의무로 요구하기 때문이다 |
| D | MeshObject가 내부적으로 CommandList를 소유하고 있어서 Reset이 필요하기 때문이다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

Overlapped Frames 환경에서는 Frame N과 Frame N-1이 동시에 in-flight 상태일 수 있다.  
Frame N-1의 CommandList가 해당 MeshObject의 VertexBuffer나 Texture를 참조 중일 때  
즉시 해제하면 **GPU가 해제된 메모리를 읽는 use-after-free**가 발생한다.  
따라서 삭제 전에 모든 슬롯의 GPU 작업 완료를 확인해야 한다.

</details>

---

## Q10. `BeginRender()`에서 슬롯 선택 방법

Ch11의 `BeginRender()`가 현재 사용할 CommandAllocator와 CommandList를 선택하는 방법은?

| 보기 | 내용 |
|-----|------|
| A | `m_pSwapChain->GetCurrentBackBufferIndex()`로 SwapChain 버퍼 인덱스를 가져와 사용한다 |
| B | 매 프레임 랜덤으로 슬롯을 선택한다 |
| C | `m_ppCommandAllocator[m_dwCurContextIndex]`처럼 `m_dwCurContextIndex`를 인덱스로 사용하며, 이 값은 이전 프레임의 `Present()` 끝에서 업데이트된다 |
| D | Fence 값 홀수/짝수에 따라 슬롯 0/1을 선택한다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

`BeginRender()`는 `m_dwCurContextIndex`를 인덱스로 사용해  
`m_ppCommandAllocator[m_dwCurContextIndex]`와 `m_ppCommandList[m_dwCurContextIndex]`를 선택한다.  
`m_dwCurContextIndex`는 직전 프레임의 `Present()` 마지막에  
`m_dwCurContextIndex = (m_dwCurContextIndex + 1) % MAX_PENDING_FRAME_COUNT`로 갱신되므로,  
`BeginRender()`는 자동으로 올바른 슬롯을 사용하게 된다.

</details>

---

## Q11. `INL_GetDescriptorPool()` 반환값 변경

Ch10의 `INL_GetDescriptorPool()`은 항상 `m_pDescriptorPool`을 반환했다.  
Ch11에서는 어떻게 바뀌었으며, 이로 인해 호출 측(`BasicMeshObject::Draw()` 등) 코드는 어떻게 되는가?

| 보기 | 내용 |
|-----|------|
| A | `m_ppDescriptorPool[0]`을 항상 반환하도록 고정되어, 호출 측도 인덱스를 직접 전달하도록 변경됐다 |
| B | `m_ppDescriptorPool[m_dwCurContextIndex]`를 반환하므로, 호출 측 코드 변경 없이 자동으로 현재 슬롯의 풀을 사용한다 |
| C | 함수가 삭제되고 호출 측이 직접 배열에 접근하도록 변경됐다 |
| D | 슬롯 인덱스를 인수로 받는 `INL_GetDescriptorPool(DWORD idx)`로 시그니처가 변경됐다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

Ch11에서 `INL_GetDescriptorPool()`은 `m_ppDescriptorPool[m_dwCurContextIndex]`를 반환한다.  
현재 컨텍스트 인덱스는 렌더러 내부에서 관리되므로, `BasicMeshObject::Draw()` 등  
Pool을 요청하는 **호출 측 코드는 전혀 수정할 필요가 없다**.  
이 설계 덕분에 Overlapped Frames 도입이 외부 코드에 최소한의 영향만 준다.

</details>

---

## Q12. `UpdateWindowSize()` 전체 대기 이유

Ch11의 `UpdateWindowSize()`에서 ResizeBuffers 전에 `Fence()` + 전체 슬롯 `WaitForFenceValue` 루프를 호출하는 이유는?

| 보기 | 내용 |
|-----|------|
| A | Fence 값을 0으로 리셋하기 위한 준비 단계이다 |
| B | SwapChain 버퍼를 재생성(ResizeBuffers)하려면 해당 버퍼를 참조하는 모든 GPU 커맨드가 완료되어야 하며, Overlapped 환경에서는 여러 슬롯이 in-flight일 수 있으므로 전체 대기가 필요하다 |
| C | 윈도우 크기 변경 시 CommandAllocator를 재생성해야 하기 때문이다 |
| D | D3D12 드라이버가 ResizeBuffers 전에 반드시 CPU 유휴 상태를 요구하기 때문이다 |

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`ResizeBuffers`는 SwapChain 버퍼를 파괴하고 새로 만든다.  
Overlapped 환경에서는 여러 슬롯이 동시에 in-flight 상태일 수 있고,  
그 중 어떤 슬롯의 CommandList가 기존 버퍼를 렌더 타깃으로 참조하고 있을 수 있다.  
`Fence()` 후 모든 슬롯의 `WaitForFenceValue`를 통해 **모든 in-flight 프레임이 완료됨을 보장**한 뒤 버퍼를 재생성한다.

</details>
