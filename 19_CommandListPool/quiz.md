# Chapter 19 — CommandListPool 퀴즈 (객관식)

> 각 문제의 **정답 보기 ▼** 를 클릭하면 정답이 나옵니다.

---

## Q1. Chapter 19에서 새로 도입된 핵심 아키텍처는?

① 멀티 GPU 지원  
② CommandList를 매 프레임 new/delete하지 않고 풀(Pool)에서 재사용하며, 한 프레임에 여러 CommandList를 만들어 `ExecuteCommandLists(N)`로 한 번에 제출하는 구조  
③ DescriptorHeap을 셰이더 가시성 기반으로 분리  
④ 비동기 컴퓨트 큐(Compute Queue) 도입  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

ch18까지는 프레임당 CommandList 1개로 모든 Draw를 기록했다.  
ch19에서는 `CCommandListPool`이 여러 개의 `(CommandAllocator, CommandList)` 쌍을 관리하고, `RenderQueue::Process()`가 400개 단위로 끊어 여러 CommandList에 분산 기록한 뒤 `ExecuteCommandLists(N, ...)`로 한 번에 제출한다.  
ch19 자체는 싱글스레드지만, ch20 멀티스레드 기록을 위한 구조적 토대다.

</details></td></tr></table>

---

## Q2. `CCommandListPool::Initialize()`가 호출된 직후 Pool의 상태는?

① CommandList 256개가 미리 생성되어 Available 리스트에 들어있다  
② CommandList 1개만 생성되어 있다  
③ `m_dwMaxCmdListNum`만 저장되어 있고 DX12 객체는 하나도 만들어지지 않은 상태다  
④ 멤버 변수들이 무작위 값으로 초기화된 상태다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
BOOL CCommandListPool::Initialize(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type, DWORD dwMaxCommandListNum)
{
    m_dwMaxCmdListNum = dwMaxCommandListNum;  // 256 저장만
    m_pD3DDevice = pDevice;
    return TRUE;
}
```

`CreateCommandAllocator`, `CreateCommandList` 모두 호출하지 않는다.  
실제 생성은 최초 `GetCurrentCommandList()` → `AllocCmdList()` → `AddCmdList()` 호출 시점에 발생한다(lazy 생성).

</details></td></tr></table>

---

## Q3. `m_ppCommandListPool[MAX_PENDING_FRAME_COUNT]`에서 `MAX_PENDING_FRAME_COUNT = 2`로 Pool을 2개 두는 이유는?

① SwapChain 백버퍼 개수가 2개라서  
② DIRECT Queue와 COPY Queue 두 개를 각각 관리하기 위해  
③ CPU가 다음 프레임을 기록하는 동안 GPU는 이전 프레임을 실행 중이므로, GPU가 사용 중인 Pool은 건드리지 않고 반대편 Pool을 쓰기 위해 (더블 버퍼링)  
④ 단일 Pool은 멀티스레드 안전성이 없어서 2개가 필요  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```
프레임 0:  [CPU: Pool[0] 기록]  [GPU: Pool[0] 실행────────]
프레임 1:        [CPU: Pool[1] 기록]  [GPU: Pool[1] 실행────]
프레임 2:  Pool[0].Reset() → [CPU: Pool[0] 재기록]
```

`Reset()`은 CommandAllocator를 재활용하는 작업인데, GPU가 그 Allocator의 메모리를 아직 읽고 있으면 미정의 동작이다.  
그래서 "지금 쓸 Pool"과 "다음에 쓸 Pool"을 분리해야 하고, 다음에 쓸 Pool은 `WaitForFenceValue`로 GPU 완료를 확인한 후에만 `Reset()` 한다.

`SWAP_CHAIN_FRAME_COUNT = 3`인데 `MAX_PENDING_FRAME_COUNT = 3 - 1 = 2`인 이유는, CPU가 GPU보다 한 프레임 앞서 가는 것까지만 허용하기 위함이다.

</details></td></tr></table>

---

## Q4. 다음 중 `CCommandListPool`의 멤버 변수가 **아닌** 것은?

① `m_pCurCmdList`  
② `m_pAvailableCmdLinkHead`, `m_pAlloatedCmdLinkHead`  
③ `m_dwMaxCmdListNum`, `m_dwTotalCmdNum`  
④ `m_pui64LastFenceValue`  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ④**

`m_pui64LastFenceValue`는 `CD3D12Renderer`의 멤버다 (`UINT64 m_pui64LastFenceValue[MAX_PENDING_FRAME_COUNT]`).  
Pool은 펜스 값을 직접 알지 못한다. 펜스 동기화는 외부(Renderer)가 책임지고 Pool은 `Reset()` 호출만 받는 분업 구조다.

Pool의 실체:
```cpp
COMMAND_LIST* m_pCurCmdList;
SORT_LINK*    m_pAvailableCmdLinkHead/Tail;
SORT_LINK*    m_pAlloatedCmdLinkHead/Tail;
DWORD         m_dwMaxCmdListNum, m_dwTotalCmdNum, m_dwAvailableCmdNum, m_dwAllocatedCmdNum;
ID3D12Device* m_pD3DDevice;
D3D12_COMMAND_LIST_TYPE m_CommnadListType;
```

</details></td></tr></table>

---

## Q5. `AddCmdList()`와 `AllocCmdList()`의 역할 차이는?

① 둘 다 같은 함수의 별칭이다  
② `AddCmdList()` = DX12 객체 생성 + Available에 추가 / `AllocCmdList()` = Available → Allocated 이동  
③ `AddCmdList()` = Allocated에 추가 / `AllocCmdList()` = Available에서 제거  
④ `AddCmdList()` = 풀 확장 / `AllocCmdList()` = 풀 축소  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
BOOL AddCmdList() {
    CreateCommandAllocator(...);
    CreateCommandList(...);
    new COMMAND_LIST;
    LinkFIFO(Available, ...);   // Available에 추가
    m_dwTotalCmdNum++;
}

COMMAND_LIST* AllocCmdList() {
    if (Available 비어있음) AddCmdList();
    UnLink(Available, ...);     // Available에서 제거
    LinkFIFO(Allocated, ...);   // Allocated로 이동
    return pCmdList;
}
```

