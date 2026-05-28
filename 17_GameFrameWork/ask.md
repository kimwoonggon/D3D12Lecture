# Chapter 17 Q&A

---

## Q1. Dirty Flag란 무엇인가?

**"값이 변경되었으니 다음 번에 재계산이 필요하다"는 표시를 남기는 최적화 기법**입니다.

매 프레임마다 World 행렬을 재계산하는 것은 낭비입니다. 오브젝트가 **움직이지 않았다면** 이전 결과를 그대로 쓰면 됩니다.

```cpp
// SetPosition이 불릴 때 → "계산이 필요하다"고 표시만 해둠
void CGameObject::SetPosition(float x, float y, float z) {
    m_matTrans = XMMatrixTranslation(x, y, z);
    m_bUpdateTransform = TRUE;  // ← dirty flag ON (더러워짐 = 재계산 필요)
}

// Run()에서 → 표시가 있을 때만 실제 계산 수행
void CGameObject::Run() {
    if (m_bUpdateTransform) {          // dirty?
        UpdateTransform();             // World 행렬 재계산
        m_bUpdateTransform = FALSE;    // ← dirty flag OFF (깨끗해짐)
    }
    // dirty가 아니면 아무것도 안 함 → CPU 절약
}
```

### 왜 "dirty(더럽다)"인가?

"원본 데이터(위치/회전)가 바뀌었는데 파생 데이터(World 행렬)는 아직 업데이트 안 됨 = **오래된/오염된 상태**"라는 의미에서 dirty라고 부릅니다.

| 상태 | `m_bUpdateTransform` | 의미 |
|------|----------------------|------|
| **Clean** | `FALSE` | World 행렬이 최신 상태 → 재계산 불필요 |
| **Dirty** | `TRUE` | 트랜스폼이 변경됨 → 다음 `Run()`에서 반드시 재계산 |

### 1,000개 오브젝트에서의 효과

Ch17에서 오브젝트는 초기화 시 한 번만 `SetPosition/SetRotation`을 호출합니다. 이후 프레임에서는 아무도 움직이지 않으므로 1,000번의 행렬 곱셈이 모두 스킵됩니다. 오브젝트가 애니메이션되는 이후 챕터에서 "움직이는 오브젝트만" 재계산하는 핵심 패턴으로 활용됩니다.

---

## Q2. `m_CamPos`의 4번째 원소(w)가 1.0f인 이유는?

```cpp
m_CamPos = XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f);  // w = 1.0f
m_CamDir = XMVectorSet(0.0f, 0.0f,  1.0f, 0.0f);  // w = 0.0f
XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // w = 0.0f
```

이것은 **동차좌표계(Homogeneous Coordinates)** 의 관례입니다.

| 벡터 종류 | w 값 | 의미 |
|-----------|------|------|
| **위치(Point)** | `1.0f` | "이동(Translation)의 영향을 받는다" |
| **방향(Direction)** | `0.0f` | "이동의 영향을 받지 않는다" |

4×4 변환 행렬에 곱할 때:

$$\begin{pmatrix} 1 & 0 & 0 & t_x \\ 0 & 1 & 0 & t_y \\ 0 & 0 & 1 & t_z \\ 0 & 0 & 0 & 1 \end{pmatrix} \begin{pmatrix} x \\ y \\ z \\ w \end{pmatrix} = \begin{pmatrix} x + t_x \cdot w \\ y + t_y \cdot w \\ z + t_z \cdot w \\ w \end{pmatrix}$$

- **위치** (`w=1`): 이동 성분이 더해짐 ✅
- **방향** (`w=0`): 이동이 무시됨 ✅ (카메라를 옮겨도 바라보는 방향은 불변)

> DirectX의 `XMMatrixLookToLH`는 내부적으로 xyz만 사용하므로 이 코드에서 w가 연산에 직접 영향을 미치지는 않지만, **"이것은 위치다 vs 방향이다"를 코드에 명시하는 관례**입니다.

---

## Q3. Scan Code vs Virtual Key Code란?

### Scan Code — 하드웨어의 언어

키보드가 "**어느 위치의 키가 눌렸다**"를 CPU에 전달하는 번호. 언어/OS 설정과 무관하게 **물리적 위치 번호**입니다.

