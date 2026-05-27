# Chapter 15 - TextToTexture Q&A

---

## Q. 텍스트 렌더링 전체 파이프라인에서 각 단계별 물리 위치와 내용물은?

```
① DWrite CreateTextLayout()
   위치: CPU
   내용: 레이아웃 메타데이터 (글자 위치, 줄바꿈) — 픽셀 없음

② D2D DrawTextLayout() → m_pD2DTargetBitmap
   위치: GPU (VRAM)
   내용: [검은 배경 + 흰 글씨]  512×256 전체

③ CopyFromBitmap() → m_pD2DTargetBitmapRead
   위치: 시스템 RAM
   내용: [검은 배경 + 흰 글씨]  (텍스트 영역만)

④ Map/memcpy → g_pTextImage
   위치: 시스템 RAM
   내용: [투명(0x00) 배경 + 흰 글씨]  ← memset(0) 후 덮어씀

⑤ UpdateTextureWithImage() → pUploadBuffer
   위치: 시스템 RAM (GPU 주소 공간에 매핑)
   내용: [투명 배경 + 흰 글씨]

⑥ CopyTextureRegion() → pTexResource
   위치: GPU (VRAM)
   내용: [투명 배경 + 흰 글씨] → 셰이더 샘플링
```

전 구간에서 이미지(별도 배경 그림)는 없고, 글씨 픽셀만 이동한다.  
②에서 GPU로 올라갔다가 ③에서 CPU로 내려오고, ⑥에서 다시 VRAM으로 올라가는 **GPU→CPU→GPU 왕복**이 발생한다.

---

## Q. CPU를 경유하지 않고 GPU에서 바로 처리하면 안 되나?

가능하다. D3D11On12의 리소스 직접 공유 방식이 더 효율적이다.

```cpp
// D3D12 텍스처를 D2D가 직접 렌더 타겟으로 쓰는 방식
pD3D11On12Device->AcquireWrappedResources(&pWrappedTex, 1);
    pD2DDeviceContext->SetTarget(pD2DBitmapFromTex);
    pD2DDeviceContext->DrawTextLayout(...);  // GPU에서 바로 씀
pD3D11On12Device->ReleaseWrappedResources(&pWrappedTex, 1);
// CPU 이동 없음
```

Ch15가 CPU를 경유하는 이유는 **Ch14의 UpdateTextureWithImage 구조를 재사용하기 위한 설계 단순화**이다.  
`wcscmp`로 텍스트 변경 시에만 업데이트하므로 왕복 비용이 실제로는 크지 않다.

### 방식 비교

| | CPU 경유 (Ch15) | D3D12 직접 공유 | 폰트 아틀라스 |
|---|---|---|---|
| 복사 횟수 | 4회 | 0회 | 0회 |
| 구현 복잡도 | 낮음 | 중간 | 높음 (초기 구축) |
| 실제 게임 | 교육용 | 동적 텍스트 빈번 시 | Unreal/Unity 표준 |

---

## Q. m_pD2DTargetBitmap과 m_pD2DTargetBitmapRead 각각의 용도는?

```cpp
// 비트맵 1: D2D 렌더 타겟
D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW
→ m_pD2DTargetBitmap
```
**D2D가 글씨를 그리는 캔버스.** GPU VRAM에 있고 CPU는 못 읽음.

```cpp
// 비트맵 2: CPU 읽기용
D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW
→ m_pD2DTargetBitmapRead
```
**CPU가 픽셀을 꺼내가는 창구.** 시스템 RAM에 있고 GPU는 못 씀.

두 옵션을 동시에 설정할 수 없는 이유:
- 렌더 타겟 → GPU 내부 타일 메모리 (스와즐링), CPU 접근 불가
- CPU_READ → 시스템 RAM, 선형 레이아웃, GPU 렌더 불가

`CopyFromBitmap()`이 단순 복사가 아니라 **GPU 타일 레이아웃 → CPU 선형 레이아웃 변환**을 수행한다.

---

## Q. DPI란 무엇인가? 왜 필요한가?

**DPI(Dots Per Inch)** = 1인치 안에 픽셀이 몇 개인지.

```
96 DPI 모니터:  18pt → 24 물리픽셀
192 DPI 모니터: 18pt → 48 물리픽셀
```

DPI를 모르면 고해상도 모니터에서 글씨가 절반 크기로 보인다.

- **Virtual 해상도 (DIP)**: 사람 눈에 보이는 크기 기준, DPI 무관하게 동일
- **Real 해상도 (물리픽셀)**: 실제 GPU가 처리하는 픽셀 수, DPI에 따라 달라짐