`AddCmdList()`는 **만들기**, `AllocCmdList()`는 **꺼내기**다. 둘은 호출 순서로 연결되며, Available에 재고가 있으면 `AddCmdList()`는 건너뛰고 바로 꺼낸다.

</details></td></tr></table>

---

## Q6. `GetCurrentCommandList()`를 연속으로 두 번 호출하면?

① 매번 다른 CommandList가 반환된다  
② 두 번째 호출은 `nullptr`을 반환한다  
③ `m_pCurCmdList`가 nullptr이 아니므로 같은 CommandList가 반환된다  
④ Pool이 자동으로 두 개를 묶어 반환한다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
ID3D12GraphicsCommandList* GetCurrentCommandList() {
    if (!m_pCurCmdList)
        m_pCurCmdList = AllocCmdList();
    return m_pCurCmdList->pDirectCommandList;
}
```

`m_pCurCmdList`는 한 번 채워지면 `Close()` 또는 `CloseAndExecute()`가 호출되어 nullptr이 될 때까지 유지된다.  
실제로 `EndRender()`에는 `pCommandListPool->GetCurrentCommandList()`가 두 줄 연속으로 호출되는데, 두 번째 호출은 같은 포인터를 반환한다(코드상의 사실상 무해한 중복).

</details></td></tr></table>

---

## Q7. `Close()`와 `CloseAndExecute()`의 차이는?

① 동일한 함수다  
② `Close()`는 닫기만 함 (Execute는 외부에서 처리), `CloseAndExecute()`는 닫자마자 `ExecuteCommandLists(1, ...)`까지 호출  
③ `Close()`는 GPU 동기화, `CloseAndExecute()`는 CPU 동기화  
④ `Close()`는 동기 호출, `CloseAndExecute()`는 비동기 호출  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
void Close() {
    m_pCurCmdList->pDirectCommandList->Close();
    m_pCurCmdList->bClosed = TRUE;
    m_pCurCmdList = nullptr;  // ← Execute는 안 함
}

void CloseAndExecute(ID3D12CommandQueue* pQ) {
    m_pCurCmdList->pDirectCommandList->Close();
    m_pCurCmdList->bClosed = TRUE;
    pQ->ExecuteCommandLists(1, ...);  // ← 1개만 즉시 실행
    m_pCurCmdList = nullptr;
}
```