```
키보드 물리 배열:
┌────┬────┬────┐
│ Q  │ W  │ E  │  ← QWERTY
│0x10│0x11│0x12│  ← 스캔 코드
└────┴────┴────┘

┌────┬────┬────┐
│ A  │ Z  │ E  │  ← AZERTY (프랑스)
│0x10│0x11│0x12│  ← 스캔 코드 (동일!)
└────┴────┴────┘
```

같은 위치의 키는 키보드 레이아웃에 상관없이 **스캔 코드가 같습니다.**

### Virtual Key Code — 소프트웨어의 언어

OS가 "**이 키는 논리적으로 어떤 기능이다**"라고 정의한 번호. `VK_*` 상수로 표현됩니다.

```cpp
VK_SHIFT  = 0x10   // Shift 키
VK_ESCAPE = 0x1B   // ESC 키
'A'       = 0x41   // 알파벳 A
'W'       = 0x57   // 알파벳 W
VK_LEFT   = 0x25   // 방향키 ←
VK_F1     = 0x70   // F1 키
```

### 핵심 차이 예시

한국어 IME 켠 상태에서 'ㅈ' 키(W 위치)를 누르면:

| | 값 | 의미 |
|--|---|------|
| **Scan Code** | `0x11` | "W 위치 키가 눌렸다" |
| **Virtual Key** | `'W'` (0x57) | "W 문자 키다" |
| **문자 입력** | `'ㅈ'` | IME가 변환한 최종 문자 |

게임 이동키는 **Virtual Key Code**를 씁니다. 한국어/영어 모드 관계없이 W 위치를 누르면 `0x57`이 오기 때문입니다.

---

## Q4. WM_KEYDOWN 코드 상세 분석

```cpp
case WM_KEYDOWN:
    UINT uiScanCode = (0x00ff0000 & lParam) >> 16;
    UINT vkCode = MapVirtualKey(uiScanCode, MAPVK_VSC_TO_VK);
    if (!(lParam & 0x40000000))
        g_pGame->OnKeyDown(vkCode, uiScanCode);
    break;
```

### lParam 32비트 구조

```
 31  30  29  28~24  23~16      15      0~14
 [TR][PR][CT][미사용][ScanCode][Extend][RepeatCount]
```

| 비트 | 이름 | 값의 의미 |
|------|------|----------|
| 비트 31 | Transition | 0=누름, 1=뗌 |
| **비트 30** | **Previous key state** | **0=이전에 뗀 상태(최초 누름), 1=이전에도 눌림(반복)** |
| 비트 29 | Context | ALT 눌림 여부 |
| 비트 16~23 | **Scan Code** | 하드웨어 키 번호 |
| 비트 0~15 | Repeat count | 반복 횟수 |

### ① 스캔 코드 추출

```cpp
UINT uiScanCode = (0x00ff0000 & lParam) >> 16;
//                 마스킹(비트 16~23만 남김)  >> 16(0번 위치로 이동)
```

### ② MapVirtualKey — 스캔 코드 → VK 변환

```cpp
UINT vkCode = MapVirtualKey(uiScanCode, MAPVK_VSC_TO_VK);
```

| 파라미터 | 설명 |
|----------|------|
| `uiScanCode` | 변환할 스캔 코드 |
| `MAPVK_VSC_TO_VK` | Scan Code → Virtual Key 방향 지정 |

`wParam`에도 VK 코드가 있는데 왜 스캔 코드를 경유하는가?
- `wParam`은 좌Shift/우Shift를 모두 `VK_SHIFT`로 반환 → 구분 불가
- 스캔 코드 경유 시 `MAPVK_VSC_TO_VK_EX`로 좌/우 구분 가능 (향후 확장성)

---

## Q5. `uiScanCode`를 OnKeyDown에 넘기는 이유 + auto-repeat이란?

### uiScanCode를 넘기는 이유

현재 코드에서 `OnKeyDown`은 `uiScanCode`를 **받기만 하고 실제로 사용하지 않습니다.**

```cpp
void CGame::OnKeyDown(UINT nChar, UINT uiScanCode)
{
    switch (nChar)  // nChar(VK코드)만 사용
    {
        case VK_SHIFT: ...
        case 'W': ...
    }
    // uiScanCode는 아무데도 안 씀
}
```

**그럼 왜 넘기는가?** → **향후 확장을 위한 인터페이스 설계**입니다. 나중에 좌/우 Shift 구분이나 특수 키 처리가 필요해질 때를 대비해 파라미터로 함께 전달해두는 관례입니다.

### auto-repeat(자동 반복)이란?

