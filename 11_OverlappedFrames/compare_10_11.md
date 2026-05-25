# Chapter 10 vs Chapter 11: Overlapped Frames 심층 분석

## 핵심 개념: Overlapped Frames(프레임 오버랩)이란?

Chapter 10까지의 구조는 매 프레임 끝에서 GPU가 **완전히 끝날 때까지 CPU가 기다리는** 방식이었다.  
Chapter 11의 Overlapped Frames는 **CPU가 다음 프레임을 준비하는 동안 GPU는 이전 프레임을 처리**할 수 있도록,  
Command Allocator, Command List, Descriptor Pool, Constant Buffer Pool 등의 리소스를  
**프레임 단위로 복수 개 유지하는 기법**이다.

```
[Chapter 10 - 동기 방식]
Frame N:  CPU기록 → GPU실행 → CPU대기(Fence) → CPU기록 → GPU실행 → ...
Timeline: [==CPU==][======GPU======][==CPU==][======GPU======]
                               ^---- CPU가 여기서 GPU 완료까지 블로킹

[Chapter 11 - Overlapped 방식]  
Frame N:  CPU기록[N] → GPU실행[N] (비동기 시작)
Frame N+1:          CPU기록[N+1] (GPU[N]과 동시 진행)
            GPU[N]이 아직 진행 중이어도 CPU는 다음 프레임 준비 가능
Timeline: [=CPU N=][=CPU N+1=][=CPU N+2=]...
                   [===GPU N===][===GPU N+1===]...
```

---

## 1. 상수 변경: SWAP_CHAIN_FRAME_COUNT와 MAX_PENDING_FRAME_COUNT

### Chapter 10
```cpp
// D3D12Renderer.h
const UINT SWAP_CHAIN_FRAME_COUNT = 2;
```

### Chapter 11
```cpp
// D3D12Renderer.h
const UINT SWAP_CHAIN_FRAME_COUNT = 3;
const UINT MAX_PENDING_FRAME_COUNT = SWAP_CHAIN_FRAME_COUNT - 1; // == 2
```

### 왜 3버퍼인가? (Triple Buffering)

| 개념 | 역할 |
|---|---|
| Back Buffer 0 | GPU가 렌더링 중인 버퍼 |
| Back Buffer 1 | SwapChain이 Present 대기 중인 버퍼 |
| Back Buffer 2 | CPU가 다음 프레임을 기록하는 버퍼 |

더블 버퍼(2개)로는 CPU와 GPU가 동시에 작업할 수 있는 슬롯이 1개뿐이라서  
실질적인 파이프라인 오버랩이 불가능하다.  
트리플 버퍼(3개)가 되어야 비로소 **"GPU가 N번 프레임을 렌더링하는 동안, CPU가 N+1번 프레임을 기록"**하는  
진정한 오버랩이 성립한다.

`MAX_PENDING_FRAME_COUNT = SWAP_CHAIN_FRAME_COUNT - 1 = 2`는  
"GPU에서 **처리 중(pending)** 상태로 둘 수 있는 최대 프레임 수"를 뜻한다.  
항상 SwapChain 버퍼 수보다 1개 적은 이유는, Present된 버퍼를 제외한 나머지가  
실제로 CPU/GPU가 동시에 사용할 수 있는 슬롯이기 때문이다.

---

## 2. 멤버 변수 구조 변경: 단일 → 배열

### Chapter 10 (단일 세트)
```cpp
ID3D12CommandAllocator*    m_pCommandAllocator  = nullptr;
ID3D12GraphicsCommandList* m_pCommandList       = nullptr;
CDescriptorPool*           m_pDescriptorPool    = nullptr;
CSimpleConstantBufferPool* m_pConstantBufferPool = nullptr;
UINT64                     m_ui64FenceValue     = 0;
```

