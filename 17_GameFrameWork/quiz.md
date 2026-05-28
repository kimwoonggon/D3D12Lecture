# Chapter 17 — GameFrameWork 퀴즈 (객관식)

> 각 문제의 **정답 보기 ▼** 를 클릭하면 정답이 나옵니다.

---

## Q1. Chapter 17에서 가장 핵심적인 아키텍처 변화는?

① `MAX_DRAW_COUNT_PER_FRAME`을 256에서 1024로 늘린 것  
② 전역 변수 기반 절차적 구조에서 `CGame` + `CGameObject` 클래스 계층으로 전환한 것  
③ `CTextureManager`를 별도 클래스로 분리한 것  
④ 카메라 View 행렬 계산 방식을 변경한 것  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

Chapter 16은 `CD3D12Renderer*`, `void* g_pMeshObj0` 등 10개 이상의 전역 변수와 전역 함수(`RunGame`, `Update`)로 구성된 절차적 구조였다. Chapter 17은 `CGame`(게임 총괄 매니저)과 `CGameObject`(개별 3D 오브젝트)를 도입해 게임 프레임워크의 기초를 만들었다. `main.cpp`의 전역 변수는 `CGame* g_pGame` 단 하나로 줄었다.

</details></td></tr></table>

---

## Q2. `CGameObject::m_bUpdateTransform`(Dirty Flag)이 `TRUE`로 설정되는 시점은?

① 매 프레임 `Run()`이 호출될 때  
② `Render()`가 호출될 때  
③ `SetPosition()`, `SetScale()`, `SetRotationY()` 중 하나가 호출될 때  
④ `UpdateTransform()`이 완료된 후  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
void CGameObject::SetPosition(float x, float y, float z) {
    m_matTrans = XMMatrixTranslation(x, y, z);
    m_bUpdateTransform = TRUE;  // Dirty 플래그 ON
}
```

트랜스폼 값이 변경될 때만 플래그를 켜고, `Run()`에서 플래그가 `TRUE`일 때만 `UpdateTransform()`을 호출한다. 변경이 없으면 World 행렬 재계산을 건너뛰어 1,000개 오브젝트의 행렬 곱셈 비용을 절약한다.

</details></td></tr></table>

---

## Q3. `CGameObject::UpdateTransform()`에서 World 행렬을 계산하는 올바른 순서는?

① Translation × Rotation × Scale  
② Rotation × Scale × Translation  
③ Scale × Rotation × Translation  
④ Scale × Translation × Rotation  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
m_matWorld = XMMatrixMultiply(m_matScale, m_matRot);
m_matWorld = XMMatrixMultiply(m_matWorld, m_matTrans);
// World = Scale × Rotation × Translation (SRT 순서)
```

SRT 순서를 지켜야 오브젝트 자신의 중심을 기준으로 스케일 적용 → 중심 기준 회전 → 월드 위치로 이동이 올바르게 적용된다. 순서가 바뀌면 회전 축이나 스케일 방향이 달라진다.

</details></td></tr></table>

---

## Q4. `SORT_LINK` 연결 리스트에서 노드로 연결되는 것은 무엇인가?

① `CGameObject*` 포인터  
② `CGameObject` 객체 전체 (값 복사)  
③ `CGameObject::m_LinkInGame` 멤버의 주소  
④ `CGame` 포인터  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
LinkToLinkedListFIFO(&m_pGameObjLinkHead, &m_pGameObjLinkTail,
                     &pGameObj->m_LinkInGame);  // m_LinkInGame 주소