키보드의 **꾹 누르기 자동 반복** 기능입니다. 워드에서 'A'를 꾹 누르면 `aaaaaaaaaa...`처럼 계속 입력되는 것이 auto-repeat입니다.

Windows는 이것을 `WM_KEYDOWN`을 **반복해서 보내는 것**으로 구현합니다:

```
키 누름 ──────────────────────────────────── 키 뗌
         ↓    ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓      ↑
     WM_KEYDOWN  WM_KEYDOWN 연속 전송      WM_KEYUP
     (최초 1회)  (자동 반복, 수십 번)       (1회)
```

### 비트 30으로 최초 누름만 처리

```cpp
if (!(lParam & 0x40000000))   // 비트 30이 0 = 최초 누름만
    g_pGame->OnKeyDown(vkCode, uiScanCode);
```

| 상황 | 비트 30 | `OnKeyDown` 호출 |
|------|---------|-----------------|
| W 처음 누름 | `0` | ✅ 호출 |
| W 꾹 누르는 중 (반복) | `1` | ❌ 차단 |
| W 뗌 | — | `OnKeyUp` 호출 |

---

## Q6. W를 계속 누르면 auto-repeat이 발동하는데 그걸 무시하는 건가?

맞습니다.

```
W 꾹 누르는 동안:

        최초           반복(차단)     반복(차단)     반복(차단)
         ↓                ↓              ↓              ↓
 WM_KEYDOWN(비트30=0)  WM_KEYDOWN(1)  WM_KEYDOWN(1)  ...
         ↓                ×              ×              ×
   OnKeyDown 호출        무시           무시           무시
   m_CamOffsetZ=0.05f

              ↕ 매 프레임 Update()가 돌면서 0.05씩 이동 ↕

                                                      WM_KEYUP
                                                         ↓
                                                   OnKeyUp 호출
                                                   m_CamOffsetZ=0.0f
```

`OnKeyDown`은 딱 1번만 불러서 **"이동 시작"** 상태를 켜고, 실제 이동은 **매 프레임 `Update()`** 가 담당합니다.

이 코드에서는 `m_CamOffsetZ = 0.05f`를 여러 번 세팅해도 결과가 같아서 차단 여부가 무관하지만, "총 발사", "아이템 줍기" 같은 **1회성 동작**에서는 반드시 최초 누름 1번만 처리해야 하므로 올바른 설계 패턴으로 막아두는 것입니다.

---

## Q7. 링크드 리스트에는 CGameObject의 뭘 추가하는 건가?

`CGameObject` 객체 자체가 아니라, **객체 안에 내장된 `SORT_LINK m_LinkInGame` 멤버의 주소**를 리스트에 연결합니다.

```cpp
// CGame::CreateGameObject()
LinkToLinkedListFIFO(&m_pGameObjLinkHead, &m_pGameObjLinkTail,
                     &pGameObj->m_LinkInGame);  // m_LinkInGame의 주소
```

```
힙 메모리 (CGameObject가 0x1000번지에 있을 때):

0x1000  │ m_pRenderer        │
0x1008  │ m_pMeshObj         │
...     │ ...                │
0x10A0  │ SORT_LINK          │  ← 리스트에 연결되는 주소는 0x10A0
        │   pPrv             │
        │   pNext            │
        │   pItem = 0x1000   │  ← pItem이 CGameObject 시작 주소를 저장
```

리스트 순회 시 `pItem`을 통해 원본 오브젝트로 되돌아옵니다:

```cpp
CGameObject* pGameObj = (CGameObject*)pCur->pItem;  // 0x1000 꺼냄
pGameObj->Run();
```

**핵심**: 리스트엔 `m_LinkInGame` 주소가 들어가고, `pItem`을 통해 `CGameObject` 주소를 역참조하는 2단계 구조입니다.

---

## Q8. pPrv, pNext, pItem이 실제로 무엇을 가리키는가?

