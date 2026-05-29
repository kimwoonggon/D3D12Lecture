# Chapter 18 — RenderQueue 퀴즈 (객관식)

> 각 문제의 **정답 보기 ▼** 를 클릭하면 정답이 나옵니다.

---

## Q1. Chapter 18의 핵심 아키텍처 변화는?

① `DescriptorPool` 크기를 늘린 것  
② 렌더링 함수 호출 시 즉시 CommandList에 기록하던 방식을 → 큐에 아이템을 적재했다가 `EndRender()`에서 일괄 처리하는 방식으로 전환한 것  
③ `GameObject`를 멀티스레드로 업데이트하도록 변경한 것  
④ `SwapChain`의 버퍼 개수를 늘린 것  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

ch17에서는 `RenderMeshObject()`가 호출되는 즉시 `pMeshObj->Draw(pCommandList)`로 CommandList에 기록했다.  
ch18에서는 `RenderMeshObject()`가 `RENDER_ITEM`을 만들어 `m_pRenderQueue->Add()`로 CPU 버퍼에 적재만 하고, 실제 CommandList 기록은 `EndRender()` → `Process()`에서 일괄 처리한다.  
현재는 결과물이 동일하지만, `Process()` 전에 정렬/컬링을 끼워넣을 수 있는 구조적 기반이 된다.

</details></td></tr></table>

---

## Q2. `CRenderQueue::Reset()`이 하는 일은?

① `m_pBuffer`를 `free()`하고 `nullptr`로 설정한다  
② GPU가 처리 중인 렌더링 작업이 완료될 때까지 대기한다  
③ `m_dwAllocatedSize`와 `m_dwReadBufferPos`를 0으로 설정한다 (버퍼 메모리는 유지)  
④ `RENDER_ITEM` 배열을 `memset(0)`으로 초기화한다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
void CRenderQueue::Reset() {
    m_dwAllocatedSize = 0;    // write 포인터만 0으로
    m_dwReadBufferPos = 0;    // read 포인터만 0으로
}
// m_pBuffer 자체는 free하지 않음 → 다음 프레임에 재사용
```

버퍼 메모리는 `Initialize()`에서 한 번 `malloc`하고 `Cleanup()`에서 `free`한다.  
`Reset()`은 포인터만 초기화해 다음 프레임에서 같은 버퍼에 덮어쓰게 한다.  
`malloc/free` 오버헤드 없이 매 프레임 재사용하는 것이 목적이다.

</details></td></tr></table>

---

## Q3. `RENDER_ITEM` 구조체에서 `union`을 사용하는 이유는?

① C++ 표준에서 구조체 내 두 가지 타입을 동시에 사용하려면 union이 필수이기 때문에  
② `RENDER_MESH_OBJ_PARAM`과 `RENDER_SPRITE_PARAM` 중 한 번에 하나만 유효하므로 같은 메모리 공간을 공유해 크기를 줄이기 위해  
③ `memcpy` 시 union 멤버가 자동으로 복사되기 때문에  
④ GPU가 union 타입만 읽을 수 있기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
struct RENDER_ITEM {
    RENDER_ITEM_TYPE Type;
    void* pObjHandle;
    union {
        RENDER_MESH_OBJ_PARAM  MeshObjParam;  // Type이 MESH_OBJ일 때만 유효
        RENDER_SPRITE_PARAM    SpriteParam;   // Type이 SPRITE일 때만 유효
    };
};
```

메시 아이템이라면 `SpriteParam`이, 스프라이트 아이템이라면 `MeshObjParam`이 필요 없다.  
`union`으로 두 구조체가 같은 메모리를 공유하므로 큰 쪽의 크기만 차지한다.  
8192개 아이템 버퍼에서 메모리 절약 효과가 있다.

</details></td></tr></table>

---

## Q4. `CRenderQueue::Add()`가 `FALSE`를 반환하는 조건은?

① `pItem->Type`이 정의되지 않은 타입일 때  
② `m_dwAllocatedSize + sizeof(RENDER_ITEM) > m_dwMaxBufferSize`일 때 (버퍼 초과)  
③ GPU가 아직 이전 프레임을 처리 중일 때  
④ `pItem->pObjHandle`이 `nullptr`일 때  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
BOOL CRenderQueue::Add(const RENDER_ITEM* pItem) {
    if (m_dwAllocatedSize + sizeof(RENDER_ITEM) > m_dwMaxBufferSize)
        goto lb_return;  // FALSE 반환
    memcpy(m_pBuffer + m_dwAllocatedSize, pItem, sizeof(RENDER_ITEM));
    m_dwAllocatedSize += sizeof(RENDER_ITEM);
    ...
}
```

큐가 `Initialize(this, 8192)`로 생성되었으므로 최대 8192개 아이템까지 가능하다.  
`RenderMeshObject()` 등에서 `Add()`가 `FALSE`를 반환하면 `__debugbreak()`로 즉시 중단한다.

</details></td></tr></table>

---

## Q5. `CRenderQueue::Process()` 내부에서 스프라이트 아이템의 두 가지 분기 기준은?

① `RENDER_ITEM_TYPE`이 `MESH_OBJ`인지 `SPRITE`인지  
② `pItem->SpriteParam.bUseRect`가 TRUE인지 FALSE인지  
③ `pItem->SpriteParam.pTexHandle`이 `nullptr`인지 아닌지  
④ `pItem->SpriteParam.Z`가 0.0f보다 큰지 아닌지  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
case RENDER_ITEM_TYPE_SPRITE:
    if (pTexureHandle)              // 텍스처 있음
        pSpriteObj->DrawWithTex(pCommandList, ...);
    else                            // 텍스처 없음
        pSpriteObj->Draw(pCommandList, ...);
```