```
물리픽셀 = 논리픽셀 × (DPI / 96)
```

코드에서 `GetDpiForWindow(hWnd)`로 실제 모니터 DPI를 조회하고, D2D 비트맵 생성 시 전달한다. DWrite가 `fFontSize=18.0f`(논리 크기)를 받았을 때 DPI 정보로 실제 픽셀 크기를 계산한다.

---

## Q. D3D12를 쓰는데 왜 D3D11 코드가 섞여 있나?

**DirectWrite/Direct2D가 D3D12를 직접 지원하지 않기 때문이다.**

D2D/DWrite는 D3D12 출시 이전에 D3D11 기반으로 설계되었고, D3D12 직접 지원이 추가되지 않았다.  
Microsoft가 이 문제를 위해 **D3D11On12** 브리지를 만들었다.

```
D3D12 디바이스
    ↓  D3D11On12CreateDevice()
D3D11 디바이스 (가짜, D3D12 위에서 동작)
    ↓
D2D → DirectWrite 사용 가능
```

D3D12 커맨드 큐를 D3D11On12에 넘기기 때문에 D2D 결과가 같은 GPU에서 실행된다.  
**D3D11이 실제 렌더링을 하는 게 아니라 D2D/DWrite 연결을 위한 어댑터 역할만 한다.**

---

## Q. Device와 DeviceContext란 무엇인가?

**Device = GPU와 연결하는 계약서**
- "나 이 GPU 쓸게요" 등록
- 텍스처, 버퍼, 셰이더 등 리소스를 생성하는 역할
- 실제로 그림을 그리지는 않음

**DeviceContext = 실제 그리기 명령을 내리는 곳**
- "이 텍스처로 이 메시를 여기에 그려라" 지시
- D3D12에서는 `ID3D12GraphicsCommandList`
- D2D에서는 `ID2D1DeviceContext`

식당 비유:
```
Device       = 주방 (냄비, 칼, 재료 준비)
DeviceContext = 요리사 (실제로 요리 실행)
```

---

## Q. D3D12 Context와 D2D Context가 분리되어 있는데 데이터를 어떻게 교환하나?

**현재 Ch15 방식: CPU 메모리가 중간 전달자**

```
D2D Context
    → D2D 비트맵(GPU) → CopyFromBitmap → Map/memcpy
    → g_pTextImage (CPU)  ← 우체통 역할
                              ↓
                   UpdateTextureWithImage()
                              ↓
D3D12 CommandList → pTexResource(GPU) → 셰이더 샘플링
```

D2D가 CPU에 데이터를 놓고 가면, D3D12가 나중에 그것을 GPU로 올린다.  
두 Context는 **직접 데이터를 주고받지 않고 교대로 CPU 메모리를 통해 전달**한다.

**직접 공유 방식: Acquire/Release로 같은 GPU 텍스처 번갈아 사용**

```cpp
pD3D11On12Device->AcquireWrappedResources();  // D2D에게 권한 넘김
    D2D가 pTex에 글씨 씀
pD3D11On12Device->ReleaseWrappedResources(); // D3D12에게 권한 반환
    D3D12가 pTex 샘플링
```

CPU를 거치지 않지만 누가 쓰고 있는지 명시적으로 권한을 넘겨야 한다.

---

## Q. 두 Context가 동시에 동작하면 문제가 생기나?

생긴다. **리소스 해저드(Resource Hazard)**라고 한다.

```
D2D Context: 텍스처 A에 글씨 쓰는 중...
D3D12 CommandList: 텍스처 A를 셰이더로 읽는 중...
→ 데이터 충돌! GPU 오류
```

Ch15가 CPU 경유 방식을 쓰는 덕분에 이 문제가 자동으로 회피된다.  
D2D가 비트맵 → CPU로 내린 순간 D2D 역할이 끝나고, 그 다음 D3D12가 시작하므로 **항상 교대로** 동작한다.

---

## Q. D3D12 CommandList → [CommandQueue] ← D3D11On12, 실제 코드는 어떻게 생겼나?

### 1단계 — D3D12 커맨드 큐를 FontManager에 전달 (D3D12Renderer.cpp)

```cpp
// D3D12Renderer::Initialize() 안
m_pFontManager->Initialize(
    this,             // D3D12Renderer (D3D12 Device 포함)
    m_pCommandQueue,  // ← D3D12 커맨드 큐를 넘김
    1024, 256,
    bEnableDebugLayer
);
```

### 2단계 — D3D11을 D3D12 위에 얹음 (FontManager.cpp)