```
힙 메모리 상의 실제 모습 (오브젝트 3개):

┌──────────────────────────┐   ┌──────────────────────────┐   ┌──────────────────────────┐
│      CGameObject #1      │   │      CGameObject #2      │   │      CGameObject #3      │
│  ...                     │   │  ...                     │   │  ...                     │
│  ┌──────────────────┐    │   │  ┌──────────────────┐    │   │  ┌──────────────────┐    │
│  │ SORT_LINK        │    │   │  │ SORT_LINK        │    │   │  │ SORT_LINK        │    │
│  │ pPrv  = nullptr  │    │   │  │ pPrv  → #1 Link  │    │   │  │ pPrv  → #2 Link  │    │
│  │ pNext → #2 Link  │────┼──→│  │ pNext → #3 Link  │────┼──→│  │ pNext = nullptr  │    │
│  │ pItem → 자기자신 │    │   │  │ pItem → 자기자신 │    │   │  │ pItem → 자기자신 │    │
│  └──────────────────┘    │   │  └──────────────────┘    │   │  └──────────────────┘    │
└──────────────────────────┘   └──────────────────────────┘   └──────────────────────────┘
↑                                                                                        ↑
m_pGameObjLinkHead                                                          m_pGameObjLinkTail
```

- **`pPrv`** → 이전 `CGameObject`의 `m_LinkInGame` 주소 (첫 번째는 `nullptr`)
- **`pNext`** → 다음 `CGameObject`의 `m_LinkInGame` 주소 (마지막은 `nullptr`)
- **`pItem`** → 자기 자신인 `CGameObject` 전체의 시작 주소

---

## Q9. CurTick - m_PrvUpdateTick < 16이면 왜 60FPS인가?

1초 = 1000ms이고, 1000 ÷ 60 ≈ **16.6ms**이기 때문입니다.

```
1000ms ÷ 16ms = 62.5회 ≈ 60FPS
```

```
시간(ms): 0    16    32    48    64 ...
               ↓     ↓     ↓     ↓
           Update  Update Update Update  → 1초에 약 60번

구간 사이(0~15ms): CurTick - m_PrvUpdateTick < 16 → return FALSE → Update 스킵
```

`Run()`은 매 프레임(240Hz면 1초에 240번) 호출되지만, **게임 로직은 60FPS로 고정**하기 위한 제한입니다. 렌더링은 최대한 빠르게 하되, 게임 오브젝트 이동/물리 로직은 어떤 PC에서도 같은 속도가 나오도록 고정합니다.

---

## Q10. 코드에서 링크드 리스트는 총 몇 개인가?

**2개**입니다.

### 리스트 ① — CGame의 게임 오브젝트 리스트

```
소유자:   CGame
헤드/테일: m_pGameObjLinkHead / m_pGameObjLinkTail
노드:     CGameObject::m_LinkInGame
목적:     Update/Render 시 1,000개 오브젝트 순회
```

### 리스트 ② — CTextureManager의 텍스처 리스트

```
소유자:   CTextureManager
헤드/테일: m_pTexLinkHead / m_pTexLinkTail
노드:     TEXTURE_HANDLE::Link
목적:     생성된 텍스처 전체 추적 및 일괄 해제
```

```cpp
// typedef.h
struct TEXTURE_HANDLE {
    ID3D12Resource* pTexResource;
    ID3D12Resource* pUploadBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE srv;
    DWORD  dwRefCount;
    SORT_LINK Link;    // ← 리스트 노드
};
```

두 리스트는 완전히 다른 타입의 원소를 관리하며 교차되지 않습니다.

---

## Q11. CGameObject가 각각 달라도 하나의 링크드 리스트에 들어가는가?

네, **1개의 링크드 리스트에 1,000개의 서로 다른 CGameObject가 모두 들어갑니다.**

각 오브젝트는 위치, 회전, 월드 행렬이 모두 다르지만 `m_LinkInGame`이라는 같은 구조의 노드를 통해 하나의 리스트에 연결됩니다.

```cpp
for (DWORD i = 0; i < 1000; i++)
{
    CGameObject* pGameObj = new CGameObject;   // 서로 다른 힙 주소
    pGameObj->SetPosition(랜덤x, 0, 랜덤z);    // 서로 다른 위치
    pGameObj->SetRotationY(랜덤각도);

    LinkToLinkedListFIFO(
        &m_pGameObjLinkHead,
        &m_pGameObjLinkTail,
        &pGameObj->m_LinkInGame   // 전부 같은 하나의 리스트에 연결
    );
}
```

---

## Q12. 링크드 리스트 삭제는 포인터 하나만 있으면 O(1)로 삭제 가능한가?

**네, O(1) 삭제가 가능합니다.** 이유는 **양방향(doubly linked) 리스트**이기 때문입니다.

```
단방향 리스트 (O(n)):
Head → [A] → [B] → [C] → [D]
C를 삭제하려면 A부터 "다음이 C인 노드(B)"를 탐색해야 함

양방향 리스트 (O(1)):
Head → [A] ⇄ [B] ⇄ [C] ⇄ [D]
C는 pPrv로 B를, pNext로 D를 이미 알고 있음
→ B.pNext = D, D.pPrv = B  끝 → O(1)
```

