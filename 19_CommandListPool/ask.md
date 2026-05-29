# Chapter 19 Q&A

---

## Q. CommandListPool 구조가 이해가 안 간다. 왜 이게 멀티스레드인지?

CommandList가 여러 개 필요한 이유와 멀티스레드 관계는 **GPU 실행** 쪽이 아니라 **CPU 기록** 쪽에 있습니다.

**D3D12 근본 제약**:
- CommandList 1개는 한 번에 한 스레드만 기록 가능
- CommandAllocator는 GPU가 다 쓰기 전까지 Reset 불가

**현재 ch19 (싱글스레드지만 구조만 준비)**:
```
메인스레드만:
  아이템 0~399   → CL_0 에 기록 → Close
  아이템 400~799 → CL_1 에 기록 → Close
  아이템 800~    → CL_2 에 기록 → Close
  ExecuteCommandLists([CL_0, CL_1, CL_2])
```

**미래 ch20+ (멀티스레드)**:
```
스레드 A:  아이템 0~399   → Pool에서 CL 할당 → Draw 기록 → Close
스레드 B:  아이템 400~799 → Pool에서 CL 할당 → Draw 기록 → Close  ← 동시에
스레드 C:  아이템 800~    → Pool에서 CL 할당 → Draw 기록 → Close

메인스레드: ExecuteCommandLists([CL_A, CL_B, CL_C])  ← 모아서 한 번만
```

Pool이 없으면 "스레드 A가 쓸 CL", "스레드 B가 쓸 CL"을 미리 고정해야 합니다.  
Pool이 있으면 각 스레드가 **필요할 때 꺼내 쓰고 반납**할 수 있습니다.

**프레임당 Pool이 따로 있는 이유** (`m_ppCommandListPool[2]`):
- CPU가 프레임 N+1을 기록하는 동안 GPU는 프레임 N을 실행 중
- Pool[0]은 GPU가 실행 중이므로 CPU가 건드리면 안 됨
- Pool[1]은 GPU가 안 쓰므로 CPU가 자유롭게 기록 가능 → 더블버퍼링

```
타임라인:
프레임 0:  [CPU: Pool[0] 기록]  [GPU: Pool[0] 실행────────────────]
프레임 1:                [CPU: Pool[1] 기록]  [GPU: Pool[1] 실행────]
프레임 2:  Wait(F2) 후 Pool[0].Reset() → [CPU: Pool[0] 재기록]
```

---

## Q. Available 상태랑 Allocated 리스트가 있는데 언제 반환하는지 자세히 알고 싶다. 어떤 프레임에서 어떤 자원이 등록되고 쓰이고 반환되는지?

### CommandList 하나의 전체 생명주기

```
생성(AddCmdList)        AllocCmdList()         Close()/CloseAndExecute()
      │                      │                         │
      ▼                      ▼                         ▼
 [Available 리스트]  →  [Allocated 리스트]     (bClosed=TRUE, 리스트엔 그대로)
   "쓸 준비 완료"          "CPU 기록 중 or              "기록 완료,
                           기록 끝났지만                GPU가 실행 중"
                           GPU 아직 안 끝남"

                                   Reset() 호출 시 (GPU 완료 확인 후)
                                          │
                                          ▼
                                    Allocator.Reset()
                                    CommandList.Reset()
                                    bClosed = FALSE
                                    Allocated → Available 이동
```

### 프레임별 상세 추적 (MAX_PENDING_FRAME_COUNT=2)

