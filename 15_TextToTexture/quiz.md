# Chapter 15 — TextToTexture 퀴즈 (객관식)

> 각 문제의 **정답 보기 ▼** 를 클릭하면 정답이 나옵니다.

---

## Q1. `D3D11On12CreateDevice()`에 D3D12 커맨드 큐를 넘기는 이유는?

① D3D11이 독립적인 GPU를 사용하도록 분리하기 위해  
② D2D/DWrite 명령이 결국 같은 D3D12 커맨드 큐를 통해 GPU에 전달되도록 하기 위해  
③ 커맨드 큐를 두 개 만들면 성능이 두 배가 되기 때문에  
④ D3D12 커맨드 큐가 없으면 D3D11On12가 크래시하기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

D3D11On12는 D3D12 위에서 동작하는 래퍼이다. D3D12 디바이스와 커맨드 큐를 함께 넘기면, D2D/DWrite 명령이 내부적으로 D3D12 커맨드로 변환되어 같은 커맨드 큐를 통해 GPU에 전달된다. GPU 입장에서는 D3D12 명령만 보인다.

</details></td></tr></table>

---

## Q2. `m_pD2DTargetBitmap` 생성 시 사용하는 옵션의 조합은?

① `D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_TARGET`  
② `D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW`  
③ `D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW`  
④ `D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CPU_READ`  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`m_pD2DTargetBitmap`은 D2D가 글씨를 그리는 GPU VRAM 캔버스다.  
- `TARGET` : D2D DeviceContext의 렌더 타겟으로 설정 가능  
- `CANNOT_DRAW` : 이 비트맵 자체를 다른 곳에 그리지 못하게 함 (렌더 타겟 전용)  
- `CPU_READ`를 함께 쓸 수 없음 — GPU 타일 메모리는 CPU가 선형 접근 불가

</details></td></tr></table>

---

## Q3. `m_pD2DTargetBitmapRead`에 `CPU_READ`와 `TARGET`을 동시에 쓸 수 없는 이유는?

① CPU_READ 비트맵은 크기 제한이 있기 때문이다  
② TARGET 옵션은 비트맵을 읽기 전용으로 잠그기 때문이다  
③ GPU 렌더 타겟은 GPU 내부 타일(스와즐) 메모리를 사용하고, CPU_READ는 선형 메모리를 사용하기 때문이다  
④ D2D API 버전 차이로 인한 제한이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

GPU 렌더 타겟은 캐시 효율을 위해 타일(swizzled) 레이아웃을 사용하며 CPU가 선형으로 읽을 수 없다.  
CPU_READ 비트맵은 시스템 RAM에 선형으로 저장되며 GPU 렌더 타겟으로 쓸 수 없다.  
따라서 두 비트맵이 분리되어야 하고, `CopyFromBitmap()`이 레이아웃 변환(detiling)을 담당한다.

</details></td></tr></table>

---

## Q4. `CopyFromBitmap(Read ← Target)`이 단순 메모리 복사가 아닌 이유는?

① 비트맵 크기가 달라서 스케일링이 발생하기 때문이다  
② GPU 타일 레이아웃을 CPU 선형 레이아웃으로 변환하기 때문이다  
③ Alpha 채널을 premultiplied로 변환하기 때문이다  
④ D3D12 펜스가 없으면 GPU 대기를 자동으로 삽입하기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`m_pD2DTargetBitmap`(GPU VRAM)의 내부 레이아웃은 타일/스와즐 방식이다.  
`CopyFromBitmap()`은 이를 `m_pD2DTargetBitmapRead`(시스템 RAM)의 선형 레이아웃으로 변환하며 복사한다. 이 과정을 detiling 또는 deswizzling이라고 한다.

</details></td></tr></table>

---

## Q5. `CFontManager::CreateBitmapFromText()` 안에서 `DrawTextLayout()` 이후 `pTextLayout->Release()`를 호출하는 이유는?