```

리스트엔 `m_LinkInGame`(SORT_LINK 멤버)의 주소가 들어가고, `SORT_LINK::pItem`이 `CGameObject` 전체 시작 주소를 저장한다. 순회 시 `(CGameObject*)pCur->pItem`으로 오브젝트를 역참조한다. `CGameObject` 내부에 노드를 내장하므로 별도 힙 할당이 필요 없다.

</details></td></tr></table>

---

## Q5. `CGame::Update()`에서 `CurTick - m_PrvUpdateTick < 16`이면 `return FALSE`를 하는 이유는?

① GPU 렌더링이 아직 완료되지 않았기 때문에  
② 게임 로직 업데이트를 약 60FPS(16ms 간격)로 제한하기 위해  
③ `GetTickCount64()`가 16ms 단위로만 업데이트되기 때문에  
④ 1,000개 오브젝트 업데이트에 최소 16ms가 걸리기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

1초 = 1000ms이고 1000 ÷ 16 ≈ 62.5회 ≈ 60FPS이다. `Run()`은 매 프레임(240Hz에서는 초당 240번) 호출되지만, 게임 오브젝트 이동/물리 등 **게임 로직은 60FPS로 고정**해야 고사양 PC와 저사양 PC에서 동일한 게임 속도가 보장된다. Render()는 제한 없이 매 프레임 실행된다.

</details></td></tr></table>

---

## Q6. `m_CamPos = XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f)`에서 4번째 원소(w)가 `1.0f`인 이유는?

① DirectX 내부 계산에서 w=1이 기본값이기 때문에  
② 동차좌표계에서 **위치(Point)**는 w=1이어야 Translation 행렬의 이동 성분이 올바르게 적용되기 때문에  
③ XMVECTOR가 4개 원소를 항상 채워야 하기 때문에  
④ 카메라 거리를 나타내는 값이기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

동차좌표계(Homogeneous Coordinates)에서:
- **위치(Point)**: w=1 → Translation 행렬과 곱할 때 이동 성분(tx, ty, tz)이 더해짐
- **방향(Direction)**: w=0 → Translation이 무시됨 (카메라 방향은 이동해도 불변)

```
┌1 0 0 tx┐   ┌x┐   ┌x + tx·w┐
│0 1 0 ty│ × │y│ = │y + ty·w│
│0 0 1 tz│   │z│   │z + tz·w│
└0 0 0  1┘   └w┘   └   w    ┘
```

`m_CamDir`과 `Up` 벡터는 방향이므로 w=0.0f이다.

</details></td></tr></table>

---

## Q7. `WM_KEYDOWN` 메시지의 `lParam`에서 `(0x00ff0000 & lParam) >> 16`이 추출하는 것은?

① 키의 반복 횟수(Repeat Count)  
② 이전 키 상태(Previous Key State)  
③ 하드웨어 스캔 코드(Scan Code)  
④ 가상 키 코드(Virtual Key Code)  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

lParam 비트 구조:
```
 31  30  29  28~24  23~16      15      0~14
 [TR][PR][CT][미사용][ScanCode][Extend][RepeatCount]
```

`0x00ff0000`으로 비트 16~23을 마스킹한 뒤 `>> 16`으로 0번 위치로 이동시켜 스캔 코드를 추출한다. 스캔 코드는 키보드의 **물리적 위치 번호**이며 언어/레이아웃에 무관하다. 이후 `MapVirtualKey()`로 논리적 Virtual Key Code로 변환한다.

</details></td></tr></table>

---

## Q8. `if (!(lParam & 0x40000000))` 조건으로 `OnKeyDown` 호출을 제한하는 이유는?

① `lParam`이 음수일 때 오류를 방지하기 위해  
② 키를 꾹 누를 때 Windows가 반복 전송하는 `WM_KEYDOWN`(auto-repeat)을 무시하고 최초 누름 1번만 처리하기 위해  
③ ALT 키가 눌린 상태를 제외하기 위해  
④ 키패드 숫자키와 일반 숫자키를 구분하기 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`lParam`의 비트 30은 "Previous Key State"로, 키가 이전에도 눌려 있었으면 1이다. 키를 꾹 누르면 Windows는 `WM_KEYDOWN`을 수십 번 연속으로 보내는데(auto-repeat), 이때 비트 30=1이다. `!(lParam & 0x40000000)`가 TRUE인 경우(비트 30=0)만 `OnKeyDown`을 호출하면 **최초 누름 1번**만 처리된다. "총 발사", "점프" 같은 1회성 동작에서 필수적인 패턴이다.

</details></td></tr></table>

---

## Q9. `MapVirtualKey(uiScanCode, MAPVK_VSC_TO_VK)`를 사용하는 이유는? `wParam`에도 VK 코드가 있는데.

① `wParam`의 VK 코드는 ASCII 코드와 달라서  
② `wParam`은 좌/우 Shift를 모두 `VK_SHIFT`로 반환해 구분이 불가능하지만, 스캔 코드 경유 방식은 향후 `MAPVK_VSC_TO_VK_EX`로 좌/우 구분이 가능하기 때문에  
③ `wParam`은 한글 IME가 켜진 상태에서 잘못된 값을 반환하기 때문에  
④ `MapVirtualKey`가 `wParam`보다 항상 더 빠르기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`wParam`은 좌Shift와 우Shift를 모두 `VK_SHIFT(0x10)`으로 반환한다. 스캔 코드를 경유해 `MAPVK_VSC_TO_VK_EX`를 사용하면 `VK_LSHIFT(0xA0)` / `VK_RSHIFT(0xA1)`으로 구분할 수 있다. 현재 코드는 `MAPVK_VSC_TO_VK`를 쓰므로 실질적 차이는 없지만, 향후 확장을 위해 스캔 코드 경유 패턴을 미리 적용한 것이다.

</details></td></tr></table>

---

## Q10. Ch17에서 `MAX_DRAW_COUNT_PER_FRAME`을 256에서 1024로 늘린 직접적인 이유는?

① 텍스처 해상도가 높아져서 디스크립터가 더 필요해졌기 때문에  
② 1,000개 오브젝트를 렌더링하면 프레임당 9,002개 슬롯이 필요하여 기존 256×9=2,304개로는 부족하기 때문에  
③ D3D12 Agility SDK 버전 업그레이드로 최소값이 변경되었기 때문에  
④ 스프라이트 렌더링이 추가되어 2개 슬롯이 더 필요해졌기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```
프레임당 소비 슬롯:
- CGameObject 1,000개 × 9슬롯(CBV1+SRV6+여분2) = 9,000
- RenderSpriteWithTex 1회 × 2슬롯 = 2
합계 = 9,002