### Chapter 11 (배열로 확장)
```cpp
ID3D12CommandAllocator*    m_ppCommandAllocator[MAX_PENDING_FRAME_COUNT]  = {};
ID3D12GraphicsCommandList* m_ppCommandList[MAX_PENDING_FRAME_COUNT]       = {};
CDescriptorPool*           m_ppDescriptorPool[MAX_PENDING_FRAME_COUNT]    = {};
CSimpleConstantBufferPool* m_ppConstantBufferPool[MAX_PENDING_FRAME_COUNT];
UINT64                     m_pui64LastFenceValue[MAX_PENDING_FRAME_COUNT] = {};
UINT64                     m_ui64FenceVaule = 0;
```

### 왜 배열이어야 하는가?

`CommandAllocator`는 GPU가 해당 Allocator의 명령을 **실행 중인 동안 절대 Reset(재사용)할 수 없다**.  
만약 GPU가 Frame N의 CommandAllocator를 처리하는 도중 CPU가 같은 Allocator를 Reset하면  
**메모리 상의 커맨드 데이터가 덮어씌워져 GPU가 잘못된 커맨드를 실행**하게 된다.  
→ D3D12 Validation Layer가 오류를 발생시키고, 실제로는 렌더링 오류 또는 GPU 크래시로 이어진다.

따라서 Frame 0과 Frame 1이 동시에 in-flight 상태가 될 수 있으므로,  
각 프레임 슬롯마다 독립된 Allocator / CommandList / DescriptorPool / CBPool이 필요하다.

`m_pui64LastFenceValue[MAX_PENDING_FRAME_COUNT]`는 **슬롯별 마지막 Fence 값**을 기억한다.  
이를 통해 "이 슬롯을 재사용하기 전에 GPU가 해당 Fence까지 완료했는지" 선별적으로 확인할 수 있다.

---

## 3. `Fence()` 함수 변경

### Chapter 10
```cpp
UINT64 CD3D12Renderer::Fence()
{
    m_ui64FenceValue++;
    m_pCommandQueue->Signal(m_pFence, m_ui64FenceValue);
    return m_ui64FenceValue;
}
```

### Chapter 11
```cpp
UINT64 CD3D12Renderer::Fence()
{
    m_ui64FenceVaule++;
    m_pCommandQueue->Signal(m_pFence, m_ui64FenceVaule);
    m_pui64LastFenceValue[m_dwCurContextIndex] = m_ui64FenceVaule; // ★ 추가
    return m_ui64FenceVaule;
}
```

### 핵심 차이: 슬롯별 Fence 값 저장

`m_pCommandQueue->Signal(pFence, value)`는 **GPU 커맨드 큐에 Signal 명령을 삽입**한다.  
- `pFence`: 신호를 받을 Fence 오브젝트  
- `value`: Fence가 이 값으로 설정될 때, "여기까지의 커맨드가 모두 완료됨"을 의미

Chapter 11에서 추가된 `m_pui64LastFenceValue[m_dwCurContextIndex] = m_ui64FenceVaule;`는  
현재 컨텍스트 인덱스(슬롯)가 **어느 Fence 값까지 사용했는지**를 기록한다.  
이 값이 있어야 나중에 해당 슬롯을 재사용하기 전에  
"GPU가 이 슬롯의 커맨드를 완료했는지" 정확히 확인할 수 있다.

---

## 4. `WaitForFenceValue()` 시그니처 변경

### Chapter 10
```cpp
void WaitForFenceValue();                          // 인자 없음

void CD3D12Renderer::WaitForFenceValue()
{
    const UINT64 ExpectedFenceValue = m_ui64FenceValue; // 항상 최신 값
    if (m_pFence->GetCompletedValue() < ExpectedFenceValue)
    {
        m_pFence->SetEventOnCompletion(ExpectedFenceValue, m_hFenceEvent);
        WaitForSingleObject(m_hFenceEvent, INFINITE);
    }
}
```

### Chapter 11
```cpp
void WaitForFenceValue(UINT64 ExpectedFenceValue);  // 명시적 인자

void CD3D12Renderer::WaitForFenceValue(UINT64 ExpectedFenceValue)
{
    if (m_pFence->GetCompletedValue() < ExpectedFenceValue)
    {
        m_pFence->SetEventOnCompletion(ExpectedFenceValue, m_hFenceEvent);
        WaitForSingleObject(m_hFenceEvent, INFINITE);
    }
}
```

### 왜 인자가 필요한가?