**프레임 0 (curIndex=0, Pool[0] 사용)**:
```
BeginRender():
  Pool[0].GetCurrentCommandList()
    → Available 비어있음 → AddCmdList() → (Alloc_0, CL_0) 생성
    → Available→Allocated 이동
  CL_0에 ResourceBarrier, ClearRTV, ClearDSV 기록
  Pool[0].CloseAndExecute() → CL_0.Close(), ExecuteCommandLists(1, CL_0)
                               m_pCurCmdList = nullptr
  Fence() → 펜스값 F1

  Pool[0]: Available=[], Allocated=[CL_0(closed)], Total=1

EndRender():
  아이템 1~400 기록:
    GetCurrentCommandList() → AddCmdList() → (Alloc_1, CL_1) 생성 → Allocated
    Draw 400개 → 배치 한도: Pool[0].Close() → CL_1.Close(), ppCommandList[0]=CL_1

  아이템 401~N 기록:
    GetCurrentCommandList() → AddCmdList() → (Alloc_2, CL_2) 생성 → Allocated
    Draw 나머지 → Pool[0].Close() → ppCommandList[1]=CL_2

  ExecuteCommandLists(2, [CL_1, CL_2])  ← GPU 렌더 아이템 실행 시작

  GetCurrentCommandList() → (Alloc_3, CL_3) 생성 → Allocated
  ResourceBarrier RT→PRESENT → CloseAndExecute → CL_3 즉시 Submit

  Pool[0]: Available=[], Allocated=[CL_0, CL_1, CL_2, CL_3], Total=4
           모두 bClosed=TRUE, GPU 실행 중

Present():
  Fence() → F2 / m_pui64LastFenceValue[0] = F2
  nextIndex = 1
  WaitForFenceValue(m_pui64LastFenceValue[1]) → 0이라 즉시 통과
  Pool[1].Reset() → 비어있어 아무것도 안 함
  curIndex = 1
```

**프레임 1 (curIndex=1, Pool[1] 사용)**:
```
GPU는 Pool[0]의 CL_0~CL_3 실행 중 — CPU는 Pool[1]을 독립적으로 사용

BeginRender() → Pool[1]에서 CL_0' 생성, Clear 기록, CloseAndExecute
                Fence() → F3 / m_pui64LastFenceValue[1] = F3
EndRender()   → Pool[1]에서 CL_1', CL_2', CL_3' 생성, 렌더 기록

Present():
  Fence() → F4 / m_pui64LastFenceValue[1] = F4
  nextIndex = 0
  ★ WaitForFenceValue(m_pui64LastFenceValue[0]) = WaitForFenceValue(F2)
    → GPU가 프레임 0의 CL_0~CL_3 끝낼 때까지 CPU 대기
  ★ Pool[0].Reset():
    Allocated=[CL_0, CL_1, CL_2, CL_3] 전체 순회:
      각 Allocator.Reset() + CL.Reset() + bClosed=FALSE
      Allocated → Available 이동
    결과: Available=[CL_0, CL_1, CL_2, CL_3], Allocated=[], Total=4
  curIndex = 0
```

**프레임 2 (curIndex=0)**:
```
Pool[0].GetCurrentCommandList()
  → Available=[CL_0, CL_1, CL_2, CL_3]에서 CL_0 꺼냄
  → AddCmdList() 호출 없음! 기존 객체 재사용
```

### Present()의 교대 패턴 요약

| Present() 호출 시 curIndex | nextIndex | 누구를 Wait하고 Reset하나 |
|--------------------------|-----------|--------------------------|
| 0 | 1 | Pool[1] Wait(F_1의 fence) → Pool[1].Reset() |
| 1 | 0 | Pool[0] Wait(F_0의 fence) → Pool[0].Reset() |

**"지금 내가 쓴 Pool은 건드리지 않고, 다음에 쓸 Pool을 미리 Reset한다"** 는 구조.  
자기 Pool을 Reset하면 GPU가 아직 실행 중일 수 있으므로 항상 반대편 슬롯을 Reset합니다.

---

## Q. Pool[1]의 Allocated=[CL_0', CL_1', CL_2', CL_3']는 언제 다시 쓰이는가?

프레임 2의 `Present()`에서 Reset되고, 프레임 3에서 재사용됩니다.

```
프레임 1 Present():
  Fence() → F4 / m_pui64LastFenceValue[1] = F4
  nextIndex = 0 → Pool[0] Reset (Pool[1]은 아직 안 건드림)
  curIndex = 0

프레임 2 Present():
  nextIndex = (0+1)%2 = 1
  ★ WaitForFenceValue(m_pui64LastFenceValue[1]) = WaitForFenceValue(F4)
    → GPU가 프레임 1의 CL_0'~CL_3' 다 끝낼 때까지 대기
  ★ Pool[1].Reset()
    → Available=[], Allocated=[CL_0', CL_1', CL_2', CL_3']
    → 전부 Allocator.Reset() + CL.Reset()
    → Available=[CL_0', CL_1', CL_2', CL_3'], Allocated=[]
  curIndex = 1

프레임 3 (curIndex=1):
  Pool[1].GetCurrentCommandList()
    → Available에서 CL_0' 꺼내 재사용
```