`RenderQueue::Process()`는 N개를 모아 한 번에 `ExecuteCommandLists(N)`을 호출하기 위해 `Close()`만 쓴다.  
반면 BeginRender의 Clear, EndRender 마지막의 Present용 Barrier는 단독 CommandList이므로 `CloseAndExecute()`를 쓴다.

</details></td></tr></table>

---

## Q8. `RenderQueue::Process()`에서 `dwProcessCountPerCommandList = 400`의 의미는?

① 한 프레임에 그릴 수 있는 오브젝트 최대 개수  
② CommandList 1개에 400개의 Draw를 기록한 후 `Close()`하고 다음 CommandList로 넘어가는 분할 단위  
③ CommandQueue가 한 번에 처리할 수 있는 명령 개수  
④ DescriptorPool 슬롯 개수  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
if (dwProcessedCountPerCommandList > dwProcessCountPerCommandList)  // 400 초과
{
    pCommandListPool->Close();
    ppCommandList[count++] = pCommandList;
    dwProcessedCountPerCommandList = 0;
    // 다음 루프에서 GetCurrentCommandList() → 새 CL 꺼냄
}
```

오브젝트 1200개라면 CommandList 3개(400, 400, 400)에 분산 기록된다.  
이렇게 나누는 이유는 ch20에서 각 묶음을 다른 스레드가 동시에 기록하기 위한 준비다.

</details></td></tr></table>

---

## Q9. `EndRender()`에서 RenderQueue 처리가 끝난 뒤 Present용 CommandList를 다시 꺼내는 이유는?

① 두 번째 SwapChain 백버퍼를 위해  
② RenderTarget 리소스 상태를 `RENDER_TARGET → PRESENT`로 바꾸는 `ResourceBarrier` 명령을 GPU에 보내려면 별도의 CommandList가 필요해서  
③ Fence 신호를 보내기 위해  
④ 디버그 정보를 출력하기 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
m_pRenderQueue->Process(...);  // CL_0~CL_N 제출, 마지막 상태 = RENDER_TARGET

// Present 전 PRESENT 상태로 전환 필요
pCommandList = pCommandListPool->GetCurrentCommandList();
pCommandList->ResourceBarrier(... RENDER_TARGET → PRESENT ...);
pCommandListPool->CloseAndExecute(m_pCommandQueue);
```

GPU에게 어떤 명령(ResourceBarrier 포함)을 보내려면 반드시 CommandList가 필요하다.  
이미 제출한 CL_0~CL_N에는 더 이상 명령을 추가할 수 없으므로(`Close`된 상태), 새 CommandList 1개를 꺼내 Barrier만 기록하고 Submit한다.

</details></td></tr></table>

---

## Q10. Present용 CommandList는 어디서 가져오는가?