Chapter 10의 Wait는 **"지금 당장 최신 프레임"이 끝날 때까지 기다리는 전체 대기**였다.  
Chapter 11에서는 **"슬롯 X에 해당하는 Fence 값 Y까지만"** 선택적으로 대기해야 한다.

예를 들어 `m_pui64LastFenceValue[0] = 5`, `m_pui64LastFenceValue[1] = 7`이라면,  
슬롯 0을 재사용하기 전에는 `WaitForFenceValue(5)`만 호출하면 된다.  
슬롯 1의 작업이 끝날 때까지 기다릴 필요가 없다.  
이것이 바로 불필요한 블로킹을 줄이는 핵심 메커니즘이다.

#### 관련 API 설명
```cpp
m_pFence->GetCompletedValue()
// GPU가 현재까지 완료한 Fence 값을 반환
// 이 값이 ExpectedFenceValue 이상이면 이미 완료된 것이므로 대기 불필요

m_pFence->SetEventOnCompletion(UINT64 Value, HANDLE hEvent)
// GPU가 Fence 값이 Value 이상이 되는 순간 hEvent를 신호 상태로 설정
// 인자: Value - 대기할 Fence 값, hEvent - 신호받을 Win32 이벤트 핸들

WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
// hHandle이 신호 상태가 될 때까지 CPU 스레드 블로킹
// INFINITE = 무한 대기
```

---

## 5. `Present()` 함수 — 가장 큰 구조적 변화

### Chapter 10
```cpp
void CD3D12Renderer::Present()
{
    HRESULT hr = m_pSwapChain->Present(uiSyncInterval, uiPresentFlags);
    
    m_uiRenderTargetIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    Fence();
    WaitForFenceValue();           // ← 즉시 GPU 완료 대기 (블로킹)
    
    m_pConstantBufferPool->Reset();
    m_pDescriptorPool->Reset();
}
```

### Chapter 11
```cpp
void CD3D12Renderer::Present()
{
    Fence();                       // ← Present 전에 먼저 Signal 삽입

    HRESULT hr = m_pSwapChain->Present(uiSyncInterval, uiPresentFlags);
    
    m_uiRenderTargetIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    // 다음 슬롯을 계산하고, 그 슬롯의 이전 작업이 끝났는지 확인
    DWORD dwNextContextIndex = (m_dwCurContextIndex + 1) % MAX_PENDING_FRAME_COUNT;
    WaitForFenceValue(m_pui64LastFenceValue[dwNextContextIndex]); // ← 선택적 대기

    // 다음 슬롯의 리소스를 초기화
    m_ppConstantBufferPool[dwNextContextIndex]->Reset();
    m_ppDescriptorPool[dwNextContextIndex]->Reset();
    m_dwCurContextIndex = dwNextContextIndex;
}
```

### 상세 분석

#### (1) Fence() 호출 위치가 Present 앞으로 이동

Chapter 10에서는 Present 후에 Fence → WaitForFenceValue 순서였다.  
Chapter 11에서는 **Present 전에 Fence를 먼저 호출**한다.

이유: `Present()` 자체도 GPU 커맨드 큐 기반으로 동작한다.  
현재 프레임의 렌더링 커맨드가 ExecuteCommandLists에 의해 큐에 삽입된 상태이므로,  
Present 전에 Signal을 삽입해야 "렌더링 완료 → Fence → Present 표시" 순서로  
GPU 타임라인에 정확히 기록된다.

#### (2) `WaitForFenceValue`의 대기 대상이 "현재"가 아닌 "다음 슬롯"

```
슬롯 배열 (MAX_PENDING_FRAME_COUNT = 2):
  인덱스: [0]         [1]
         현재 슬롯   다음 슬롯

현재 슬롯(0)으로 Frame N을 기록·제출했다.
다음 슬롯(1)은 Frame N-1에 사용됐고, LastFenceValue[1]이 그때의 Fence 값이다.
→ 슬롯 1을 재사용하기 전에 LastFenceValue[1]까지 GPU가 완료했는지 확인.
```