`pTexHandle != nullptr` → `DrawWithTex()` (SRV 바인딩 + 텍스처 샘플링)  
`pTexHandle == nullptr` → `Draw()` (버텍스 컬러만, 텍스처 없음)  
추가로 `pTexHandle->pUploadBuffer`가 존재하면 동적 텍스처이므로 `bUpdated` 플래그를 확인하여 CPU → GPU 업로드를 먼저 수행한다.

</details></td></tr></table>

---

## Q6. `Initialize(this, 8192)`에서 8192의 의미는?

① GPU CommandList에 기록할 수 있는 최대 Draw 호출 횟수  
② 큐가 수용할 수 있는 최대 `RENDER_ITEM` 개수 → `8192 × sizeof(RENDER_ITEM)` 바이트의 flat 버퍼를 `malloc`  
③ DescriptorPool의 최대 슬롯 수  
④ 프레임당 최대 텍스처 업로드 횟수  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
BOOL CRenderQueue::Initialize(CD3D12Renderer* pRenderer, DWORD dwMaxItemNum) {
    m_dwMaxBufferSize = sizeof(RENDER_ITEM) * dwMaxItemNum;  // 8192 × sizeof(RENDER_ITEM)
    m_pBuffer = (char*)malloc(m_dwMaxBufferSize);
}
```

`MAX_DRAW_COUNT_PER_FRAME = 1024`(오브젝트 최대 수)보다 훨씬 크게 잡은 이유는 스프라이트 등 추가 아이템을 고려한 여유분이다. 버퍼는 앱 시작 시 한 번만 할당되고 매 프레임 `Reset()`으로 재사용된다.

</details></td></tr></table>

---

## Q7. `EndRender()`에서 `Process()`와 `Reset()`의 호출 순서로 올바른 것은?

① `Reset()` → `Process()` → `ResourceBarrier` → `Close` → `ExecuteCommandLists`  
② `ResourceBarrier` → `Process()` → `Close` → `ExecuteCommandLists` → `Reset()`  
③ `Process()` → `ResourceBarrier` → `Close` → `ExecuteCommandLists` → `Reset()`  
④ `Process()` → `Reset()` → `ResourceBarrier` → `Close` → `ExecuteCommandLists`  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
void CD3D12Renderer::EndRender() {
    m_pRenderQueue->Process(pCommandList);   // ① Draw 명령 CommandList에 기록
    pCommandList->ResourceBarrier(...);      // ② RT → PRESENT 상태 전환
    pCommandList->Close();                   // ③ 기록 종료
    m_pCommandQueue->ExecuteCommandLists(...)// ④ GPU에 제출
    m_pRenderQueue->Reset();                 // ⑤ 다음 프레임 준비
}
```

`Process()` 전에 `Reset()`을 호출하면 큐가 비어있어 아무것도 그려지지 않는다.  
`ExecuteCommandLists()` 후 `Reset()`을 호출해야 GPU 제출이 완료된 후 안전하게 다음 프레임 데이터로 덮어쓸 수 있다.

</details></td></tr></table>

---

## Q8. `RENDER_ITEM` 안의 `pObjHandle`(포인터)과 `matWorld`(값)의 수명 차이는?

① 둘 다 1프레임 수명이다  
② `pObjHandle`은 앱 종료 시까지, `matWorld`는 `Reset()` 후 무효화된다  
③ `matWorld`는 앱 종료 시까지, `pObjHandle`은 `Reset()` 후 무효화된다  
④ 둘 다 `DeleteBasicMeshObject()` 호출 전까지 유효하다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`Add()`에서 `memcpy`로 복사되는 내용:
- `matWorld` → **값이 복사**됨 → `Reset()` 후 버퍼가 덮어씌워지면 소멸 → 1프레임 수명
- `pObjHandle` → **포인터(주소)가 복사**됨 → 큐가 리셋되어도 원본 `CBasicMeshObject`는 살아있음 → `DeleteBasicMeshObject()` 호출 전까지 유효

따라서 `Add()` 후 같은 프레임에 `DeleteBasicMeshObject(pObj)`를 호출하면 `Process()` 시점에 dangling pointer가 되어 크래시가 발생한다. `Delete*()` 함수 내부에서 `WaitForFenceValue()`로 GPU 작업 완료를 기다리는 이유가 이것이다.

</details></td></tr></table>

---