① 별도의 전용 CommandListPool에서  
② Draw용 CommandList들과 같은 `m_ppCommandListPool[m_dwCurContextIndex]`의 Available 리스트에서  
③ Renderer가 멤버로 들고 있는 고정 CommandList 1개  
④ 매번 `new CommandList`로 새로 생성  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
void CD3D12Renderer::EndRender()
{
    CCommandListPool* pCommandListPool = m_ppCommandListPool[m_dwCurContextIndex];
    m_pRenderQueue->Process(pCommandListPool, ...);   // Draw용
    ...
    pCommandList = pCommandListPool->GetCurrentCommandList();  // ← 같은 Pool
    pCommandListPool->CloseAndExecute(...);
}
```

같은 프레임의 모든 CommandList(Clear용, Draw용, Present용)는 동일한 Pool의 Available에서 순서대로 꺼내진다.  
Pool은 용도에 관계없이 단순한 창고 역할만 한다.

</details></td></tr></table>

---

## Q11. 같은 `ID3D12CommandQueue`에 `ExecuteCommandLists`로 여러 CommandList를 제출하면 GPU 실행 순서는?

① GPU가 알아서 병렬로 동시에 실행  
② 제출 순서와 무관하게 GPU가 스케줄링  
③ 같은 Queue에 들어간 CommandList는 항상 제출 순서대로 순차 실행 (순서 보장)  
④ Fence를 걸지 않으면 순서가 보장되지 않음  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

D3D12 사양상 **같은 Queue 내의 작업은 strict ordering**이다.  
`ExecuteCommandLists(3, [CL_0, CL_1, CL_2])`는 CL_0 → CL_1 → CL_2 순서로 실행되고 완료된다.

GPU를 진짜 병렬로 일하게 하려면 별도 Queue(DIRECT / COMPUTE / COPY)를 사용해야 하며 Fence로 동기화가 필요하다.  
ch19의 멀티 CommandList는 GPU 병렬화가 아니라 **CPU 기록 병렬화(ch20)**를 위한 준비다.

</details></td></tr></table>

---

## Q12. `Reset()`을 호출하는 안전한 시점은?

① CommandList를 `Close()`한 직후 즉시  
② `ExecuteCommandLists()` 호출 직후 즉시  
③ 해당 Pool의 CommandList들을 GPU가 모두 실행 완료한 후 (Fence로 확인)  
④ SwapChain의 `Present()` 호출 직전  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
// Present() 내부
DWORD dwNextContextIndex = (m_dwCurContextIndex + 1) % MAX_PENDING_FRAME_COUNT;
WaitForFenceValue(m_pui64LastFenceValue[dwNextContextIndex]);  // ← GPU 완료 대기

m_ppCommandListPool[dwNextContextIndex]->Reset();  // ← 그 후에 Reset
```

`CommandAllocator::Reset()`은 GPU가 해당 Allocator의 메모리를 더 이상 읽지 않는다는 보장이 필요하다.  
Submit하자마자 Reset하면 GPU는 아직 명령을 읽고 있을 수 있어서 미정의 동작/크래시가 발생한다. Fence로 완료를 확인한 후에만 Reset해야 한다.

</details></td></tr></table>

---

## Q13. Pool[0]을 `Reset()`하는 시점은 정확히 언제인가?

① Pool[0]을 사용한 그 프레임의 `EndRender()` 끝에서  
② Pool[0]을 사용한 그 프레임의 `Present()` 끝에서  
③ Pool[1]을 사용한 다음 프레임의 `Present()`에서, nextIndex가 0으로 토글된 직후  
④ Pool은 Reset되지 않고 매 프레임 새로 생성된다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
void Present() {
    Fence();  // 방금 끝낸 프레임(curIndex)의 펜스값 기록
    SwapChain->Present(...);

    DWORD dwNextContextIndex = (m_dwCurContextIndex + 1) % MAX_PENDING_FRAME_COUNT;
    WaitForFenceValue(m_pui64LastFenceValue[dwNextContextIndex]);

    // 다음에 쓸 Pool을 Reset (자기가 방금 쓴 Pool은 안 건드림)
    m_ppCommandListPool[dwNextContextIndex]->Reset();
    m_ppDescriptorPool[dwNextContextIndex]->Reset();
    m_ppConstBufferManager[dwNextContextIndex]->Reset();

    m_dwCurContextIndex = dwNextContextIndex;
}
```

"방금 쓴 Pool은 GPU가 아직 실행 중이라 건드리지 않고, **다음에 쓸 Pool**을 미리 Reset한다"는 패턴이다.  
Pool[0]은 Pool[1]을 쓴 프레임 끝의 Present()에서 Reset된다.

</details></td></tr></table>

---

## Q14. `Reset()`이 호출되면 CommandList 객체에 어떤 일이 일어나는가?

① `Release()`되어 메모리 해제됨  
② Allocator와 List만 `Reset()`이 호출되고, Allocated 리스트에서 Available 리스트로 이동 (객체 자체는 살아있음)  
③ Available 리스트에서 영구히 제거됨  
④ GPU 메모리만 해제되고 CPU 객체는 그대로  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
void CCommandListPool::Reset() {
    while (m_pAlloatedCmdLinkHead) {
        COMMAND_LIST* pCmdList = (COMMAND_LIST*)m_pAlloatedCmdLinkHead->pItem;

        pCmdList->pDirectCommandAllocator->Reset();      // GPU 메모리 재활용
        pCmdList->pDirectCommandList->Reset(pCmdList->pDirectCommandAllocator, nullptr);
        pCmdList->bClosed = FALSE;

        UnLink(Allocated, ...);
        LinkFIFO(Available, ...);  // Allocated → Available로 이동
    }
}
```