이 패턴에서 중요한 점:  
- GPU가 Frame N-1(슬롯 1)을 **이미 완료했다면** WaitForFenceValue는 즉시 반환된다.  
- GPU가 아직 처리 중이라면 그 시점까지만 기다린다.  
- GPU가 Frame N(슬롯 0)을 처리하는 동안 CPU는 Frame N+1(슬롯 1)을 준비할 수 있다.  
  → **CPU와 GPU가 실제로 병렬로 작동**한다.

#### (3) Reset 대상 슬롯도 다음 슬롯

```cpp
m_ppConstantBufferPool[dwNextContextIndex]->Reset();
m_ppDescriptorPool[dwNextContextIndex]->Reset();
```

현재 슬롯은 GPU가 아직 처리 중이므로 Reset하면 안 된다.  
다음 슬롯은 방금 WaitForFenceValue로 GPU 완료를 확인했으므로 안전하게 Reset할 수 있다.

---

## 6. `BeginRender()` / `EndRender()` / `RenderMeshObject()` 변경

### Chapter 10
```cpp
void CD3D12Renderer::BeginRender()
{
    if (FAILED(m_pCommandAllocator->Reset())) ...
    if (FAILED(m_pCommandList->Reset(m_pCommandAllocator, nullptr))) ...
    // m_pCommandList 직접 사용
    m_pCommandList->ResourceBarrier(...);
    m_pCommandList->ClearRenderTargetView(...);
    m_pCommandList->OMSetRenderTargets(...);
}
```

### Chapter 11
```cpp
void CD3D12Renderer::BeginRender()
{
    // 현재 컨텍스트 인덱스로 슬롯 선택
    ID3D12CommandAllocator*    pCommandAllocator = m_ppCommandAllocator[m_dwCurContextIndex];
    ID3D12GraphicsCommandList* pCommandList      = m_ppCommandList[m_dwCurContextIndex];

    if (FAILED(pCommandAllocator->Reset())) ...
    if (FAILED(pCommandList->Reset(pCommandAllocator, nullptr))) ...
    // 로컬 포인터를 통해 사용
    pCommandList->ResourceBarrier(...);
    pCommandList->ClearRenderTargetView(...);
    pCommandList->OMSetRenderTargets(...);
}
```

`m_dwCurContextIndex`가 현재 프레임이 사용할 슬롯을 결정한다.  
`Present()` 끝에서 `m_dwCurContextIndex = dwNextContextIndex`로 업데이트되므로  
다음 `BeginRender()`는 자동으로 새 슬롯을 사용하게 된다.

---

## 7. `CreateCommandList()` 변경

### Chapter 10
```cpp
void CD3D12Renderer::CreateCommandList()
{
    // 1개만 생성
    m_pD3DDevice->CreateCommandAllocator(..., IID_PPV_ARGS(&m_pCommandAllocator));
    m_pD3DDevice->CreateCommandList(0, ..., m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));
    m_pCommandList->Close();
}
```

### Chapter 11
```cpp
void CD3D12Renderer::CreateCommandList()
{
    for (DWORD i = 0; i < MAX_PENDING_FRAME_COUNT; i++)
    {
        ID3D12CommandAllocator*    pCommandAllocator = nullptr;
        ID3D12GraphicsCommandList* pCommandList      = nullptr;

        m_pD3DDevice->CreateCommandAllocator(..., IID_PPV_ARGS(&pCommandAllocator));
        m_pD3DDevice->CreateCommandList(0, ..., pCommandAllocator, nullptr, IID_PPV_ARGS(&pCommandList));
        pCommandList->Close();

        m_ppCommandAllocator[i] = pCommandAllocator;
        m_ppCommandList[i]      = pCommandList;
    }
}
```

`MAX_PENDING_FRAME_COUNT`(=2)개의 Allocator와 CommandList를 생성한다.  
각각 독립적으로 생성되어야 GPU가 하나를 처리하는 동안 CPU가 다른 것을 기록할 수 있다.