```cpp
D3D11On12CreateDevice(
    pD3DDevice,                  // ① D3D12 디바이스 (진짜 GPU)
    d3d11DeviceFlags,
    nullptr, 0,
    (IUnknown**)&pCommandQueue,  // ② D3D12 커맨드 큐 (공유)
    1, 0,
    &pD3D11Device,               // ③ 출력: 가짜 D3D11 디바이스
    &pD3D11DeviceContext,
    nullptr
);
```

`①②`를 넣으면 `③`이 나온다. GPU와 컨베이어 벨트를 그대로 물려받은 D3D11 껍데기.

### 3단계 — D3D11 껍데기로 D2D를 연결 (FontManager.cpp)

```cpp
pD3D11On12Device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice));
pD2DFactory->CreateDevice(pDXGIDevice, &m_pD2DDevice);
m_pD2DDevice->CreateDeviceContext(deviceOptions, &m_pD2DDeviceContext);
// pD3D11Device, pDXGIDevice 등 중간 객체는 모두 Release()
// m_pD2DDevice, m_pD2DDeviceContext만 멤버로 유지
```

### 전체 연결 고리

```
[D3D12 Device]  +  [D3D12 CommandQueue]
        └──── D3D11On12CreateDevice() ────┘
                      ↓
              [D3D11 Device (가짜)]
                      ↓ QueryInterface(DXGI)
              [DXGI Device]
                      ↓ D2D1Factory::CreateDevice()
              [D2D Device]
                      ↓ CreateDeviceContext()
              [D2D DeviceContext] ← DrawTextLayout() 여기서 호출
```

D2D가 `DrawTextLayout()`을 호출하면 그 명령이 결국 **D3D12 CommandQueue**를 통해 GPU에 전달된다.  
GPU 입장에서는 D3D12 명령만 보인다. D3D11 코드는 중간 어댑터 역할만 하고 실물이 없다.

---

## Q. COM이란 무엇인가? QueryInterface, AddRef, Release 사용법은?

**COM(Component Object Model)** = Windows의 인터페이스 표준. 언어/버전이 달라도 같은 방식으로 객체를 쓸 수 있게 한다.

핵심 규칙:
1. 모든 COM 객체는 `IUnknown`을 상속한다
2. 객체는 여러 인터페이스(기능 집합)를 동시에 구현할 수 있다
3. 인터페이스 간 이동은 `QueryInterface()`로 한다
4. 수명 관리는 `AddRef()`/`Release()`로 한다

```cpp
// IUnknown — 모든 COM의 뿌리
struct IUnknown {
    virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG   AddRef()  = 0;
    virtual ULONG   Release() = 0;
};
```

**QueryInterface** — 같은 객체에서 다른 인터페이스 포인터 얻기:
```cpp
ID3D11Device*      pD3D11Device     = ...; // 이미 있음
ID3D11On12Device*  pD3D11On12Device = nullptr;

HRESULT hr = pD3D11Device->QueryInterface(IID_PPV_ARGS(&pD3D11On12Device));
if (FAILED(hr)) { __debugbreak(); }
```

`IID_PPV_ARGS`는 `(__uuidof(T), reinterpret_cast<void**>(pp))`와 동일한 매크로. 타입과 GUID를 자동으로 맞춰준다.

**AddRef / Release** — 참조 카운트 관리:
```cpp
pObj->AddRef();   // 카운트 +1
pObj->Release();  // 카운트 -1, 0이 되면 객체 스스로 소멸
```

QueryInterface를 호출하면 내부적으로 `AddRef()`가 불린다. 쓰고 나면 반드시 `Release()` 해야 한다.

---

## Q. m_pD2DDevice와 m_pD2DDeviceContext 둘 다 필요한 이유는?

```
m_pD2DDevice        = GPU와의 계약 (리소스 생성, DeviceContext의 부모)
m_pD2DDeviceContext = 실제 그리기 명령 실행
```

**m_pD2DDevice** — 공장장. 직접 그리지 않고 DeviceContext를 낳는다:
```cpp
m_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2DDeviceContext);
```

**m_pD2DDeviceContext** — 실제 화가. 모든 그리기 명령이 여기서 나간다:
```cpp
m_pD2DDeviceContext->SetTarget(m_pD2DTargetBitmap);
m_pD2DDeviceContext->BeginDraw();
m_pD2DDeviceContext->Clear(...);
m_pD2DDeviceContext->DrawTextLayout(...);
m_pD2DDeviceContext->EndDraw();
```