DescriptorPool 크기 = MAX_DRAW_COUNT × MAX_DESCRIPTOR_COUNT_FOR_DRAW
  Ch16: 256  × 9 = 2,304  → 9,002 슬롯 필요 시 AllocDescriptorTable 실패
  Ch17: 1024 × 9 = 9,216  → 충분
```

256으로는 약 256번째 오브젝트를 그리다가 할당 실패로 렌더링이 멈춘다.

</details></td></tr></table>

---

## Q11. CPU가 텍스처 데이터를 `pUploadBuffer`를 거쳐 `pTexResource`로 복사하는 2단계 방식을 쓰는 이유는?

① D3D12 API가 직접 쓰기를 금지하기 때문에  
② `pTexResource`는 `HEAP_TYPE_DEFAULT`(GPU 전용 VRAM)에 있어 CPU가 직접 접근할 수 없고, `pUploadBuffer`(HEAP_TYPE_UPLOAD)는 CPU/GPU 모두 접근 가능한 중간 다리이기 때문에  
③ `pTexResource`가 읽기 전용이기 때문에  
④ 압축 포맷(DDS)을 중간에서 디코딩해야 하기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```
CPU RAM          UploadBuffer(PCIe 경유)     GPU VRAM
m_pTextImage  →  pUploadBuffer          →   pTexResource
CPU 쓰기 가능    CPU·GPU 모두 접근 가능       GPU 고속 접근
(Map/Unmap)      (느림, 중간 다리)            CPU 직접 불가
```

GPU가 렌더링 시 텍스처를 수백만 번 샘플링하므로 VRAM에 있어야 최대 성능이 나온다. CPU가 가끔 업데이트하는 비용(1회 복사)보다 GPU가 매 픽셀마다 느린 메모리에 접근하는 비용이 훨씬 크다.

</details></td></tr></table>

---

## Q12. `CSpriteObject::DESCRIPTOR_COUNT_FOR_DRAW`가 2인 이유는?

① 스프라이트는 앞면/뒷면 두 장을 그리기 때문에  
② 스프라이트를 그리는 데 셰이더가 CBV(상수 버퍼) 1개 + SRV(텍스처) 1개, 총 2개의 descriptor가 필요하기 때문에  
③ 더블 버퍼링으로 두 프레임의 데이터를 동시에 유지하기 때문에  
④ 스프라이트 정점 버퍼와 인덱스 버퍼 각 1개씩이기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
enum SPRITE_DESCRIPTOR_INDEX {
    SPRITE_DESCRIPTOR_INDEX_CBV = 0,  // 위치/스케일/Z/해상도 등 상수
    SPRITE_DESCRIPTOR_INDEX_TEX = 1   // 화면에 그릴 텍스처 이미지
};
static const UINT DESCRIPTOR_COUNT_FOR_DRAW = 2;
```

BasicMeshObject(6면 박스)는 CBV 1 + SRV 6 = 7개가 필요한 것과 달리, 스프라이트는 항상 **사각형 1장에 텍스처 1장**이므로 2개로 고정이다.

</details></td></tr></table>

---

## Q13. `DescriptorPool`의 `AllocDescriptorTable()`은 어떤 할당 방식을 사용하는가?

① `malloc/free` 기반 동적 할당  
② 포인터를 앞으로 밀기만 하는 선형 할당 (Linear Allocator), 프레임 끝에 카운터를 0으로 리셋  
③ 해제된 슬롯을 재사용하는 풀 할당  
④ 프레임마다 전체 힙을 새로 생성하고 소멸  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
// 할당: 포인터(m_AllocatedDescriptorCount)를 앞으로 밈
*pOutCPUDescriptor = 시작주소 + (AllocatedCount × 슬롯크기);
m_AllocatedDescriptorCount += DescriptorCount;