---

## Q. 여러 개의 CommandList를 제출하면 GPU가 알아서 스케줄링해서 병렬로 일을 하는가?

**아니오. 같은 Queue에 제출된 CommandList들은 GPU가 순서대로 실행합니다.**

```cpp
pCommandQueue->ExecuteCommandLists(3, [CL_0, CL_1, CL_2]);
// GPU 실행 순서: CL_0 → CL_1 → CL_2 (순차, 순서 보장)
```

이득은 **GPU 실행** 쪽이 아니라 **CPU 기록** 쪽에 있습니다:

```
싱글스레드:
메인스레드: [CL_0에 1000개 Draw 순서대로 기록─────────────────]
            → Execute
            → GPU: CL_0 ─────────────────────────────────

멀티스레드 (ch20 예정):
스레드A: [CL_0에 0~399 기록───]     ← 세 스레드가
스레드B:     [CL_1에 400~799 기록──] ← 동시에 기록
스레드C:         [CL_2에 800~999 기록]
메인스레드: ExecuteCommandLists(3, [CL_0, CL_1, CL_2])
            → GPU: CL_0 → CL_1 → CL_2 (순차)
```

CPU 기록 시간이 1/3으로 줄어들어 전체 프레임 시간이 단축됩니다.

**GPU가 진짜 병렬로 일하려면** 별도의 Queue를 사용해야 합니다:
```
DIRECT Queue:   [CL_0 → CL_1 → CL_2]  ← 렌더링
COMPUTE Queue:  [CCL_0 → CCL_1]        ← 물리/AI 계산   ← 동시 실행
COPY Queue:     [COPY_0]               ← 텍스처 업로드  ← 동시 실행
```

같은 Queue = 순차 실행 (순서 보장)  
다른 Queue = 동시 실행 가능 (Fence로 동기화 필요)

---

## Q. CloseAndExecute()는 Pool 안의 CommandList들을 동시에 실행하는 건가?

**아니오. CloseAndExecute는 m_pCurCmdList 1개만 닫고 즉시 Execute합니다.**

```cpp
void CCommandListPool::CloseAndExecute(ID3D12CommandQueue* pCommandQueue)
{
    m_pCurCmdList->pDirectCommandList->Close();
    m_pCurCmdList->bClosed = TRUE;
    pCommandQueue->ExecuteCommandLists(1, ...);  // 1개만
    m_pCurCmdList = nullptr;
}
```

N개를 한 번에 Execute하는 것은 `RenderQueue::Process()` 안에서 직접 합니다:

```cpp
// RenderQueue가 ppCommandList 배열에 모아서
pCommandQueue->ExecuteCommandLists(dwCommandListCount, ppCommandList);  // N개
```

| 상황 | 방식 |
|---|---|
| BeginRender - Clear | `CloseAndExecute()` → 1개, 즉시 실행 |
| EndRender - 오브젝트 렌더링 | `Close()` 여러 번 → 마지막에 N개 한번에 실행 |
| EndRender - Present용 Barrier | `CloseAndExecute()` → 1개, 즉시 실행 |

`Close()` = 닫기만. Execute는 RenderQueue가 나중에 일괄 처리  
`CloseAndExecute()` = 닫자마자 혼자 Execute

---

## Q. BeginRender와 EndRender의 CommandList는 이어지는 건가, 독립적인 건가?

**독립적입니다.** `CloseAndExecute()` 안에서 `m_pCurCmdList = nullptr`로 슬롯을 비우기 때문입니다.

```
BeginRender():
  GetCurrentCommandList()  → Pool에서 CL-A 꺼냄 (m_pCurCmdList = CL-A)
  ResourceBarrier, Clear 기록
  CloseAndExecute()        → CL-A 실행, m_pCurCmdList = nullptr

EndRender():
  RenderQueue::Process() 첫 GetCurrentCommandList()
    → m_pCurCmdList가 nullptr이므로 Pool에서 CL-B 꺼냄
  Draw 기록...
```

