# Chapter 20 – MultiThreadRender 객관식 퀴즈 20문제

각 문항의 "정답 보기"를 클릭하면 정답과 해설이 펼쳐집니다.

---

## Q1. ch20에서 워커 스레드 개수는 무엇으로 결정되는가?

1. 항상 4개로 고정
2. 논리 코어 수
3. 물리 코어 수 (단, `MAX_RENDER_THREAD_COUNT`로 cap)
4. SwapChain back buffer 개수

<details><summary>정답 보기</summary>

**정답: 3**

`Initialize()`에서 `GetPhysicalCoreCount()`로 얻은 물리 코어 수를 `m_dwRenderThreadCount`로 사용하며, `MAX_RENDER_THREAD_COUNT`로 상한을 둡니다.

</details>

---

## Q2. `m_ppRenderQueue`는 왜 `[프레임][스레드]`가 아니라 `[스레드]` 1차원으로만 할당되는가?

1. RenderQueue는 GPU가 직접 읽으므로 단일 인스턴스로 충분
2. RenderQueue는 CPU 임시 버퍼이고 같은 프레임 내에서 채워졌다가 비워지기 때문
3. Round-robin 정책 때문에 1개면 충분
4. Fence가 자동으로 관리해주기 때문

<details><summary>정답 보기</summary>

**정답: 2**

CommandList/Descriptor/ConstantBuffer는 GPU가 이전 프레임 것을 아직 쓰고 있을 수 있어 frame context가 필요하지만, RenderQueue는 메인이 채우고 워커가 즉시 비우는 CPU 임시 버퍼라 frame 축이 필요 없습니다.

</details>

---

## Q3. `RENDER_ITEM`이 메시 draw 요청일 때 들어가는 데이터가 아닌 것은?

1. `Type = RENDER_ITEM_TYPE_MESH_OBJ`
2. `pObjHandle = CBasicMeshObject*`
3. `MeshObjParam.matWorld`
4. View / Projection 행렬

<details><summary>정답 보기</summary>

**정답: 4**

View/Projection은 `RENDER_ITEM`에 들어가지 않습니다. Draw 시점에 `m_pRenderer->GetViewProjMatrix(...)`로 가져와 ConstantBuffer에 채웁니다.

</details>

---

## Q4. RenderQueue 내부 저장 방식은?

1. `std::vector<RENDER_ITEM>`
2. 연결 리스트
3. 미리 `malloc`한 선형 `char*` 버퍼에 `memcpy`로 누적
4. D3D12 ConstantBuffer

<details><summary>정답 보기</summary>

**정답: 3**

`Initialize()`에서 `sizeof(RENDER_ITEM) * dwMaxItemNum`만큼 `malloc`하고, `Add()`가 `memcpy`로 쌓고 `Dispatch()`가 read 포인터를 전진시킵니다.

</details>

---

## Q5. 메인이 `RenderMeshObject()`/`RenderSprite()`를 호출할 때 round-robin을 쓰는 이유는?

1. 워커별 RenderQueue 부하를 균등하게 분산하기 위해
2. 락을 줄이기 위해
3. Descriptor 충돌을 방지하기 위해
4. CommandList 순서를 보장하기 위해

<details><summary>정답 보기</summary>

**정답: 1**

한 큐에만 다 넣으면 그 워커 1명만 일하고 나머지는 노는 결과가 됩니다. round-robin으로 N개 큐에 균등 분산해야 멀티스레드 효과가 납니다. 부수적으로 메인 1명이 분배하므로 락도 필요 없습니다.

</details>

---

## Q6. `InitRenderThreadPool()`에서 워커 한 명당 생성되는 Event 개수는?

1. 1개
2. 2개 (`PROCESS`, `DESTROY`)
3. 3개
4. N개 (N=워커 수)

<details><summary>정답 보기</summary>

**정답: 2**

`hEventList[RENDER_THREAD_EVENT_TYPE_COUNT]`에 PROCESS와 DESTROY 두 개의 auto-reset event가 만들어집니다. 공용 `m_hCompleteEvent`는 워커당이 아니라 renderer당 1개입니다.

</details>

---

## Q7. `_InterlockedDecrement(&m_lActiveThreadCount)`이 반환하는 값은?

1. 감소 전의 원래 값
2. 감소 후의 새 값
3. 항상 0
4. 감소 작업 성공 여부 (BOOL)

<details><summary>정답 보기</summary>

**정답: 2**

`_InterlockedDecrement`는 atomic하게 1 감소시키고 **감소 후의 값**을 반환합니다. N개 워커 중 마지막 워커만 반환값 0을 받으므로, 그 한 명만 `SetEvent(m_hCompleteEvent)`를 호출합니다.

</details>

---

## Q8. `_InterlockedDecrement` 대신 일반 `m_lActiveThreadCount--`를 쓰면 일어날 수 있는 문제는?