#### `CreateCommandList` API 시그니처 상세
```cpp
HRESULT ID3D12Device::CreateCommandList(
    UINT nodeMask,                         // 멀티 GPU 시 노드 마스크 (단일 GPU = 0)
    D3D12_COMMAND_LIST_TYPE type,          // DIRECT: 모든 커맨드 / BUNDLE: 묶음 실행용
    ID3D12CommandAllocator* pCommandAllocator, // 이 List가 기록될 Allocator
    ID3D12PipelineState* pInitialState,    // 초기 PSO (nullptr = 기본값)
    REFIID riid,
    void** ppCommandList
);
```

CommandAllocator와 CommandList의 관계:  
- Allocator는 GPU 커맨드의 **메모리 풀**  
- CommandList는 그 풀에 **기록하는 인터페이스**  
- 하나의 Allocator에 여러 CommandList를 연결할 수 있지만,  
  **동시에 기록 중인 CommandList는 1개뿐**이어야 한다  
- Reset하려면 **GPU가 해당 Allocator를 사용한 작업을 완료**해야 한다

---

## 8. `Initialize()`에서 풀 생성 변경

### Chapter 10
```cpp
m_pDescriptorPool = new CDescriptorPool;
m_pDescriptorPool->Initialize(m_pD3DDevice, MAX_DRAW_COUNT_PER_FRAME * DESCRIPTOR_COUNT_FOR_DRAW);

m_pConstantBufferPool = new CSimpleConstantBufferPool;
m_pConstantBufferPool->Initialize(m_pD3DDevice, AlignConstantBufferSize(...), MAX_DRAW_COUNT_PER_FRAME);
```

### Chapter 11
```cpp
for (DWORD i = 0; i < MAX_PENDING_FRAME_COUNT; i++)
{
    m_ppDescriptorPool[i] = new CDescriptorPool;
    m_ppDescriptorPool[i]->Initialize(m_pD3DDevice, MAX_DRAW_COUNT_PER_FRAME * DESCRIPTOR_COUNT_FOR_DRAW);

    m_ppConstantBufferPool[i] = new CSimpleConstantBufferPool;
    m_ppConstantBufferPool[i]->Initialize(m_pD3DDevice, AlignConstantBufferSize(...), MAX_DRAW_COUNT_PER_FRAME);
}
```

DescriptorPool과 ConstantBufferPool 역시 GPU가 사용 중인 동안 Reset되면 안 된다.  
(GPU가 참조하는 Descriptor나 Constant Buffer 데이터를 CPU가 덮어쓰는 문제 발생)  
따라서 각 슬롯마다 독립된 풀이 필요하다.

#### `INL_GetDescriptorPool()` / `INL_GetConstantBufferPool()` 변경
```cpp
// Chapter 10
CDescriptorPool* INL_GetDescriptorPool() { return m_pDescriptorPool; }

// Chapter 11
CDescriptorPool* INL_GetDescriptorPool() { return m_ppDescriptorPool[m_dwCurContextIndex]; }
```
`BasicMeshObject::Draw()` 등 내부에서 Pool을 요청할 때  
항상 **현재 컨텍스트 인덱스**의 풀을 반환하므로,  
호출 측 코드 변경 없이 자동으로 올바른 슬롯의 풀을 사용한다.

---

## 9. `DeleteBasicMeshObject()` / `DeleteTexture()` 변경

### Chapter 10
```cpp
void CD3D12Renderer::DeleteBasicMeshObject(void* pMeshObjHandle)
{
    CBasicMeshObject* pMeshObj = (CBasicMeshObject*)pMeshObjHandle;
    delete pMeshObj; // 즉시 삭제
}
```

### Chapter 11
```cpp
void CD3D12Renderer::DeleteBasicMeshObject(void* pMeshObjHandle)
{
    // 모든 in-flight 프레임이 완료될 때까지 대기
    for (DWORD i = 0; i < MAX_PENDING_FRAME_COUNT; i++)
    {
        WaitForFenceValue(m_pui64LastFenceValue[i]);
    }
    CBasicMeshObject* pMeshObj = (CBasicMeshObject*)pMeshObjHandle;
    delete pMeshObj;
}

void CD3D12Renderer::DeleteTexture(void* pHandle)
{
    // 모든 in-flight 프레임이 완료될 때까지 대기
    for (DWORD i = 0; i < MAX_PENDING_FRAME_COUNT; i++)
    {
        WaitForFenceValue(m_pui64LastFenceValue[i]);
    }
    // ... 리소스 해제
}
```