D3D12 자체 제약: CommandList는 `Close()` 한 번 하면 수정 불가.  
다시 기록하려면 `Reset()` (= 새 CommandList)이 필요하므로 이어질 수가 없습니다.

---

## Q. CCommandListPool의 실체(멤버변수)는 정확히 무엇인가?

```cpp
class CCommandListPool
{
    // 현재 기록 중인 COMMAND_LIST 포인터 1개
    COMMAND_LIST* m_pCurCmdList = nullptr;

    // 연결리스트 2개
    SORT_LINK* m_pAvailableCmdLinkHead;   // 쓸 수 있는 COMMAND_LIST들
    SORT_LINK* m_pAvailableCmdLinkTail;
    SORT_LINK* m_pAlloatedCmdLinkHead;    // 이미 Close된 COMMAND_LIST들
    SORT_LINK* m_pAlloatedCmdLinkTail;

    // 카운터
    DWORD m_dwMaxCmdListNum = 256;    // 최대 몇 개까지 만들 수 있나
    DWORD m_dwTotalCmdNum   = 0;      // 현재까지 만들어진 총 개수
    DWORD m_dwAvailableCmdNum = 0;
    DWORD m_dwAllocatedCmdNum = 0;
};

// Pool이 관리하는 노드 하나
struct COMMAND_LIST {
    ID3D12CommandAllocator*    pDirectCommandAllocator;  // GPU 명령 저장 메모리
    ID3D12GraphicsCommandList* pDirectCommandList;       // 실제 CommandList API 핸들
    SORT_LINK  Link;    // 연결리스트 링크 (앞뒤 포인터)
    BOOL bClosed;       // Close 됐는지 플래그
};
```

Pool은 이 COMMAND_LIST 구조체들을 Available/Allocated 두 연결리스트로 관리하는 클래스입니다.

---

## Q. CommandListPool은 Initialize 시 미리 CommandList를 확보해 두는가?

**아니오. Lazy 생성입니다.** `Initialize()`는 상한선(256)만 저장하고 아무것도 만들지 않습니다.

```cpp
BOOL CCommandListPool::Initialize(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type, DWORD dwMaxCommandListNum)
{
    m_dwMaxCmdListNum = dwMaxCommandListNum;  // 256만 기록
    m_pD3DDevice = pDevice;
    return TRUE;
    // Available 비어있음, DX12 객체 아무것도 없음
}
```

실제 생성은 첫 `GetCurrentCommandList()` 호출 때 일어납니다.

---

## Q. AddCmdList()와 AllocCmdList()는 각각 무슨 역할인가?

```cpp
BOOL CCommandListPool::AddCmdList()
{
    if (m_dwTotalCmdNum >= m_dwMaxCmdListNum)
        __debugbreak();  // 256개 초과 에러

    // DX12 객체 생성
    pDevice->CreateCommandAllocator(..., &pDirectCommandAllocator);
    pDevice->CreateCommandList(..., &pDirectCommandList);

    // CPU 구조체 new
    pCmdList = new COMMAND_LIST;
    pCmdList->pDirectCommandList = pDirectCommandList;
    pCmdList->pDirectCommandAllocator = pDirectCommandAllocator;
    m_dwTotalCmdNum++;

    // Available 리스트에 추가
    LinkToLinkedListFIFO(&m_pAvailableCmdLinkHead, ..., &pCmdList->Link);
    m_dwAvailableCmdNum++;
}
```

```cpp
COMMAND_LIST* CCommandListPool::AllocCmdList()
{
    if (!m_pAvailableCmdLinkHead)   // Available 비어있으면
        AddCmdList();               // 새로 생성

    // Available 맨 앞에서 꺼냄
    pCmdList = m_pAvailableCmdLinkHead->pItem;
    UnLink(Available, pCmdList);
    m_dwAvailableCmdNum--;

    // Allocated로 이동
    LinkFIFO(Allocated, pCmdList);
    m_dwAllocatedCmdNum++;

    return pCmdList;  // m_pCurCmdList에 저장됨
}
```