1. 컴파일 오류
2. 두 워커가 같은 값을 동시에 읽어 카운트가 잘못되고, `SetEvent(hCompleteEvent)`가 호출되지 않아 메인이 영원히 대기
3. 카운트가 음수가 됨
4. 워커가 종료되지 않음

<details><summary>정답 보기</summary>

**정답: 2**

read-modify-write가 원자적이지 않으면 race가 생겨 마지막 워커가 0을 못 보거나 여러 명이 0을 봐서 결과가 망가집니다. 결과적으로 메인의 `WaitForSingleObject(hCompleteEvent)`가 풀리지 않는 데드락이 가능합니다.

</details>

---

## Q9. `pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)`의 `0`이 의미하는 것은?

1. Descriptor heap 번호
2. Root Signature의 root parameter index
3. Shader register slot 번호
4. Descriptor table의 offset

<details><summary>정답 보기</summary>

**정답: 2**

`0`은 Root Signature에 정의된 root parameter index입니다. 메시 root signature에서 `rootParameters[0]`은 per-object CBV table, `rootParameters[1]`은 tri-group SRV table입니다.

</details>

---

## Q10. `gpuDescriptorTable`과 `pDescriptorHeap`은 어떻게 연결되어 있는가?

1. 서로 무관한 별개의 객체
2. `gpuDescriptorTable`이 `pDescriptorHeap`으로 복사된다
3. `pDescriptorHeap = pDescriptorPool->m_pDescritorHeap`, `gpuDescriptorTable = 그 heap의 GPU 시작 주소 + offset`이므로 같은 heap을 가리킨다
4. `SetDescriptorHeaps` 호출이 둘을 묶어준다

<details><summary>정답 보기</summary>

**정답: 3**

`CDescriptorPool`이 만든 `m_pDescritorHeap`이 `INL_GetDescriptorHeap()`으로 `pDescriptorHeap`이 되고, `AllocDescriptorTable()`이 그 heap의 GPU 시작 주소에 offset을 더해 `gpuDescriptorTable`을 만듭니다. 둘은 같은 heap 기준입니다.

</details>

---

## Q11. `SetDescriptorHeaps(1, &pDescriptorHeap)`이 하는 일은?

1. descriptor를 heap으로 복사
2. 현재 CommandList에서 사용할 shader-visible descriptor heap을 바인딩
3. heap 객체를 새로 생성
4. root signature를 설정

<details><summary>정답 보기</summary>

**정답: 2**

복사가 아니라 “이번에 사용할 descriptor heap은 이거다”라고 command list에 알리는 호출입니다. 실제 descriptor 복사는 `CopyDescriptorsSimple()`에서 일어납니다.

</details>

---

## Q12. ConstantBufferPool / DescriptorPool / CommandListPool은 언제 생성되는가?

1. 매 프레임 BeginRender에서
2. 워커 스레드가 시작될 때마다
3. `Initialize()`에서 `[프레임 수 × 스레드 수]`만큼 한 번에 생성
4. 첫 Draw 호출 시 lazy 생성

<details><summary>정답 보기</summary>

**정답: 3**

`Initialize()`의 2중 루프 `for(frame) for(thread)`에서 `MAX_PENDING_FRAME_COUNT × m_dwRenderThreadCount`만큼 미리 전부 생성됩니다.

</details>

---

## Q13. `Present()`에서 다음 frame context의 Pool들을 reset하기 직전에 `WaitForFenceValue(...)`를 호출하는 이유는?

1. CPU 캐시를 비우기 위해
2. 그 frame context의 리소스를 GPU가 아직 쓰고 있을 수 있어, 다 썼다는 것을 확인하기 위해
3. SwapChain 인덱스를 동기화하기 위해
4. Descriptor heap 크기를 재조정하기 위해

<details><summary>정답 보기</summary>

**정답: 2**

CommandList/Descriptor/ConstantBuffer는 GPU가 실행 중일 수 있으므로, 해당 context의 마지막 fence가 완료되기 전에 reset/재사용하면 GPU가 잘못된 데이터를 읽게 됩니다.

</details>

---

## Q14. EndRender에서 메인이 `SetEvent(PROCESS_i)`를 N번 호출한 직후 바로 호출하는 함수는?

1. `Present()`
2. `WaitForSingleObject(m_hCompleteEvent, INFINITE)`
3. `ProcessByThread()`
4. `ExecuteCommandLists()`

<details><summary>정답 보기</summary>

**정답: 2**

N명 워커를 깨운 다음 메인은 `m_hCompleteEvent`를 기다립니다. 마지막 워커가 `_InterlockedDecrement → 0`을 보고 `SetEvent(m_hCompleteEvent)`를 호출하면 메인이 깨어나 Present barrier로 진행합니다.

</details>

---