**둘 다 멤버 변수로 유지하는 이유**: COM 수명 규칙상 Device가 Release되면 DeviceContext가 무효화될 수 있다.
Device가 살아 있어야 DeviceContext도 유효하므로 Cleanup() 때까지 둘 다 잡아둔다.

| | D3D12 | D2D |
|---|---|---|
| "Device" 역할 | `ID3D12Device` | `ID2D1Device` |
| "그리기 명령" 역할 | `ID3D12GraphicsCommandList` | `ID2D1DeviceContext` |

---

## Q. COM 객체는 임의의 인터페이스로 QueryInterface 가능한가? 순차적으로 가야 하나?

**임의 이동은 불가.** 그 객체가 직접 구현한다고 선언한 인터페이스만 꺼낼 수 있다.  
**순차적으로 가지 않아도 된다.** 같은 객체라면 어느 포인터에서든 바로 꺼낼 수 있다.

`D3D11On12CreateDevice`가 반환한 객체는 C++ 다중 상속으로 여러 인터페이스를 동시에 구현하고 있다:

```
[D3D11On12 내부 객체 하나]
    ├─ ID3D11Device       ← pD3D11Device 가 가리킴
    ├─ ID3D11On12Device2  ← pD3D11On12Device 가 가리킴
    └─ IDXGIDevice        ← pDXGIDevice 가 가리킴
```

따라서 단계를 건너뛰어 바로 가능하다:
```cpp
// 코드처럼 순차적으로 (가독성용)
pD3D11Device->QueryInterface(IID_PPV_ARGS(&pD3D11On12Device));
pD3D11On12Device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice));

// 한 번에도 됨 (같은 결과)
pD3D11Device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice));  // 직접 가능
```

C++ 상속과는 별개다. 인터페이스가 상속 관계가 아니어도 한 객체가 둘 다 구현할 수 있다.

| 질문 | 답 |
|---|---|
| 임의의 COM 객체로 이동 가능? | ❌ 객체가 구현한 인터페이스만 가능 |
| 상속 관계여야 하나? | ❌ COM은 상속 무관, 구현 선언 기준 |
| 순차적으로 가야 하나? | ❌ 같은 객체면 어느 포인터에서든 바로 가능 |

---

## Q. riid란 무엇인가? QueryInterface 구현 안에 인터페이스가 명시되어 있어야 하나?

**riid = GUID (Globally Unique Identifier)** — 128비트 고유 번호. 인터페이스마다 전 세계에서 유일하다:

```cpp
// IID_ID3D11Device 실제 정의 (d3d11.h)
// {DB6F6DDB-AC77-4E88-8253-819DF9BBF140}
DEFINE_GUID(IID_ID3D11Device, 0xdb6f6ddb, 0xac77, 0x4e88, 0x82,0x53,...);

// IID_IDXGIDevice 실제 정의 — 완전히 다른 번호
DEFINE_GUID(IID_IDXGIDevice,  0x54ec77fa, 0x1377, 0x44e6, 0x8c,0x32,...);
```

**YES, QueryInterface 구현 안에 반드시 명시되어 있어야 한다:**

```cpp
// D3D11On12 객체 내부 (Microsoft 구현, 우리는 못 봄)
HRESULT QueryInterface(REFIID riid, void** ppv) {
    if (riid == IID_ID3D11Device)      { *ppv = (ID3D11Device*)this;      return S_OK; }
    if (riid == IID_ID3D11On12Device2) { *ppv = (ID3D11On12Device2*)this; return S_OK; }
    if (riid == IID_IDXGIDevice)       { *ppv = (IDXGIDevice*)this;       return S_OK; }
    *ppv = nullptr;
    return E_NOINTERFACE; // 목록에 없으면 실패
}
```

캐스팅이 안전한 이유: C++ 다중 상속이므로 `this`는 진짜로 그 타입이다. 다중 상속에서는 각 베이스 클래스가 자기 vtable 포인터를 가지므로 `(ID3D11Device*)this`와 `(IDXGIDevice*)this`는 주소가 다를 수 있다. QueryInterface가 올바른 서브오브젝트 주소를 계산해서 반환한다.

| | 누가 작성 |
|---|---|
| QueryInterface 구현 | 객체를 만든 사람 (여기선 Microsoft) |
| QueryInterface 호출 | 사용자 (우리) |

성공 여부는 MSDN 문서에 "이 인터페이스는 QueryInterface로 얻을 수 있다"고 명시된 것만 가능하다.

---

## Q. 텍스트 이미지가 화면에 출력되기까지 거치는 메서드 전체 순서는?