| | `AddCmdList()` | `AllocCmdList()` |
|---|---|---|
| 하는 일 | DX12 객체 생성 + Available에 추가 | Available → Allocated로 이동 |
| 호출 시점 | Available이 비어있을 때 AllocCmdList가 호출 | GetCurrentCommandList()가 호출 |

**Allocated로 언제 이동하나**: `AllocCmdList()` 시점, 즉 `GetCurrentCommandList()` 호출 때입니다.  
`Close()`는 Allocated 리스트를 건드리지 않고 `m_pCurCmdList = nullptr`만 합니다.

---

## Q. 그러면 결국 커맨드 기록을 빨리 하는 거지, GPU에서는 순차 실행이 맞는 건가?

맞습니다.

```
CPU: 기록 빠르게 (병렬)  →  GPU: 순차 실행
```

CommandListPool / 멀티스레드 기록은 **CPU 병목을 줄이는 기법**이고,  
GPU 실행 순서는 항상 Queue에 넣은 순서대로입니다.

---

## Q. EndRender()에서 오브젝트 렌더링이 끝난 뒤 Present용 CommandList를 왜 또 만드는가?

`RenderQueue::Process()` 직후 RenderTarget 리소스 상태는 `RENDER_TARGET`입니다.  
`SwapChain->Present()` 호출 전에 반드시 `PRESENT` 상태로 바꿔야 합니다.  
이 상태 전환(ResourceBarrier)은 GPU 명령이므로 CommandList를 통해야 합니다.

```
RenderQueue::Process() 끝
  ExecuteCommandLists([CL_0, CL_1, CL_2]) → GPU 큐에 Submit
  RenderTarget 상태 = RENDER_TARGET (GPU 기준)

// 바로 이어서
GetCurrentCommandList() → Pool에서 CL_3 꺼냄
CL_3: ResourceBarrier(RENDER_TARGET → PRESENT)
CloseAndExecute(CL_3) → GPU 큐에 CL_3 Submit

// GPU 실행 순서: CL_0 → CL_1 → CL_2 → CL_3
// CL_3까지 끝나면 RenderTarget 상태 = PRESENT

SwapChain->Present()  ← 이때 상태 전환 완료되어 있음
```

Present용 Barrier는 CommandList 1개로 충분하므로 `CloseAndExecute()` 1번입니다.

---

## Q. CommandList는 Draw시마다 생성하는 건가? 생성 단위가 뭔가?

**Draw 1개마다 생성하는 게 아닙니다.** CommandList 1개에 Draw 400개를 기록합니다.

```
오브젝트 1200개, dwProcessCountPerCommandList=400 인 경우:

프레임 1:
  GetCurrentCommandList() → Available 비어있음 → AddCmdList() → CL-A 생성
  Draw 1~400 기록 (CL-A 하나에 400개)
  Close() → m_pCurCmdList = nullptr

  GetCurrentCommandList() → Available 비어있음 → AddCmdList() → CL-B 생성
  Draw 401~800 기록
  Close()

  GetCurrentCommandList() → Available 비어있음 → AddCmdList() → CL-C 생성
  Draw 801~1200 기록
  Close()

  → Draw 1200번에 CommandList 생성은 3번

프레임 1 끝: Reset() → CL-A, CL-B, CL-C → Available 복귀

프레임 2:
  GetCurrentCommandList() → Available=[CL-A, CL-B, CL-C] → CL-A 꺼냄 (생성 없음)
  Draw 1~400 기록
  ...
```

생성 단위 = **Close() 이후 다음 GetCurrentCommandList() 호출 시점** = 400개 batch 단위.  
프레임 1에서 필요한 개수만큼 만들어지고, 프레임 2부터는 `AddCmdList()` 호출 없이 재사용합니다.

---

## Q. CommandListPool을 쓰는 의의가 뭔가?

**세 가지입니다.**

**1. 재사용 (new/delete 비용 제거)**
```
프레임 1: new CommandList (CreateCommandAllocator, CreateCommandList 비용)
프레임 2: 기존 CommandList 재사용 (비용 없음)
프레임 3: 재사용 ...
```