## Q9. ch17에서 ch18로 변경 후 즉각적인 성능 변화는?

① 정렬이 추가되어 GPU 효율이 올라간다  
② Frustum Culling이 추가되어 DrawCall이 줄어든다  
③ `RENDER_ITEM` 생성 + `memcpy` 오버헤드가 추가되어 오히려 약간 느려진다  
④ 멀티스레드 렌더링이 적용되어 빨라진다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

ch18의 `RenderMeshObject()`는:
1. `RENDER_ITEM` 구조체 생성
2. `memcpy`로 flat 버퍼에 복사
3. `Process()`에서 버퍼 순회 후 Draw

ch17의 `RenderMeshObject()`는 바로 `pMeshObj->Draw()`를 호출했다.  
ch18은 CPU 버퍼에 한 번 더 쓰고 읽는 단계가 추가되어 지금 당장은 오히려 느리다.  
정렬/컬링/멀티스레드가 구현되는 이후 챕터에서 비로소 성능 이득이 발생한다.

</details></td></tr></table>

---

## Q10. `pTexHandle->pUploadBuffer != nullptr`이 의미하는 것은?

① 텍스처가 파일에서 로드되었다  
② 텍스처가 GPU VRAM에 존재한다  
③ CPU에서 매 프레임 내용을 갱신할 수 있는 동적 텍스처이다  
④ 텍스처 업로드가 아직 완료되지 않았다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
struct TEXTURE_HANDLE {
    ID3D12Resource* pTexResource;   // GPU VRAM 텍스처
    ID3D12Resource* pUploadBuffer;  // CPU 접근 가능 업로드 버퍼
                                    // 파일 텍스처: nullptr
                                    // 동적 텍스처: 존재
    BOOL bUpdated;                  // CPU에서 내용이 바뀌었는지 플래그
};
```

- 파일 로드 텍스처(`CreateTextureFromFile`) → `pUploadBuffer = nullptr` → GPU에 한 번 올리고 끝
- 동적 텍스처(`CreateDynamicTexture`) → `pUploadBuffer` 존재 → 매 프레임 CPU에서 내용 변경 후 `bUpdated = TRUE`로 표시 → `Process()` 내부에서 CommandList에 복사 명령 기록

FPS 텍스트 오버레이가 대표적인 동적 텍스처 사용 사례다.

</details></td></tr></table>

---

## Q11. `CSpriteObject`의 버텍스/인덱스 버퍼가 `static`으로 선언된 이유는?

① D3D12에서 스프라이트 버퍼는 반드시 static이어야 하기 때문에  
② 모든 스프라이트는 항상 같은 사각형(quad) 형태이므로 인스턴스마다 개별 버퍼를 만들 필요가 없기 때문에  
③ static 버퍼가 GPU 캐시 히트율이 높기 때문에  
④ 멀티스레드 환경에서 static만 안전하기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

모든 `CSpriteObject` 인스턴스는 동일한 사각형(좌하~우상 4개 버텍스, 인덱스 6개)을 사용한다. 위치, 크기, UV는 `CONSTANT_BUFFER_SPRITE`(상수 버퍼)로 셰이더에 전달해 제어하므로 버텍스 데이터 자체는 항상 동일하다.

```cpp
// InitMesh()에서 딱 한 번만 GPU에 올림
static ID3D12Resource* m_pVertexBuffer;  // 1,000개 스프라이트가 이 버퍼 하나 공유
static ID3D12Resource* m_pIndexBuffer;
```

반면 `CBasicMeshObject`는 메시마다 형태가 다르므로 인스턴스별 개별 버퍼를 가진다.

</details></td></tr></table>

---

## Q12. `BasicMeshObject::Draw()`에서 TriGroup마다 `DrawIndexedInstanced()`를 별도로 호출하는 이유는?

① DirectX 12 API가 TriGroup당 하나의 Draw 호출을 요구하기 때문에  
② 각 TriGroup은 다른 텍스처(SRV)를 사용하므로 SRV를 교체한 뒤 각각 Draw해야 하기 때문에  
③ 인덱스 버퍼를 TriGroup마다 새로 생성하기 때문에  
④ 상수 버퍼를 TriGroup마다 다르게 설정해야 하기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
for (DWORD i = 0; i < m_dwTriGroupCount; i++)
{
    // TriGroup마다 다른 SRV(텍스처) 바인딩
    pCommandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorTableForTriGroup);
    gpuDescriptorTableForTriGroup.Offset(1, srvDescriptorSize);

    // 이 TriGroup의 인덱스 버퍼로 Draw
    pCommandList->IASetIndexBuffer(&pTriGroup->IndexBufferView);
    pCommandList->DrawIndexedInstanced(pTriGroup->dwTriCount * 3, 1, 0, 0, 0);
}
```

CBV(상수 버퍼)는 오브젝트당 1개로 공유하고, SRV(텍스처)만 TriGroup마다 다르다.  
텍스처를 교체하려면 DescriptorTable을 다시 바인딩해야 하므로 Draw 호출도 분리된다.

</details></td></tr></table>