① TextLayout을 Release해야 EndDraw()가 실행되기 때문이다  
② TextLayout은 메타데이터 객체이고 GPU 그리기가 끝났으면 더 이상 필요 없기 때문이다  
③ Release하지 않으면 다음 CreateTextLayout()이 실패하기 때문이다  
④ COM 규칙상 함수 내에서 생성한 모든 객체는 즉시 Release해야 하기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`IDWriteTextLayout`은 텍스트의 글자 위치, 줄바꿈 등 레이아웃 메타데이터를 담는 객체로, 픽셀 데이터가 없다. `DrawTextLayout()`으로 GPU에 렌더링이 완료되면 더 이상 필요 없으므로 참조 카운트를 Release한다.

</details></td></tr></table>

---

## Q6. COM에서 `QueryInterface()`가 `E_NOINTERFACE`를 반환하는 경우는?

① 인터페이스 포인터가 이미 초기화되어 있을 때  
② 요청한 인터페이스가 해당 COM 객체의 QueryInterface 구현에 명시되지 않았을 때  
③ AddRef() 없이 QueryInterface를 호출했을 때  
④ 두 개 이상의 인터페이스를 동시에 요청할 때  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

COM 객체의 `QueryInterface` 구현 안에 해당 IID(인터페이스 식별자)가 명시되어 있어야만 성공한다. Microsoft가 구현한 D3D11On12 객체는 `ID3D11Device`, `ID3D11On12Device2`, `IDXGIDevice` 등을 지원하도록 내부에 기술되어 있다. 목록에 없는 인터페이스를 요청하면 `E_NOINTERFACE`가 반환되고 포인터는 `nullptr`이 된다.

</details></td></tr></table>

---

## Q7. `IID_PPV_ARGS(&pInterface)` 매크로가 하는 일은?

① 포인터를 null로 초기화하고 인터페이스 크기를 반환한다  
② `__uuidof(T)`와 `reinterpret_cast<void**>(pp)`를 자동으로 조합하여 타입 안전한 QueryInterface 인자를 만든다  
③ COM 객체를 자동으로 Release하는 스마트 포인터를 생성한다  
④ 인터페이스의 vtable 포인터를 직접 반환한다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`IID_PPV_ARGS(&ptr)`는 `(__uuidof(decltype(*ptr)), reinterpret_cast<void**>(&ptr))`로 전개된다.  
타입에서 GUID를 자동으로 얻고, void** 캐스팅도 처리하므로 수동으로 IID를 적거나 캐스팅할 필요가 없다.

</details></td></tr></table>

---

## Q8. D3D11On12 객체에서 `pD3D11Device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice))`처럼 중간 단계를 건너뛰고 바로 IDXGIDevice를 얻을 수 있는 이유는?

① IDXGIDevice가 ID3D11Device를 상속하기 때문이다  
② D3D11On12CreateDevice가 반환한 객체가 C++ 다중 상속으로 ID3D11Device, IDXGIDevice 등을 동시에 구현하기 때문이다  
③ DXGI 인터페이스는 모든 COM 객체에서 항상 얻을 수 있기 때문이다  
④ QueryInterface는 내부적으로 모든 D3D 인터페이스 트리를 재귀 탐색하기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

D3D11On12CreateDevice가 반환한 객체는 메모리상 하나의 COM 객체이며 여러 인터페이스를 동시에 구현한다:
```
[단일 COM 객체]
  ├─ ID3D11Device
  ├─ ID3D11On12Device2
  └─ IDXGIDevice
```
어느 포인터(pD3D11Device, pD3D11On12Device 등)에서 QueryInterface를 호출해도 결과가 같다. 코드에서 순차적으로 쓰는 것은 가독성 관습이다.

</details></td></tr></table>

---

## Q9. `m_pD2DDevice`와 `m_pD2DDeviceContext`를 둘 다 멤버 변수로 유지하는 이유는?