**2. ExecuteCommandLists(N) 한 번으로 드라이버 오버헤드 절감**
```
// 나쁜 방식
ExecuteCommandLists(1, CL_0)  // 드라이버 오버헤드
ExecuteCommandLists(1, CL_1)  // 드라이버 오버헤드
ExecuteCommandLists(1, CL_2)  // 드라이버 오버헤드

// Pool 방식 (RenderQueue가 모아서)
ExecuteCommandLists(3, [CL_0, CL_1, CL_2])  // 오버헤드 1번
```

**3. 멀티스레드 준비 (ch20 목적)**
```
스레드A: Pool에서 CL 꺼냄 → 400개 기록
스레드B: Pool에서 CL 꺼냄 → 400개 기록  ← 동시에
스레드C: Pool에서 CL 꺼냄 → 400개 기록  ← 동시에
ExecuteCommandLists(3, [CL_A, CL_B, CL_C])
```
Pool 없이 멀티스레드를 하려면 스레드마다 CommandList를 고정 배정해야 합니다.  
Pool이 있으면 스레드가 필요할 때 꺼내 쓰고 반납합니다.

ch19는 싱글스레드이므로 1번, 2번이 주요 이득이고, ch20에서 3번이 진짜 목적입니다.

---

## Q. `Fence()` 함수와 `WaitForFenceValue()`는 어떤 관계인가?

둘은 한 쌍입니다.

- `Fence()` = "여기까지 GPU가 실행하면 이 숫자가 완료됐다고 표시해라" 라는 표식을 GPU Queue 끝에 꽂는 함수
- `WaitForFenceValue(x)` = "GPU가 x번 표식까지 끝낼 때까지 CPU가 기다리는" 함수

코드 기준:

```cpp
UINT64 CD3D12Renderer::Fence()
{
    m_ui64FenceVaule++;
    m_pCommandQueue->Signal(m_pFence, m_ui64FenceVaule);
    m_pui64LastFenceValue[m_dwCurContextIndex] = m_ui64FenceVaule;
    return m_ui64FenceVaule;
}

void CD3D12Renderer::WaitForFenceValue(UINT64 ExpectedFenceValue)
{
    if (m_pFence->GetCompletedValue() < ExpectedFenceValue)
    {
        m_pFence->SetEventOnCompletion(ExpectedFenceValue, m_hFenceEvent);
        WaitForSingleObject(m_hFenceEvent, INFINITE);
    }
}
```

즉 `Fence()`는 **값을 발행/기록**하고, `WaitForFenceValue()`는 그 **값이 완료될 때까지 대기**합니다.

### 왜 `m_pui64LastFenceValue[m_dwCurContextIndex]`에 저장하나?

`m_dwCurContextIndex`는 현재 사용 중인 프레임 슬롯이자 Pool 인덱스입니다.

```
m_pui64LastFenceValue[0] = Pool[0]로 제출한 GPU 작업의 마지막 완료 표식
m_pui64LastFenceValue[1] = Pool[1]로 제출한 GPU 작업의 마지막 완료 표식
```

Fence 값은 CommandList 1개마다 따로 저장하지 않고, **Pool/프레임 슬롯 단위**로 저장합니다.

한 프레임에서 Pool[0]로 여러 CommandList를 제출해도 GPU Queue 마지막에 `Signal(F2)`가 들어가면:

```
GPU Queue:
  Pool[0] Clear CL
  Pool[0] Draw CL_0
  Pool[0] Draw CL_1
  Pool[0] PresentBarrier CL
  Signal(F2)
```

GPU Queue는 순서대로 실행되므로 `F2 완료`는 그 앞의 Pool[0] CommandList들이 전부 끝났다는 뜻입니다.

### Present()에서 왜 nextIndex의 fence를 기다리나?

```cpp
DWORD dwNextContextIndex = (m_dwCurContextIndex + 1) % MAX_PENDING_FRAME_COUNT;
WaitForFenceValue(m_pui64LastFenceValue[dwNextContextIndex]);

m_ppConstBufferManager[dwNextContextIndex]->Reset();
m_ppDescriptorPool[dwNextContextIndex]->Reset();
m_ppCommandListPool[dwNextContextIndex]->Reset();
m_dwCurContextIndex = dwNextContextIndex;
```