객체는 살아있고 단지 "비어있는 상태"로 되돌아가 다음 프레임에 재사용된다.  
실제 `Release()`/`delete`는 `Cleanup()`(소멸자에서 호출)에서만 한다.

</details></td></tr></table>

---

## Q15. CommandList가 1200개 Draw를 기록하는데 400개씩 끊는다고 가정할 때, 프레임 1에서 `AddCmdList()`가 호출되는 횟수는?

① 0번 (Initialize에서 이미 생성됨)  
② 1번  
③ 3번 (CL-A, CL-B, CL-C 각각 생성)  
④ 1200번 (Draw마다)  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```
프레임 1:
  GetCurrentCommandList() → Available 비어있음 → AddCmdList() → CL-A 생성  [1회]
  Draw 1~400, Close()
  GetCurrentCommandList() → Available 비어있음 → AddCmdList() → CL-B 생성  [2회]
  Draw 401~800, Close()
  GetCurrentCommandList() → Available 비어있음 → AddCmdList() → CL-C 생성  [3회]
  Draw 801~1200, Close()
```

Initialize는 아무것도 안 만들고, 첫 프레임에 필요한 만큼만 lazy 생성된다.  
프레임 2부터는 Reset으로 복귀된 CL-A/B/C를 재사용하므로 `AddCmdList()`가 0번 호출된다.

(실제로는 BeginRender의 Clear용 + EndRender의 Present용 Barrier를 합치면 프레임 1 총 생성 개수는 5개가 된다)

</details></td></tr></table>

---

## Q16. 다음 중 `COMMAND_LIST` 구조체의 멤버가 **아닌** 것은?

① `ID3D12CommandAllocator* pDirectCommandAllocator`  
② `ID3D12GraphicsCommandList* pDirectCommandList`  
③ `SORT_LINK Link`  
④ `UINT64 ui64FenceValue`  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ④**

```cpp
struct COMMAND_LIST {
    ID3D12CommandAllocator*    pDirectCommandAllocator;
    ID3D12GraphicsCommandList* pDirectCommandList;
    SORT_LINK  Link;
    BOOL bClosed;
};
```

각 COMMAND_LIST는 자기 자신의 펜스 값을 모른다. 펜스는 프레임 단위로 Renderer가 `m_pui64LastFenceValue[프레임인덱스]`에 저장한다.  
즉 **CommandList별 동기화가 아니라 Pool 전체(=프레임) 단위 동기화**라는 설계다.

</details></td></tr></table>

---

## Q17. `CommandAllocator`와 `CommandList`가 1:1로 묶여있는 이유는?

① D3D12 API가 1:1 매핑을 강제하기 때문에  
② `CommandAllocator::Reset()`은 거기서 할당된 모든 CommandList가 GPU에서 실행 완료된 상태여야만 안전하므로, 추적 단순화를 위해 1:1로 묶은 것 (실제로는 1:N도 가능)  
③ 멀티 GPU에서 동작하게 하려면 1:1이 필요  
④ DXGI 1.6 사양 변경으로 1:1만 허용됨  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

D3D12에서 하나의 `CommandAllocator`는 여러 `CommandList`에 백업 메모리를 제공할 수 있다(1:N 가능).  
그러나 `Allocator::Reset()`을 호출하려면 그 Allocator로 만든 **모든** CommandList가 GPU 실행 완료 상태여야 한다.

CommandList마다 별도 Allocator를 두면 추적이 단순해진다(이 CL이 끝났는가? → 이 Allocator를 Reset해도 되는가?). ch19의 Pool은 이 단순화를 위해 1:1 묶음을 사용한다.