① DeviceContext는 Device 없이 독립적으로 동작하므로 참조용으로 보관한다  
② DeviceContext를 만들기 위해 Device가 필요하기 때문에 기록용으로 보관한다  
③ COM 수명 규칙상 Device가 Release되면 DeviceContext가 무효화될 수 있기 때문이다  
④ Direct2D 내부적으로 Device와 DeviceContext는 같은 객체이기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

`m_pD2DDevice`가 Release되면 참조 카운트가 감소하여 DeviceContext의 GPU 연결이 끊길 수 있다. Device는 DeviceContext의 부모 객체이며, Device가 살아 있어야 DeviceContext도 유효하다. 따라서 Cleanup() 때까지 둘 다 멤버로 잡아둔다.

</details></td></tr></table>

---

## Q10. D3D12 프로젝트에서 D3D11 코드가 필요한 근본 이유는?

① D3D12는 스프라이트 렌더링을 지원하지 않기 때문이다  
② DirectWrite/Direct2D가 D3D12를 직접 지원하지 않고 D3D11 기반으로만 설계되었기 때문이다  
③ D3D11이 D3D12보다 텍스트 렌더링 성능이 더 빠르기 때문이다  
④ D3D12 디버그 레이어가 D2D를 지원하지 않기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

DirectWrite와 Direct2D는 D3D12 출시 이전에 D3D11 기반으로 설계되었으며, D3D12 직접 지원이 추가되지 않았다. Microsoft가 이 문제를 위해 D3D11On12 브리지를 만들었다. D3D11은 실제 렌더링을 하는 것이 아니라 D2D/DWrite가 사용할 수 있는 어댑터 역할만 한다.

</details></td></tr></table>

---

## Q11. `Update()` 안에서 `wcscmp(g_wchText, wchTxt)` 검사를 하는 이유는?

① FPS가 0일 때 나누기 오류를 방지하기 위해  
② 텍스트가 바뀌지 않으면 GPU 복사를 포함한 전체 텍스트 업데이트 파이프라인을 건너뛰기 위해  
③ swprintf_s가 항상 새 문자열을 생성하기 때문에 비교가 필요하다  
④ wcscpy_s를 호출하기 전에 버퍼 오버플로우를 방지하기 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

FPS가 바뀌지 않는 프레임에서는 `wcscmp`가 0(같음)을 반환하여 `WriteTextToBitmap`, `UpdateTextureWithImage` 등 CPU→GPU 왕복 전체가 생략된다. Ch15의 GPU→CPU→GPU 왕복 비용이 크기 때문에 이 최적화가 중요하다.

</details></td></tr></table>

---

## Q12. `WriteTextToBitmap()` 호출 직전에 `memset(g_pTextImage, 0, ...)`을 하는 이유는?

① g_pTextImage가 초기화되지 않은 쓰레기 값을 가지기 때문이다  
② 이전 텍스트 픽셀이 남아 있어 새 글씨와 합성되므로 투명(0x00)으로 초기화한 뒤 덮어쓰기 위해  
③ WriteTextToBitmap이 항상 전체 버퍼를 쓰지 않으면 크래시하기 때문이다  
④ 검은색 배경으로 초기화해서 글씨가 잘 보이게 하기 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`WriteTextToBitmap`은 텍스트 영역(`iTextWidth × iTextHeight`)만 복사하고 나머지는 건드리지 않는다. 이전 텍스트가 더 길었다면 오른쪽 픽셀이 그대로 남는다. `memset(0)`으로 전체를 투명(alpha=0)으로 초기화한 후 새 텍스트를 덮어써야 깨끗하게 표시된다.

</details></td></tr></table>

---

## Q13. DPI(Dots Per Inch)와 관련하여 `GetDpiForWindow(hWnd)`를 사용하는 이유는?

