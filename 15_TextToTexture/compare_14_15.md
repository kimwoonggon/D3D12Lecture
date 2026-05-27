# Chapter 14 vs Chapter 15 — 코드 변경 분석

## 핵심 주제

| Chapter 14 | Chapter 15 |
|---|---|
| DynamicTexture (CPU 픽셀 직접 조작) | TextToTexture (DirectWrite 텍스트 → 텍스처) |

---

## 1. 새로 추가된 파일

| 파일 | 내용 |
|---|---|
| `FontManager.h / .cpp` | DirectWrite + Direct2D 기반 텍스트 렌더링 관리자 |

Ch14에 없던 `CFontManager` 클래스가 전부 새로 추가되었다.

---

## 2. 추가된 lib 링크 (main.cpp)

```cpp
// Ch15에서 추가
#pragma comment(lib, "d3d11.lib")   // D3D11On12 사용을 위해 필요
```

Ch14에 없던 `d3d11.lib`가 추가되었다.  
Direct2D가 D3D12 위에서 동작하려면 **D3D11On12** 브리지 레이어가 필요하기 때문이다.

---

## 3. CFontManager — 핵심 신규 클래스

### 3-1. 초기화 구조 (두 단계)

```
Initialize()
  ├─ CreateD2D()    : D3D11On12 → DXGI → D2D Device/DeviceContext 생성
  └─ CreateDWrite() : IDWriteFactory5, D2D Bitmap(렌더 타겟) 생성
```

#### CreateD2D() — D3D11On12 브리지

```cpp
D3D11On12CreateDevice(
    pD3DDevice,       // D3D12 디바이스
    ...
    &pCommandQueue,   // D3D12 커맨드 큐 공유
    ...
    &pD3D11Device,    // 출력: 래핑된 D3D11 디바이스
    ...
);
// D3D11 Device → DXGI Device → D2D Device → D2D DeviceContext
```

**D3D11On12란?**  
D3D12 디바이스 위에 D3D11 레이어를 얹어서 D3D11 API(특히 D2D/DWrite)를 그대로 사용할 수 있게 해주는 브리지다.  
D3D12 커맨드 큐를 공유하기 때문에 D2D 렌더링 결과가 D3D12 파이프라인과 함께 동작한다.

#### CreateDWrite() — 비트맵 2개 생성

```cpp
// 비트맵 1: D2D 렌더 타겟 (GPU에서 텍스트 그리기)
D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW
→ m_pD2DTargetBitmap

// 비트맵 2: CPU에서 읽기 가능한 사본
D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW
→ m_pD2DTargetBitmapRead
```

왜 비트맵이 2개인가?  
D2D 렌더 타겟(`TARGET`)은 CPU에서 직접 읽을 수 없다(`CANNOT_DRAW`만 조합 가능).  
CPU가 픽셀 데이터를 읽으려면 `CPU_READ` 옵션을 가진 별도 비트맵으로 복사한 뒤 `Map()`해야 한다.

---

### 3-2. FONT_HANDLE 구조체 (typedef.h에 추가)

```cpp
struct FONT_HANDLE
{
    IDWriteTextFormat*  pTextFormat;        // DWrite 텍스트 포맷 (폰트, 크기, 정렬 등)
    float               fFontSize;
    WCHAR               wchFontFamilyName[512];
};
```

Ch14의 `TEXTURE_HANDLE`과 같은 역할의 핸들 구조체.  
`IDWriteTextFormat`을 래핑하여 앱 코드가 DWrite API를 직접 다루지 않도록 한다.

---

### 3-3. 텍스트 → CPU 메모리 흐름

```
WriteTextToBitmap()
  └─ CreateBitmapFromText()
       ├─ IDWriteFactory5::CreateTextLayout()   텍스트 레이아웃 계산
       ├─ D2DDeviceContext::BeginDraw()
       ├─ D2DDeviceContext::Clear(Black)        비트맵 클리어
       ├─ D2DDeviceContext::DrawTextLayout()    텍스트 렌더링 (GPU)
       ├─ D2DDeviceContext::EndDraw()
       └─ m_pD2DTargetBitmapRead->CopyFromBitmap()  렌더 타겟 → 읽기 가능 비트맵 복사

  → m_pD2DTargetBitmapRead->Map()              CPU 접근 권한 획득
  → memcpy (행 단위)                           → pDestImage (g_pTextImage)
  → m_pD2DTargetBitmapRead->Unmap()
```

최종적으로 `g_pTextImage`(CPU 메모리)에 텍스트 픽셀이 들어오면,  
Ch14와 동일한 `UpdateTextureWithImage()`로 GPU 텍스처에 업로드한다.

---

## 4. CD3D12Renderer 변경점

### 4-1. 멤버 변수 추가

```cpp
CFontManager*   m_pFontManager = nullptr;   // 신규
float           m_fDPI = 96.0f;             // 신규
```

### 4-2. Initialize()에 추가

```cpp
m_fDPI = GetDpiForWindow(hWnd);  // 윈도우 DPI 조회

m_pFontManager = new CFontManager;
m_pFontManager->Initialize(this, m_pCommandQueue, 1024, 256, bEnableDebugLayer);
```