</details></td></tr></table>

---

## Q18. `m_pCurCmdList`가 `nullptr`이 되는 경우는?

① `Pool` 생성 직후  
② `Close()` 또는 `CloseAndExecute()` 호출 후  
③ `Reset()` 호출 후  
④ 위 모두  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ④**

- 생성 직후: 클래스 기본 초기화로 `nullptr`
- `Close()` / `CloseAndExecute()` 마지막 줄: `m_pCurCmdList = nullptr`
- `Reset()` 시점: Allocated 리스트를 비우면서 `m_pCurCmdList`도 nullptr 상태 (이미 Close되어 nullptr인 채로 도달)

`m_pCurCmdList`는 "현재 기록 슬롯"이며, `GetCurrentCommandList()`는 이 슬롯이 비어 있을 때만 새 CommandList를 꺼낸다. 슬롯이 비어있다는 것은 곧 "다음 기록을 새 CommandList에서 시작한다"는 의미다.

</details></td></tr></table>

---

## Q19. ch19에서 CommandListPool을 도입했지만 실제 성능 향상이 거의 없는 이유는?

① CommandList 객체 생성 비용이 무시할 정도로 작아서  
② 여전히 싱글스레드 기록이라 CPU 기록 시간이 줄지 않음. 같은 Queue 순차 실행이라 GPU 시간도 동일. ch20에서 멀티스레드 기록을 도입해야 효과가 나타남  
③ GPU 드라이버가 자동으로 최적화하므로  
④ Fence 동기화 비용이 더 커져서 손해를 봄  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

ch19는 **구조 변경만** 한 단계다. 한 스레드에서 CommandList 3개를 순차로 기록하는 것은 CommandList 1개에 모두 기록하는 것과 CPU 시간이 거의 같다(드라이버 오버헤드 차이만 미미하게 있음).

진짜 이득:
- **ch20**: 스레드 A/B/C가 동시에 CL_A/CL_B/CL_C에 기록 → CPU 기록 시간 1/3
- **재사용 측면**: 매 프레임 `new/delete` 비용 제거 (소소)
- **드라이버 측면**: `ExecuteCommandLists(1)` × N번보다 `ExecuteCommandLists(N)` × 1번이 약간 효율적

ch19의 진짜 목적은 ch20을 위한 **구조적 준비**다.

</details></td></tr></table>

---

## Q20. `RenderQueue::Process()`에서 한 묶음을 모두 처리한 뒤 `CloseAndExecute()`가 아니라 `Close()`만 호출하고 마지막에 `ExecuteCommandLists(N개, 배열)`을 호출하는 이유는?

① `CloseAndExecute()`는 버그가 있어서  
② `ExecuteCommandLists` 호출은 드라이버에 KMD 진입 비용이 들어가므로, N번 부르는 것보다 1번에 N개 모아 부르는 것이 효율적이기 때문  
③ Fence 신호가 마지막 한 번만 발생해야 해서  
④ GPU가 1프레임에 1번만 깨어날 수 있어서  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
// 나쁜 방식 (각 CL마다 Submit)
ExecuteCommandLists(1, CL_0)  // 커널 진입, 드라이버 오버헤드
ExecuteCommandLists(1, CL_1)  // 커널 진입, 드라이버 오버헤드
ExecuteCommandLists(1, CL_2)  // 커널 진입, 드라이버 오버헤드

// 좋은 방식 (한 번에 N개)
ExecuteCommandLists(3, [CL_0, CL_1, CL_2])  // 커널 진입 1번
```

`ExecuteCommandLists`는 사용자 모드 드라이버 → 커널 모드 드라이버(KMD)로 진입하는 비용이 있는 호출이다.  
GPU 실행 순서는 어차피 같지만, CPU 측 Submit 비용을 줄이기 위해 RenderQueue는 N개를 모아 한 번에 제출한다.

이것이 `Close()`(닫기만)와 `CloseAndExecute()`(닫고 즉시 실행)를 분리한 이유다. 단일 CommandList는 후자, 여러 개 묶을 때는 전자를 쓴다.

</details></td></tr></table>