### 왜 삭제 전에 전체 대기가 필요한가?

Overlapped Frames 환경에서는 **GPU가 현재 2개의 프레임을 처리 중**일 수 있다.  
예를 들어 Frame N과 Frame N-1이 동시에 in-flight 상태라면,  
Frame N-1의 CommandList가 이 MeshObject의 VertexBuffer나 Texture를 참조하고 있을 수 있다.  
이 상태에서 즉시 삭제하면 **GPU가 이미 해제된 메모리를 읽으려 하는 use-after-free 상황**이 발생한다.

따라서 삭제 연산은 **모든 슬롯의 LastFenceValue가 GPU에서 완료된 것을 확인**한 뒤 수행한다.  
이 대기는 게임 실행 중이 아닌 **씬 전환이나 오브젝트 소멸 시점**에 발생하므로 성능에 큰 영향은 없다.

---

## 10. `UpdateWindowSize()`에서 대기 처리 변경

### Chapter 10
```cpp
void CD3D12Renderer::UpdateWindowSize(...)
{
    // WaitForFenceValue 호출이 주석 처리되어 있음
    //WaitForFenceValue();
    // ... 버퍼 재생성
}
```

### Chapter 11
```cpp
void CD3D12Renderer::UpdateWindowSize(...)
{
    // 현재 진행 중인 모든 프레임 완료 대기
    Fence();
    for (DWORD i = 0; i < MAX_PENDING_FRAME_COUNT; i++)
    {
        WaitForFenceValue(m_pui64LastFenceValue[i]);
    }
    // ... 버퍼 재생성
}
```

창 크기 변경 시 SwapChain 버퍼를 재생성(ResizeBuffers)하려면  
현재 버퍼를 참조하는 **모든 GPU 커맨드가 완료**되어야 한다.  
Overlapped 환경에서는 여러 슬롯이 in-flight일 수 있으므로 전체 대기 필요.

---

## 11. `Cleanup()` 변경

### Chapter 10
```cpp
void CD3D12Renderer::Cleanup()
{
    WaitForFenceValue(); // 최신 값으로 한 번만 대기

    if (m_pConstantBufferPool) { delete m_pConstantBufferPool; ... }
    if (m_pDescriptorPool) { delete m_pDescriptorPool; ... }
    // ...
}
```

### Chapter 11
```cpp
void CD3D12Renderer::Cleanup()
{
    Fence();
    for (DWORD i = 0; i < MAX_PENDING_FRAME_COUNT; i++)
    {
        WaitForFenceValue(m_pui64LastFenceValue[i]); // 슬롯별 완료 대기
    }
    for (DWORD i = 0; i < MAX_PENDING_FRAME_COUNT; i++)
    {
        if (m_ppConstantBufferPool[i]) { delete m_ppConstantBufferPool[i]; ... }
        if (m_ppDescriptorPool[i]) { delete m_ppDescriptorPool[i]; ... }
    }
    // ...
}
```

---

## 12. 전체 변경 요약 표

| 항목 | Chapter 10 | Chapter 11 |
|---|---|---|
| `SWAP_CHAIN_FRAME_COUNT` | 2 | 3 |
| `MAX_PENDING_FRAME_COUNT` | (없음) | 2 (= SWAP_CHAIN_FRAME_COUNT - 1) |
| CommandAllocator | 1개 단일 | 2개 배열 |
| CommandList | 1개 단일 | 2개 배열 |
| DescriptorPool | 1개 단일 | 2개 배열 |
| ConstantBufferPool | 1개 단일 | 2개 배열 |
| FenceValue | 단일 최신값 | 슬롯별 LastFenceValue 배열 |
| `WaitForFenceValue()` | 인자 없음 (항상 최신) | `UINT64 ExpectedFenceValue` 인자 |
| Present() 동기화 | Present 후 전체 대기 | 다음 슬롯의 과거 Fence값만 대기 |
| 리소스 삭제 전 대기 | 없음 (즉시 삭제) | 모든 슬롯 Fence 완료 확인 후 삭제 |
| CPU-GPU 병렬성 | 없음 (매 프레임 동기) | Frame N GPU ↔ Frame N+1 CPU 병렬 |