① 창 크기에 따라 텍스처 해상도를 자동 조정하기 위해  
② 모니터마다 DPI가 달라 논리적 폰트 크기(pt)를 물리 픽셀 수로 올바르게 변환하기 위해  
③ D2D 비트맵 생성 시 필수 매개변수이기 때문이다  
④ DPI가 96이 아니면 D3D11On12가 초기화 실패하기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`물리픽셀 = 논리픽셀 × (DPI / 96)`. 96 DPI 모니터에서 18pt는 24물리픽셀이지만, 192 DPI(HiDPI)에서는 48물리픽셀이 필요하다. DPI를 모르면 고해상도 모니터에서 글씨가 절반 크기로 보인다. `GetDpiForWindow()`로 실제 모니터 DPI를 조회하여 D2D 비트맵 생성 시 전달한다.

</details></td></tr></table>

---

## Q14. `UpdateTextureWithImage()` 안에서 `GetCopyableFootprints()`를 호출하는 목적은?

① 텍스처 전체 크기(바이트)를 구하기 위해  
② GPU가 요구하는 행 정렬(RowPitch)을 포함한 레이아웃 정보를 얻기 위해  
③ 업로드 버퍼가 텍스처를 수용할 수 있는지 검사하기 위해  
④ 텍스처의 밉맵 수를 계산하기 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

GPU는 행(row)을 특정 바이트 단위로 정렬(RowPitch)하여 저장한다. 예를 들어 512×256 RGBA 텍스처는 실제 행 크기가 2048 바이트이지만, GPU의 정렬 요구에 따라 RowPitch는 더 클 수 있다. `GetCopyableFootprints()`로 정확한 RowPitch를 얻어서 `memcpy` 시 올바른 오프셋으로 행을 복사한다.

</details></td></tr></table>

---

## Q15. Ch15에서 D2D 결과를 CPU를 경유해 D3D12로 전달하는 방식의 단점을 보완하는 더 효율적인 방법은?

① D3D12 리소스를 D2D 렌더 타겟으로 직접 연결하는 `AcquireWrappedResources`/`ReleaseWrappedResources` 방식  
② pUploadBuffer를 D2D가 직접 Map()해서 쓰는 방식  
③ D2D 대신 OpenGL을 사용하는 방식  
④ CPU 코어를 늘려서 memcpy 속도를 높이는 방식  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ①**

```cpp
pD3D11On12Device->AcquireWrappedResources(&pWrappedTex, 1);
    pD2DDeviceContext->SetTarget(pD2DBitmapFromTex);  // D3D12 텍스처에 직접 그림
    pD2DDeviceContext->DrawTextLayout(...);
pD3D11On12Device->ReleaseWrappedResources(&pWrappedTex, 1);
```
이 방식은 CPU 복사가 전혀 없고 GPU에서 D3D12 텍스처에 직접 글씨를 쓴다. Ch15가 CPU를 경유하는 이유는 Ch14의 UpdateTextureWithImage 구조 재사용을 위한 설계 단순화다.

</details></td></tr></table>

---

## Q16. 리소스 해저드(Resource Hazard)란 무엇인가?

① GPU 메모리가 부족해 텍스처 생성이 실패하는 현상  
② 두 Context(D2D, D3D12)가 같은 GPU 리소스에 동시 접근하여 데이터 충돌이 발생하는 현상  
③ CPU와 GPU의 속도 차이로 인해 프레임이 떨어지는 현상  
④ D3D12 펜스 값이 넘쳐 오버플로우가 발생하는 현상  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```
D2D Context:      텍스처 A에 글씨 쓰는 중...
D3D12 CommandList: 텍스처 A를 셰이더로 읽는 중...
→ 데이터 충돌! GPU 오류
```
Ch15는 CPU 경유 방식 덕분에 자동으로 회피된다. D2D가 CPU 메모리로 내린 순간 역할이 끝나고 그다음 D3D12가 시작하므로 항상 교대로 동작한다.

</details></td></tr></table>

---

## Q17. `CreateD2D()` 완료 후 `pD3D11Device`, `pDXGIDevice` 등 중간 객체를 모두 `Release()`하는 이유는?