```
[매 프레임] RunGame()  (main.cpp)
│
├─ Update()  (main.cpp)
│   │
│   ├─ swprintf_s(wchTxt, L"Current FrameRate: %u", g_FPS)
│   │   → FPS 문자열 생성
│   │
│   ├─ wcscmp(g_wchText, wchTxt)
│   │   → 이전 텍스트와 비교 — 다를 때만 아래 실행
│   │
│   ├─ memset(g_pTextImage, 0, ...)
│   │   → CPU 버퍼 초기화 (투명화)
│   │
│   ├─ CD3D12Renderer::WriteTextToBitmap()  (D3D12Renderer.cpp)
│   │   └─ CFontManager::WriteTextToBitmap()  (FontManager.cpp)
│   │       │
│   │       ├─ CFontManager::CreateBitmapFromText()  (FontManager.cpp)
│   │       │   ├─ IDWriteFactory5::CreateTextLayout()
│   │       │   │   → 글자 위치/줄바꿈 메타데이터 생성 (픽셀 없음)
│   │       │   ├─ IDWriteTextLayout::GetMetrics()
│   │       │   │   → 텍스트 실제 너비/높이 측정
│   │       │   ├─ ID2D1DeviceContext::SetTarget(m_pD2DTargetBitmap)
│   │       │   │   → GPU VRAM 비트맵을 렌더 타겟으로 설정
│   │       │   ├─ ID2D1DeviceContext::BeginDraw()
│   │       │   ├─ ID2D1DeviceContext::Clear(Black)
│   │       │   │   → 검은 배경으로 초기화
│   │       │   ├─ ID2D1DeviceContext::DrawTextLayout()
│   │       │   │   → 흰 글씨를 GPU VRAM에 그림
│   │       │   ├─ ID2D1DeviceContext::EndDraw()
│   │       │   ├─ IDWriteTextLayout::Release()
│   │       │   └─ ID2D1Bitmap1::CopyFromBitmap(Read ← Target)
│   │       │       → GPU VRAM(타일) → 시스템RAM(선형) 변환 복사
│   │       │
│   │       ├─ ID2D1Bitmap1::Map(CPU_READ)
│   │       │   → m_pD2DTargetBitmapRead를 CPU 주소 공간에 매핑
│   │       ├─ memcpy() × iTextHeight
│   │       │   → 시스템RAM 비트맵 → g_pTextImage (CPU 버퍼)
│   │       └─ ID2D1Bitmap1::Unmap()
│   │
│   ├─ CD3D12Renderer::UpdateTextureWithImage()  (D3D12Renderer.cpp)
│   │   ├─ ID3D12Device::GetCopyableFootprints()
│   │   │   → RowPitch(정렬된 행 크기) 계산
│   │   ├─ ID3D12Resource::Map()
│   │   │   → pUploadBuffer(시스템RAM) CPU 매핑
│   │   ├─ memcpy() × SrcHeight
│   │   │   → g_pTextImage → pUploadBuffer
│   │   ├─ ID3D12Resource::Unmap()
│   │   └─ pTextureHandle->bUpdated = TRUE
│   │
│   └─ wcscpy_s(g_wchText, wchTxt)
│       → 현재 텍스트 저장 (다음 프레임 wcscmp용)
│
└─ CD3D12Renderer::RenderSpriteWithTex(..., g_pTextTexTexHandle)  (D3D12Renderer.cpp)
    ├─ bUpdated == TRUE 확인
    ├─ UpdateTexture()
    │   └─ ID3D12GraphicsCommandList::CopyTextureRegion()
    │       → pUploadBuffer(시스템RAM) → pTexResource(VRAM) GPU 업로드
    ├─ pTexureHandle->bUpdated = FALSE
    └─ CSpriteObject::DrawWithTex(pCommandList, ...)
        → 셰이더가 pTexResource를 샘플링하여 화면에 출력
```

### 물리 위치 흐름 요약

```
CreateTextLayout()      → CPU (메타데이터만, 픽셀 없음)
DrawTextLayout()        → GPU VRAM  (m_pD2DTargetBitmap)
CopyFromBitmap()        → 시스템 RAM (m_pD2DTargetBitmapRead)
Map/memcpy              → 시스템 RAM (g_pTextImage)
UpdateTextureWithImage  → 시스템 RAM (pUploadBuffer)
CopyTextureRegion()     → GPU VRAM  (pTexResource) ← 셰이더 최종 읽기
```

wcscmp로 텍스트가 바뀌지 않으면 `UpdateTextureWithImage` 이하가 통째로 생략된다.