```cpp
void CGame::DeleteGameObject(CGameObject* pGameObj)
{
    UnLinkFromLinkedList(
        &m_pGameObjLinkHead,
        &m_pGameObjLinkTail,
        &pGameObj->m_LinkInGame  // pPrv, pNext를 이미 알고 있음
    );
    delete pGameObj;
}
```

`pGameObj` 포인터 하나만 있으면 `m_LinkInGame.pPrv/pNext`로 즉시 앞뒤 노드에 접근해 연결을 끊을 수 있습니다.

---

## Q13. m_pRenderer는 오브젝트끼리 공유하는가 아니면 각자 하나씩인가?

**공유합니다.** `CD3D12Renderer` 객체는 프로그램 전체에서 **딱 1개**이고, 각 오브젝트는 그 주소(포인터)만 복사해서 들고 있습니다.

```
CD3D12Renderer (1개, 0x5000번지)
        ↑           ↑           ↑
   GameObject#1  GameObject#2  ... #1000
   m_pRenderer   m_pRenderer
   = 0x5000      = 0x5000      (전부 같은 주소)
```

```cpp
// CGameObject::Initialize()
m_pRenderer = pGame->INL_GetRenderer();
// 새로 만드는 게 아니라 CGame이 갖고 있는 포인터값을 복사
```

포인터 변수는 1,000개지만 전부 같은 주소를 담고 있습니다. 렌더러를 복사하는 비용 없이 모든 오브젝트가 동일한 D3D12 디바이스, 커맨드 리스트, 텍스처 매니저를 공유합니다.

---

## Q14. CreateSpriteObject()가 텍스트 표시에만 쓰이는가?

**Ch17에서는 텍스트 표시 딱 1곳에서만** 씁니다.

```cpp
// CGame::Initialize()
m_pSpriteObjCommon = m_pRenderer->CreateSpriteObject();  // 1번 생성

// CGame::Render()
m_pRenderer->RenderSpriteWithTex(
    m_pSpriteObjCommon,     // 스프라이트 파이프라인/셰이더
    512+5, 256+5+256+5,     // 화면 위치
    1.0f, 1.0f,
    nullptr, 0.0f,
    m_pTextTexTexHandle     // FPS 텍스트가 그려진 텍스처
);
```

Ch16에서는 `sprite_1024x1024.dds`를 4분할 배치하는 스프라이트 데모로 5번 호출했지만, Ch17은 게임 프레임워크 도입이 핵심이므로 FPS 텍스트 표시만 남겼습니다.

`m_pSpriteObjCommon`이 "Common(공통)"인 이유는 스프라이트 객체 1개를 만들고 `RenderSpriteWithTex`에 다른 텍스처 핸들을 넘기면 다양한 이미지를 렌더할 수 있기 때문입니다. 렌더 파이프라인(셰이더, 정점 버퍼)은 공유하고 텍스처만 교체하는 구조입니다.

---

## Q15. UploadBuffer에서 TexResource로 번거롭게 복사해야 하는 이유는?

CPU와 GPU는 **서로 직접 접근할 수 없는 별도의 메모리**를 사용하기 때문입니다.

```
CPU 영역 (RAM)                    GPU 영역 (VRAM)
┌────────────────┐                ┌─────────────────────────┐
│  m_pTextImage  │                │  pTexResource           │
│  (BYTE* 배열)  │                │  (실제 렌더링에 사용)    │
│  CPU가 자유롭게│                │  GPU 고속 접근           │
│  읽고 씀       │                │  CPU 직접 접근 불가      │
└────────────────┘                └─────────────────────────┘
         ↑ 직접 접근 불가 ↑
```

그래서 중간 다리(UploadBuffer)가 필요합니다:

```
CPU RAM       UploadBuffer            GPU VRAM
           (HEAP_TYPE_UPLOAD)      (HEAP_TYPE_DEFAULT)

m_pTextImage → pUploadBuffer   →   pTexResource
(CPU 배열)    CPU/GPU 모두 접근    (실제 렌더링용)

① CPU가 직접 씀  ② GPU가 복사 커맨드 실행
  (Map/Unmap)      (CopyTextureRegion)
```