// 프레임 끝 리셋: 카운터만 0으로
m_AllocatedDescriptorCount = 0;  // 실제 데이터 삭제 없음 → O(1)
```

draw call마다 슬롯을 새로 잡는 이유는 GPU가 커맨드를 **나중에 순서대로 실행**하므로 같은 슬롯을 덮어쓰면 이전 draw call 실행 시 데이터가 오염되기 때문이다.

</details></td></tr></table>

---

## Q14. `CGameObject::m_pRenderer`는 1,000개 오브젝트가 각각 독립적인 렌더러를 갖는가, 아니면 공유하는가?

① 각자 독립적인 `CD3D12Renderer`를 `new`로 생성한다  
② 하나의 `CD3D12Renderer` 객체를 공유하며, 각 오브젝트는 그 주소(포인터)만 복사해서 저장한다  
③ `static` 멤버로 선언되어 클래스 전체가 1개를 공유한다  
④ `CGame`이 렌더링 시 임시로 주입하고 끝나면 회수한다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
// CGameObject::Initialize()
m_pRenderer = pGame->INL_GetRenderer();
// new가 아니라 CGame이 보유한 포인터값(주소)만 복사
```

`CD3D12Renderer` 객체는 `CGame::Initialize()`에서 **딱 1번** `new`로 생성된다. 1,000개 오브젝트의 `m_pRenderer` 변수는 모두 같은 주소(예: 0x5000)를 담고 있다. 렌더러를 복사하는 비용 없이 모든 오브젝트가 동일한 D3D12 디바이스, 커맨드 리스트, 텍스처 매니저를 공유한다.

</details></td></tr></table>

---

## Q15. `CGame::DeleteGameObject(CGameObject* pGameObj)`에서 오브젝트 삭제의 시간 복잡도는?

① O(n) — 연결 리스트를 앞에서부터 탐색해야 하므로  
② O(log n) — 내부적으로 이진 탐색을 사용하므로  
③ O(1) — 양방향 리스트라 `pPrv`/`pNext`로 이전·다음 노드를 즉시 알 수 있으므로  
④ O(n²) — 1,000개를 모두 재정렬해야 하므로  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
void CGame::DeleteGameObject(CGameObject* pGameObj)
{
    UnLinkFromLinkedList(
        &m_pGameObjLinkHead,
        &m_pGameObjLinkTail,
        &pGameObj->m_LinkInGame   // pPrv, pNext를 이미 알고 있음
    );
    delete pGameObj;
}
```

`SORT_LINK`는 `pPrv`(이전 노드)와 `pNext`(다음 노드) 양쪽을 저장하는 **양방향 리스트**이므로, 삭제 대상의 포인터 하나만 있으면 앞뒤 노드의 연결을 즉시 수정할 수 있다. 탐색이 필요 없어 O(1)이다.

</details></td></tr></table>

---

## Q16. Ch17에서 `CGame::Run()`의 실행 순서로 올바른 것은?

① Render → Update → Present  
② Update → Render → Present  
③ Present → Update → Render  
④ Update와 Render를 번갈아 반복 후 Present  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
void CGame::Run()
{
    ULONGLONG CurTick = GetTickCount64();

    Update(CurTick);   // ① 게임 로직 (카메라 이동, 오브젝트 Run)
    Render();          // ② 렌더링 (BeginRender → 오브젝트 Render → EndRender → Present)
    // FPS 측정
}
```

`Update` 먼저 실행해 모든 오브젝트의 World 행렬을 최신 상태로 만든 뒤, `Render`에서 그 행렬로 그린다. 순서가 바뀌면 한 프레임 뒤처진 상태로 렌더링된다.

</details></td></tr></table>

---

## Q17. Ch17의 `CDescriptorPool`이 `[MAX_PENDING_FRAME_COUNT]` 배열로 여러 개인 이유는?

① 오브젝트 종류(메시, 스프라이트)마다 별도 풀이 필요하기 때문에  
② CPU가 다음 프레임 커맨드를 준비하는 동안 GPU가 이전 프레임 커맨드를 실행 중이므로, 같은 풀을 덮어쓰면 GPU가 읽는 데이터가 오염되기 때문에  
③ D3D12 API가 단일 DescriptorPool을 허용하지 않기 때문에  
④ 멀티스레드 렌더링을 지원하기 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```
프레임 N:   CPU → Pool[N%2]에 커맨드 기록
            GPU → Pool[(N-1)%2]의 데이터 읽으며 실행 중

Pool이 1개뿐이라면:
  CPU가 Pool 리셋하고 덮어씀
  GPU가 같은 Pool을 읽는 중 → 데이터 오염!
```

`Present()` 내에서 `WaitForFenceValue()`로 이전 프레임 GPU 완료를 확인한 뒤 해당 인덱스의 Pool을 `Reset()`한다.

</details></td></tr></table>