① Release하지 않으면 다음 QueryInterface 호출이 실패하기 때문이다  
② D2D Device와 DeviceContext를 얻은 이후에는 중간 객체가 더 이상 필요 없고, Release하지 않으면 메모리 누수가 발생하기 때문이다  
③ D3D11 객체를 살려두면 D3D12와 충돌이 발생하기 때문이다  
④ COM 규칙상 QueryInterface로 얻은 포인터는 반드시 함수 종료 전에 Release해야 하기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

QueryInterface는 내부적으로 AddRef()를 호출하여 참조 카운트를 증가시킨다. 사용이 끝난 포인터를 Release()하지 않으면 참조 카운트가 영원히 남아 객체가 소멸되지 않는 메모리 누수가 발생한다. `m_pD2DDevice`와 `m_pD2DDeviceContext`는 계속 써야 하므로 Release하지 않는다.

</details></td></tr></table>

---

## Q18. 텍스트 렌더링 파이프라인에서 데이터가 CPU↔GPU를 이동하는 순서로 올바른 것은?

① CPU → GPU → CPU → GPU  
② GPU → CPU → GPU → CPU  
③ CPU → GPU → GPU → CPU  
④ GPU → GPU → CPU → CPU  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ①**

```
① CreateTextLayout()          → CPU (메타데이터)
② DrawTextLayout()            → GPU VRAM (m_pD2DTargetBitmap)
③ CopyFromBitmap()/Map/memcpy → CPU 시스템RAM (g_pTextImage)
④ CopyTextureRegion()         → GPU VRAM (pTexResource)
```
CPU → GPU(D2D 렌더) → CPU(읽기) → GPU(D3D12 텍스처) 순서로 왕복한다.

</details></td></tr></table>

---

## Q19. `CD3D12Renderer::WriteTextToBitmap()`이 직접 처리하지 않고 `m_pFontManager->WriteTextToBitmap()`에 위임하는 이유는?

① FontManager가 없으면 Renderer가 초기화에 실패하기 때문이다  
② D2D/DWrite 관련 기능을 CFontManager에 캡슐화하여 Renderer가 D3D12 책임에만 집중하도록 설계했기 때문이다  
③ Renderer는 가상 함수만 구현할 수 있어 직접 구현이 불가능하기 때문이다  
④ 멀티스레딩을 위해 FontManager가 별도 스레드에서 동작하기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`CFontManager`는 D3D11On12, D2D, DWrite 관련 객체(`m_pD2DDevice`, `m_pD2DDeviceContext`, `m_pD2DTargetBitmap`, `m_pDWFactory` 등)를 모두 보유한다. Renderer는 D3D12 리소스 관리와 커맨드 리스트 제출에 집중하고, 텍스트 렌더링 세부 사항은 FontManager에게 위임하는 책임 분리 설계다.

</details></td></tr></table>

---

## Q20. `RenderSpriteWithTex()`에서 `bUpdated == FALSE`이면 어떻게 되는가?

① 이전 프레임 텍스처를 그대로 화면에 출력한다  
② `CopyTextureRegion()`이 호출되지 않고, 이미 VRAM에 있는 pTexResource를 그대로 셰이더가 샘플링한다  
③ 스프라이트 렌더링 자체가 취소된다  
④ pUploadBuffer의 내용이 자동으로 pTexResource로 복사된다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
if (pTexureHandle->bUpdated) {
    UpdateTexture(...);  // CopyTextureRegion — bUpdated=TRUE일 때만
}
pTexureHandle->bUpdated = FALSE;
pSpriteObj->DrawWithTex(pCommandList, ...);  // 항상 실행
```
`bUpdated = FALSE`이면 GPU 복사 없이 이미 VRAM에 올라가 있는 이전 텍스처 데이터를 셰이더가 그대로 샘플링한다. 텍스트가 바뀌지 않은 프레임에서도 스프라이트는 정상 출력된다.

</details></td></tr></table>