DPI를 조회하는 이유: DWrite는 물리 픽셀이 아닌 **DIP(Device Independent Pixel)** 단위를 사용한다.  
실제 모니터 DPI에 맞게 폰트 크기를 보정해야 텍스트가 올바른 크기로 렌더링된다.

### 4-3. 퍼블릭 API 추가

```cpp
void*   CreateFontObject(const WCHAR* wchFontFamilyName, float fFontSize);
void    DeleteFontObject(void* pFontHandle);
BOOL    WriteTextToBitmap(BYTE* pDestImage, UINT DestWidth, UINT DestHeight,
                          UINT DestPitch, int* piOutWidth, int* piOutHeight,
                          void* pFontObjHandle, const WCHAR* wchString, DWORD dwLen);
float   INL_GetDPI() const { return m_fDPI; }
```

---

## 5. main.cpp 변경점

### 5-1. 추가된 전역 변수

```cpp
// Ch14에 없던 텍스트용 변수
BYTE*   g_pTextImage = nullptr;         // 텍스트 픽셀 버퍼 (CPU)
UINT    g_TextImageWidth = 0;
UINT    g_TextImageHeight = 0;
void*   g_pTextTexTexHandle = nullptr;  // 텍스트용 DynamicTexture 핸들
void*   g_pFontObj = nullptr;           // FONT_HANDLE
DWORD   g_FPS = 0;                      // FPS 값 (텍스트에 표시)
WCHAR   g_wchText[64] = {};             // 이전 프레임 텍스트 (변경 감지용)
```

### 5-2. 초기화 (wWinMain)

```cpp
// 폰트 오브젝트 생성
g_pFontObj = g_pRenderer->CreateFontObject(L"Tahoma", 18.0f);

// 텍스트용 CPU 버퍼 + DynamicTexture
g_TextImageWidth = 512;
g_TextImageHeight = 256;
g_pTextImage = (BYTE*)malloc(g_TextImageWidth * g_TextImageHeight * 4);
memset(g_pTextImage, 0, g_TextImageWidth * g_TextImageHeight * 4);
g_pTextTexTexHandle = g_pRenderer->CreateDynamicTexture(g_TextImageWidth, g_TextImageHeight);
```

### 5-3. Update() — 텍스트 변경 감지 후 조건부 업데이트

```cpp
WCHAR wchTxt[64] = {};
DWORD dwTxtLen = swprintf_s(wchTxt, L"Current FrameRate: %u", g_FPS);

if (wcscmp(g_wchText, wchTxt))
{
    // 텍스트가 변경된 경우에만 업데이트
    memset(g_pTextImage, 0, g_TextImageWidth * g_TextImageHeight * 4);
    g_pRenderer->WriteTextToBitmap(g_pTextImage, ...);
    g_pRenderer->UpdateTextureWithImage(g_pTextTexTexHandle, g_pTextImage, ...);
    wcscpy_s(g_wchText, wchTxt);
}
```

**포인트:** 매 프레임 텍스트를 다시 그리지 않고, `wcscmp`로 텍스트 변경 여부를 확인하여 변경된 경우에만 `WriteTextToBitmap` + `UpdateTextureWithImage`를 호출한다.  
텍스트 렌더링은 비용이 크기 때문에 이 최적화가 중요하다.

---

## 6. 전체 데이터 흐름 비교

### Ch14 — 픽셀 직접 조작
```
CPU 코드로 픽셀 계산
    → g_pImage (CPU 메모리)
    → UpdateTextureWithImage()
    → pUploadBuffer (Upload Heap)
    → CopyTextureRegion()
    → pTexResource (VRAM)
    → 셰이더 샘플링
```

### Ch15 — 텍스트 렌더링 추가 경로
```
DWrite CreateTextLayout()
    → D2D DrawTextLayout()          ← D3D11On12 브리지 경유
    → m_pD2DTargetBitmap (GPU)
    → CopyFromBitmap()
    → m_pD2DTargetBitmapRead
    → Map() / memcpy (행 단위)
    → g_pTextImage (CPU 메모리)
    → UpdateTextureWithImage()      ← 여기서부터 Ch14와 동일
    → pUploadBuffer (Upload Heap)
    → CopyTextureRegion()
    → pTexResource (VRAM)
    → 셰이더 샘플링
```

---

## 7. 요약

| 항목 | Ch14 | Ch15 |
|---|---|---|
| 신규 파일 | 없음 | `FontManager.h/.cpp` |
| 신규 lib | 없음 | `d3d11.lib` |
| 텍스처 개수 | 2개 | 3개 (Dynamic 2개 + Static 1개) |
| 텍스처 업데이트 조건 | 매 프레임 무조건 | 텍스트 변경 시에만 |
| 핵심 API | D3D12 only | D3D12 + D3D11On12 + D2D + DWrite |
| 신규 구조체 | 없음 | `FONT_HANDLE` |
| DPI 처리 | 없음 | `GetDpiForWindow()` 조회 후 DWrite에 전달 |