---

## 13. 독자가 어려워하는 부분: `m_dwCurContextIndex` 흐름 추적

```
초기 상태: m_dwCurContextIndex = 0

[Frame 1]
BeginRender()  → 슬롯 0의 Allocator/CommandList/Pool 사용
RenderMeshObj()→ 슬롯 0의 CommandList에 드로우콜 기록
EndRender()    → 슬롯 0의 CommandList를 Close → ExecuteCommandLists
Present()
  → Fence()          : Signal(Fence, 1), LastFenceValue[0] = 1
  → Present swap
  → nextIdx = (0+1) % 2 = 1
  → WaitForFenceValue(LastFenceValue[1]) = WaitForFenceValue(0) → 즉시 반환 (아직 사용 안함)
  → Reset Pool[1], DescPool[1]
  → m_dwCurContextIndex = 1

[Frame 2]
BeginRender()  → 슬롯 1의 Allocator/CommandList/Pool 사용
(이때 GPU는 슬롯 0의 Frame 1을 처리 중 → CPU는 슬롯 1에서 병렬로 Frame 2 기록)
RenderMeshObj()→ 슬롯 1의 CommandList에 드로우콜 기록
EndRender()    → 슬롯 1의 CommandList를 ExecuteCommandLists
Present()
  → Fence()          : Signal(Fence, 2), LastFenceValue[1] = 2
  → Present swap
  → nextIdx = (1+1) % 2 = 0
  → WaitForFenceValue(LastFenceValue[0]) = WaitForFenceValue(1)
    → GPU가 아직 Frame 1(Signal 1)을 처리 중이면 여기서 잠깐 대기
    → GPU가 이미 완료했으면 즉시 반환
  → Reset Pool[0], DescPool[0]
  → m_dwCurContextIndex = 0

[Frame 3]
BeginRender()  → 슬롯 0 재사용 (GPU가 Frame 1을 완료했음이 보장됨)
...
```

이 순환 구조가 Overlapped Frames의 핵심이다.  
GPU는 프레임 N을 처리하고, CPU는 프레임 N+1을 기록하며, 둘 사이에 필요한 최소한의 동기화만 발생한다.

---

## 14. 성능상 의미

- **Chapter 10**: 매 프레임 CPU가 GPU 완료를 기다리므로 CPU와 GPU 중 하나가 항상 유휴 상태.  
  타임라인: `[CPU작업][GPU작업][CPU작업][GPU작업]...` (순차)

- **Chapter 11**: CPU가 N번째 프레임을 준비하는 동안 GPU는 N-1번째 프레임을 처리.  
  타임라인: `[CPU N][CPU N+1][CPU N+2]...`  
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`[GPU N-1][GPU N][GPU N+1]...`

실제 게임 엔진에서 Overlapped Frames (또는 "Frame in Flight")는  
GPU 활용률을 극대화하는 기본 패턴이며, Direct3D 12 샘플의 표준 구현이기도 하다.

---

## 참고: `CommandAllocator::Reset()` API 규칙

```cpp
HRESULT ID3D12CommandAllocator::Reset();
```
- 이 함수는 Allocator 내의 **모든 커맨드 메모리를 재활용 가능 상태로 초기화**한다.
- **반드시 GPU가 이 Allocator를 사용한 모든 CommandList의 실행을 완료한 후에만 호출 가능**하다.
- `CommandList::Reset(pAllocator, pInitialPipelineState)`를 호출하기 전에 먼저 Allocator를 Reset해야 한다.
- 이 규칙을 위반하면 D3D12 Debug Layer가 `COMMAND_ALLOCATOR_SYNC` 오류를 발생시킨다.

이것이 Chapter 11에서 `WaitForFenceValue(m_pui64LastFenceValue[dwNextContextIndex])`로  
**다음에 사용할 슬롯의 완료 여부를 확인한 뒤에만 BeginRender에서 Allocator::Reset()을 호출**하는 이유이다.