이 코드는 이렇게 읽으면 됩니다:

```
다음 프레임에 nextIndex Pool을 다시 쓸 것이다.
그런데 그 Pool은 과거 프레임에서 GPU가 사용했을 수 있다.
그러니 nextIndex Pool의 마지막 fence 값까지 GPU가 끝났는지 확인한다.
끝났으면 nextIndex Pool을 Reset하고 현재 Pool로 바꾼다.
```

타임라인:

```
프레임 0:
  curIndex = 0
  Pool[0] CommandList들 제출
  Fence() → m_pui64LastFenceValue[0] = F2
  nextIndex = 1
  WaitForFenceValue(m_pui64LastFenceValue[1])  // 초기값 0, 즉시 통과
  Pool[1].Reset()
  curIndex = 1

프레임 1:
  curIndex = 1
  Pool[1] CommandList들 제출
  Fence() → m_pui64LastFenceValue[1] = F4
  nextIndex = 0
  WaitForFenceValue(m_pui64LastFenceValue[0])  // F2까지 GPU 완료 대기
  Pool[0].Reset()
  curIndex = 0

프레임 2:
  curIndex = 0
  Pool[0] 재사용
```

핵심은 이 한 줄입니다:

```
WaitForFenceValue(m_pui64LastFenceValue[nextIndex])
= 이제 nextIndex Pool을 다시 쓸 건데, GPU가 그 Pool의 이전 사용분을 끝냈는지 확인한다.
```

따라서 `Fence()`와 `WaitForFenceValue()`의 관계는 "현재 Pool의 마지막 GPU 완료 표식을 저장하고, 나중에 그 Pool을 다시 쓰기 전에 그 표식까지 기다리는 구조"입니다.

---

## Q. Fence는 CommandList마다 하는가, 아니면 CommandListPool마다 하는가?

이 코드에서는 **CommandList마다가 아니라 CommandListPool 슬롯마다** 관리합니다.

정확히는 Pool 객체 안에 Fence 값이 들어있는 것은 아니고, Renderer가 아래 배열로 Pool 인덱스별 마지막 Fence 값을 들고 있습니다.

```cpp
UINT64 m_pui64LastFenceValue[MAX_PENDING_FRAME_COUNT] = {};
```

`MAX_PENDING_FRAME_COUNT = 2`이므로 의미는 이렇게 됩니다.

```
m_pui64LastFenceValue[0] = Pool[0]로 제출한 마지막 GPU 완료 표식
m_pui64LastFenceValue[1] = Pool[1]로 제출한 마지막 GPU 완료 표식
```

한 Pool 안에는 여러 CommandList가 있을 수 있습니다.

```
Pool[0] 한 프레임 사용분:
  CL_Clear
  CL_Draw_0
  CL_Draw_1
  CL_PresentBarrier
```

하지만 Fence는 각 CommandList 뒤에 따로 붙이지 않고, 그 프레임에서 Queue에 제출한 작업들의 끝 지점에 붙습니다.

```
GPU Queue:
  CL_Clear
  CL_Draw_0
  CL_Draw_1
  CL_PresentBarrier
  Signal(F2)

m_pui64LastFenceValue[0] = F2
```

같은 Queue는 순서대로 실행되므로 `F2 완료`는 그 앞의 Pool[0] CommandList들이 전부 끝났다는 뜻입니다. 그래서 CommandList마다 Fence를 저장하지 않아도 Pool[0] 전체를 Reset해도 되는지 판단할 수 있습니다.

정리:

| 기준 | 이 코드에서 관리 여부 | 설명 |
|---|---|---|
| CommandList 개별 | 아님 | CL마다 fence 값을 따로 저장하지 않음 |
| CommandListPool 슬롯 | 맞음 | Pool[0], Pool[1]별 마지막 fence 값을 Renderer가 저장 |
| 실제 저장 위치 | Renderer | `m_pui64LastFenceValue[index]` |

따라서 이 코드는 **multi CommandList를 개별 추적하는 구조가 아니라, multi frame Pool 슬롯을 기준으로 GPU 완료 여부를 추적하는 구조**입니다.