`pTexResource`를 CPU도 쓸 수 있게 만들면 GPU 접근 속도가 느려집니다. GPU가 렌더링할 때 텍스처를 **수백만 번 샘플링**하므로 VRAM에 있어야 최대 성능이 납니다.

---

## Q16. DESCRIPTOR_COUNT_FOR_DRAW가 2인 이유는?

스프라이트를 1번 그리는 데 셰이더가 필요로 하는 리소스가 **정확히 2개**이기 때문입니다.

```cpp
static const UINT DESCRIPTOR_COUNT_FOR_DRAW = 2; // | Constant Buffer | Tex |

enum SPRITE_DESCRIPTOR_INDEX {
    SPRITE_DESCRIPTOR_INDEX_CBV = 0,   // 슬롯 0: 위치/스케일/Z 등 상수
    SPRITE_DESCRIPTOR_INDEX_TEX = 1    // 슬롯 1: 화면에 그릴 텍스처
};
```

BasicMeshObject와 비교:

```
BasicMeshObject (6면 박스):
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│  CBV(1)  │ Tex 면0  │ Tex 면1  │ Tex 면2  │ Tex 면3  │ Tex 면4  │ Tex 면5  │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
= 1 + 6 = 7개

SpriteObject (사각형 1장):
┌──────────┬──────────┐
│  CBV(1)  │  Tex(1)  │
└──────────┴──────────┘
= 2개
```

---

## Q17. CDescriptorPool::AllocDescriptorTable 동작 원리

DescriptorPool은 **프레임당 미리 잡아둔 큰 블록에서 조금씩 잘라주는 선형 할당자**입니다.

```
Initialize() 시 1024 × 9 = 9,216개 슬롯을 통째로 예약

┌────┬────┬────┬────┬────┬────┬ ... (9,216개)
│ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │
└────┴────┴────┴────┴────┴────┴
↑
m_AllocatedDescriptorCount = 0 (처음)
```

```cpp
// AllocDescriptorTable(DescriptorCount=2) 호출 시:

// ① 범위 체크
if (m_AllocatedDescriptorCount + DescriptorCount > m_MaxDescriptorCount)
    goto lb_return;  // 꽉 찼으면 실패

// ② 현재 위치에서 CPU/GPU 주소 계산
*pOutCPUDescriptor = 시작주소 + (AllocatedCount × 슬롯크기);
*pOutGPUDescriptor = 시작주소 + (AllocatedCount × 슬롯크기);

// ③ 포인터 전진
m_AllocatedDescriptorCount += DescriptorCount;
```

```
draw 1회 (2개 요청):   슬롯 0,1 반환 → count = 2
draw 2회 (2개 요청):   슬롯 2,3 반환 → count = 4
draw 3회 (9개 요청):   슬롯 4~12 반환 → count = 13
...
```

프레임 끝에 `m_AllocatedDescriptorCount = 0`으로 리셋 → O(1) 초기화 (데이터를 지우지 않고 포인터만 되돌림).

---

## Q18. 무한루프로 Render가 걸릴 때마다 m_AllocatedDescriptorCount가 계속 증가하는가?

**네, 프레임 안에서 draw call이 있을 때마다 누적 증가합니다.**

```
BeginRender()
  → m_AllocatedDescriptorCount = 0  (리셋)

GameObject#1->Render() → AllocDescriptorTable(9)  → count = 9
GameObject#2->Render() → AllocDescriptorTable(9)  → count = 18
...
GameObject#1000->Render() → AllocDescriptorTable(9) → count = 9000
RenderSpriteWithTex()     → AllocDescriptorTable(2) → count = 9002

EndRender()  → GPU가 순서대로 실행
  draw1 → 슬롯 0~8 참조
  draw2 → 슬롯 9~17 참조
  ...
  sprite → 슬롯 9000~9001 참조

다음 프레임 BeginRender()
  → m_AllocatedDescriptorCount = 0  (리셋)
```

draw call마다 슬롯을 새로 잡는 이유: GPU는 커맨드를 **나중에 순서대로 실행**하므로, 같은 슬롯을 덮어쓰면 이전 draw call이 실행될 때 데이터가 오염됩니다. 각 draw call이 독립된 슬롯을 참조해야 안전합니다.

이것이 Ch16의 `MAX_DRAW_COUNT = 256`으로는 1,000개 오브젝트를 그릴 수 없었던 이유이고, Ch17에서 **1024로 확장**한 이유입니다 (1000 × 9 + 2 = 9,002 슬롯 필요).