## Q15. Present용 barrier(RT → PRESENT)와 그 `CloseAndExecute`를 메인 스레드가 직접 하는 가장 큰 이유는?

1. 코드 단순화
2. `ExecuteCommandLists`의 호출 순서가 GPU 실행 순서이므로 모든 워커 제출 후에 barrier가 와야 하고, 단일 제출자가 순서 보장에 가장 안전하기 때문
3. 워커가 barrier를 만들 수 없는 D3D12 제약 때문
4. Descriptor heap이 부족해서

<details><summary>정답 보기</summary>

**정답: 2**

`ExecuteCommandLists` 제출 순서가 곧 GPU 실행 순서입니다. 워커들이 draw CL을 모두 제출한 뒤에 메인이 단일 스레드로 barrier CL을 마지막에 제출해야 “draw → barrier → Present” 순서가 보장됩니다.

</details>

---

## Q16. 워커가 `ProcessByThread()`를 실행 중일 때 메인이 `SetEvent(DESTROY)`를 보내면?

1. 워커가 즉시 강제 종료된다
2. 무시된다
3. 워커는 현재 작업을 끝까지 마치고, 다음 `WaitForMultipleObjects`에서 DESTROY를 감지해 `_endthreadex(0)`로 종료한다
4. CommandList가 손상된다

<details><summary>정답 보기</summary>

**정답: 3**

`SetEvent`는 wait 중인 스레드를 깨우는 동기화 객체이지 실행 중인 함수를 끊는 인터럽트가 아닙니다. auto-reset event는 signaled 상태로 남았다가 워커가 다음 wait에 도달하면 즉시 통과합니다 (cooperative shutdown).

</details>

---

## Q17. `CloseHandle(hThread)`의 의미로 가장 정확한 것은?

1. 스레드를 강제로 종료시킨다
2. 스레드 객체에 대한 내 핸들 참조를 반납한다 (스레드는 이미 `_endthreadex`로 끝나 있음)
3. 스레드를 일시정지한다
4. 스레드의 우선순위를 낮춘다

<details><summary>정답 보기</summary>

**정답: 2**

`CloseHandle`은 종료가 아닌 “정리”입니다. 메인은 `WaitForSingleObject(hThread)`로 종료를 확인한 뒤에 핸들을 반납합니다.

</details>

---

## Q18. `WaitForSingleObject(hThread, INFINITE)`가 풀리는 조건은?

1. 누군가 `SetEvent(hThread)`를 호출
2. 워커가 `_endthreadex(0)` 등으로 종료되어 Windows 커널이 그 스레드 객체를 signaled 상태로 만듦
3. timeout이 INFINITE이라 절대 풀리지 않음
4. CPU usage가 0이 되면

<details><summary>정답 보기</summary>

**정답: 2**

스레드 핸들은 Event가 아닙니다. 해당 스레드 함수가 종료되면 커널이 thread object를 signaled로 전환하고, 그 시점에 `WaitForSingleObject(hThread)`가 풀립니다.

</details>

---

## Q19. 메시 `Draw()` 안에서 한 번의 draw에 대해 `pConstantBufferPool->Alloc()`이 하는 일은?

1. 새로운 ID3D12Resource를 매번 생성
2. 큰 ConstantBuffer 영역에서 이번 draw 전용 일부분을 잘라 `CB_CONTAINER` 형태로 반환
3. ConstantBuffer를 GPU에 즉시 업로드
4. Descriptor heap에서 CBV descriptor를 자동 복사

<details><summary>정답 보기</summary>

**정답: 2**

ConstantBufferPool은 하나의 큰 upload heap을 미리 잡아두고 draw마다 offset만 잘라 줍니다(`CB_CONTAINER`의 `pSystemMemAddr`/`CBVHandle`). draw마다 새 리소스를 만들지 않습니다. 또한 CBV는 직접 `CopyDescriptorsSimple()`로 descriptor table에 복사해야 합니다.

</details>

---

## Q20. 같은 워커 인덱스 `i`에서 한 프레임 동안 묶여 사용되는 리소스 묶음은?

1. `RenderQueue[i]`, `CommandListPool[f][i]`, `DescriptorPool[f][i]`, `ConstBufferManager[f][i]`
2. `RenderQueue[f][i]`, `CommandListPool[i]`, `DescriptorPool[i]`, `ConstBufferManager[i]`
3. 모두 공용 단일 인스턴스
4. `RenderQueue[i]`, `CommandListPool[i]`, `DescriptorPool[i]`, `ConstBufferManager[i]` (frame 축 없음)

<details><summary>정답 보기</summary>

**정답: 1**

RenderQueue만 `[thread]` 1차원이고, CommandListPool/DescriptorPool/ConstBufferManager는 `[frame][thread]` 2차원입니다. 워커 i는 현재 frame `f`에서 자기 인덱스 슬롯만 사용하므로 lock 없이 안전합니다.

</details>

---
