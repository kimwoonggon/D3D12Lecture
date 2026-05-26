# Chapter 13 — Sprite 퀴즈 20선

> 각 문제의 **정답 보기 ▼** 를 클릭하면 정답이 나옵니다.

---

## Q1. VSMain에서 Y축에 `-2`를 곱하는 이유는?

```hlsl
result.position = float4(Pos.xy * float2(2, -2) + float2(-1, 1), g_Z, 1);
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

NDC(Normalized Device Coordinates)는 Y축이 **위가 +1, 아래가 -1** 이다.  
반면 화면 픽셀 좌표는 **위가 0, 아래로 갈수록 커진다**.  
두 축이 반대이므로 -2를 곱해서 Y 방향을 뒤집는다.  
X는 같은 방향이므로 +2만 곱한다.

- `* float2(2, -2)` → [0,1] 범위를 [-1,1] (X) 또는 [1,-1] (Y) 범위로 확장
- `+ float2(-1, 1)` → 오프셋을 더해 NDC 기준점(좌상단=-1,+1) 맞춤

</details></td></tr></table>

---

## Q2. InitMesh()의 Vertex Y 좌표가 `0.0`이 top이고 `1.0`이 bottom인 이유는?

```cpp
{ { 0.0f, 1.0f, 0.0f }, ... },  // 정점 A
{ { 0.0f, 0.0f, 0.0f }, ... },  // 정점 B (왼쪽 위)
{ { 1.0f, 0.0f, 0.0f }, ... },  // 정점 C (오른쪽 위)
{ { 1.0f, 1.0f, 0.0f }, ... },  // 정점 D
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

Vertex 좌표 자체는 의미 없는 `[0,1]` 단위 사각형이다.  
셰이더에서 `Pos.y * scale + offset` 후 `* -2 + 1` NDC 변환을 거치므로,  
**Vertex Y=0.0이 화면 위(top), Vertex Y=1.0이 화면 아래(bottom)** 이 된다.  
즉, 버텍스 버퍼 좌표는 화면과 같은 방향(위→아래 = 0→1)으로 설계된 것이다.

</details></td></tr></table>

---

## Q3. `m_Scale.x`는 어떻게 계산되는가? 계산식을 쓰시오.

```cpp
// Initialize(pRenderer, wchTexFileName, pRect) 내부
m_Scale.x = ???
m_Scale.y = ???
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

```cpp
m_Scale.x = (float)(m_Rect.right  - m_Rect.left) / (float)TexWidth;
m_Scale.y = (float)(m_Rect.bottom - m_Rect.top)  / (float)TexHeight;
```

RECT가 나타내는 **샘플링 영역의 픽셀 크기**를 **텍스처 전체 크기**로 나눈 값이다.  
예) 512×512 텍스처에서 RECT={0,0,256,256} → `m_Scale = (0.5, 0.5)`

</details></td></tr></table>

---

## Q4. `m_dwInitRefCount`는 왜 `static`으로 선언되어 있는가?

<table><tr><td><details><summary>정답 보기 ▼</summary>

`CSpriteObject`의 공유 리소스(`m_pRootSignature`, `m_pPipelineState`, `m_pVertexBuffer`, `m_pIndexBuffer`)는 **모든 인스턴스가 하나만 생성해서 공유**한다.  
따라서 이것들이 이미 만들어졌는지 추적하는 카운터도 인스턴스가 아니라 **클래스 전체 수준(static)** 에서 관리해야 한다.  
첫 번째 인스턴스가 생성할 때만 리소스를 초기화하고, 이후 인스턴스는 생략한다.

</details></td></tr></table>

---

## Q5. `RenderSprite()` 호출에서 `pScale = (1.0f, 1.0f)`를 넘기면 스프라이트는 화면에 몇 픽셀 크기로 그려지는가?

조건: `sprite_1024x1024.dds`, RECT = `{0, 0, 512, 512}`, 화면 `1280×960`

<table><tr><td><details><summary>정답 보기 ▼</summary>

**512×512 픽셀** 이 아니라 **정확히 RECT 영역의 픽셀 크기인 512×512** 가 맞다.

계산:
```
m_Scale = (512/1024, 512/1024) = (0.5, 0.5)
g_Scale = m_Scale * pScale = (0.5,0.5) * (1.0,1.0) = (0.5, 0.5)

shader: scale = (g_TexSize / g_ScreenRes) * g_Scale
             = (1024/1280, 1024/960) * (0.5, 0.5)
             = (0.4, 0.533)

화면 픽셀: scale.x * 1280 = 512px,  scale.y * 960 = 512px
```

`pScale=1.0`은 **"RECT 크기 그대로 1:1 픽셀"** 을 보장하는 설계다.

</details></td></tr></table>

---

## Q6. `Draw()`와 `DrawWithTex()`의 역할 차이는?

<table><tr><td><details><summary>정답 보기 ▼</summary>

| | Draw() | DrawWithTex() |
|---|---|---|
| 호출자 | `RenderSprite()` | `RenderSpriteWithTex()` 또는 `Draw()` 내부 |
| 텍스처 | Initialize 시 고정된 `m_pTexHandle` 사용 | 인자로 받은 `pTexHandle` 사용 |
| RECT | Initialize 시 고정된 `m_Rect` 사용 | 인자로 받은 `pRect` 사용 |
| m_Scale | `m_Scale * pScale` 합산 후 전달 | 이미 합산된 Scale을 그대로 받음 |

`Draw()`는 인스턴스에 저장된 값과 pScale을 **합산**하는 래퍼이고,  
`DrawWithTex()`는 실제 GPU 커맨드를 기록하는 **구현 본체**다.

</details></td></tr></table>

---

## Q7. `DrawWithTex()`에서 매 draw마다 `DescriptorPool`과 `ConstantBufferPool`에서 새로 할당받는 이유는?

<table><tr><td><details><summary>정답 보기 ▼</summary>

GPU는 커맨드 리스트를 **비동기적으로 실행**한다.  
만약 모든 draw가 같은 descriptor/constant buffer 영역을 덮어쓰면,  
GPU가 이전 draw를 처리하는 도중에 CPU가 다음 draw 데이터를 써서 **데이터 오염(corruption)** 이 발생한다.  

매번 **새 오프셋 영역**을 할당함으로써 각 draw call의 데이터 무결성을 보장한다.  
풀(Pool)은 프레임 종료 후 일괄 리셋된다.

</details></td></tr></table>

---

## Q8. 셰이더에서 UV 좌표를 변환하는 두 공식을 쓰시오.

```hlsl
float2 tex_scale  = ???
float2 tex_offset = ???
result.TexCoord   = input.TexCoord * tex_scale + tex_offset;
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

```hlsl
float2 tex_scale  = g_TexSampleSize / g_TexSize;
float2 tex_offset = g_TexSampePos   / g_TexSize;
```

- `tex_scale` : 아틀라스 전체에서 이 RECT가 차지하는 **비율** (0~1 UV 공간으로 매핑)  
- `tex_offset` : 아틀라스에서 RECT 시작 위치를 **UV 좌표**로 변환  

예) 1024×1024 텍스처, `TexSampePos=(512,0)`, `TexSampleSize=(512,512)`:  
→ `tex_scale=(0.5, 0.5)`, `tex_offset=(0.5, 0.0)` → UV가 `[0.5,1.0]×[0.0,0.5]` 영역 샘플링

</details></td></tr></table>

---

## Q9. 스프라이트의 Z값 `0.0`과 `1.0`의 의미는? 어느 쪽이 더 앞에 그려지는가?

PSO 설정:
```cpp
psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

**Z=0.0이 더 앞에 그려진다.**

`LESS_EQUAL` Depth 판정: 새 픽셀의 Z ≤ 버퍼의 Z 이면 통과(갱신).  
- Z=0.0 → 뎁스 값이 가장 작음 → 가장 먼저/앞에 그려짐  
- Z=1.0 → 뎁스 값이 가장 큼 → 가장 뒤에 그려짐  

main.cpp에서 `g_pSpriteObj3`에 Z=0.0을 준 것은 의도적으로 다른 스프라이트보다 **앞에** 오게 하기 위해서다.

</details></td></tr></table>

---

## Q10. 아래 코드에서 `rootSignatureFlags` 변수는 실제로 적용되는가?

```cpp
D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;  // ← PS 접근 차단?

rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

**적용되지 않는다. 이것은 코드 버그(dead code)다.**

`rootSignatureFlags` 변수는 선언·대입만 되고 `rootSignatureDesc.Init()`의 마지막 인자로 **전달되지 않았다**.  
실제로는 `D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT` 만 적용된다.  
따라서 PSMain에서 `t0`(텍스처)와 `b0`(상수버퍼)에 정상 접근 가능하다.  
`DENY_PIXEL_SHADER_ROOT_ACCESS`가 실제로 적용되면 PSMain이 텍스처/CB에 접근하지 못해 렌더링이 깨진다.

</details></td></tr></table>

---

## Q11. `CreateSpriteObject(L"sprite_1024x1024.dds", 512, 0, 1024, 512)`의 RECT는 텍스처의 어느 영역인가?

<table><tr><td><details><summary>정답 보기 ▼</summary>

**텍스처 우상단 512×512 영역**

```
RECT = { left=512, top=0, right=1024, bottom=512 }
크기 = (1024-512) × (512-0) = 512 × 512 픽셀
```

1024×1024 텍스처에서:
```
┌──────────┬──────────┐
│  [0,0]   │ [512,0]  │ ← 이 영역 (Obj1)
│          │  512×512 │
├──────────┼──────────┤
│          │          │
└──────────┴──────────┘
```

→ `m_Scale = (512/1024, 512/1024) = (0.5, 0.5)`

</details></td></tr></table>

---

## Q12. `g_pSpriteObjCommon`과 `g_pSpriteObj0`의 생성 방식 차이와 각각 언제 쓰는가?

```cpp
g_pSpriteObjCommon = g_pRenderer->CreateSpriteObject();
g_pSpriteObj0 = g_pRenderer->CreateSpriteObject(L"sprite_1024x1024.dds", 0, 0, 512, 512);
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

| | SpriteObjCommon | SpriteObj0~3 |
|---|---|---|
| 생성 | 인자 없음 | 텍스처 파일 + RECT 인자 |
| 텍스처 | 없음 (m_pTexHandle=nullptr) | Initialize 시 로드·고정 |
| RECT | 없음 | Initialize 시 고정 |
| 렌더링 | `RenderSpriteWithTex(obj, x, y, sx, sy, &rect, z, texHandle)` | `RenderSprite(obj, x, y, sx, sy, z)` |
| 용도 | 하나의 오브젝트로 **다른 텍스처·영역을 매번 지정** | 텍스처·영역이 **고정된 스프라이트** |

</details></td></tr></table>

---

## Q13. `InitRootSignature()`에서 Static Sampler의 Filter를 `D3D12_FILTER_MIN_MAG_MIP_POINT`로 설정한 이유는?

<table><tr><td><details><summary>정답 보기 ▼</summary>

`POINT` 필터는 **가장 가까운 텍셀 1개만 샘플링**하는 최근방(nearest-neighbor) 방식이다.  
스프라이트는 픽셀 아트 스타일의 2D 이미지가 많아 **블러(흐림) 없이 선명하게** 표시하는 것이 일반적이다.  
`LINEAR`나 `ANISOTROPIC`을 쓰면 스프라이트가 확대될 때 뭉개진다.  
성능도 POINT가 가장 빠르다.

</details></td></tr></table>

---

## Q14. `InitMesh()`의 인덱스 `{0, 1, 2, 0, 2, 3}`으로 그려지는 두 삼각형을 정점 좌표로 나타내시오.

```cpp
// 정점 순서
A = (0,1)  B = (0,0)  C = (1,0)  D = (1,1)
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

```
삼각형 1 (인덱스 0,1,2): A(0,1) → B(0,0) → C(1,0)  (왼쪽 위 삼각형)
삼각형 2 (인덱스 0,2,3): A(0,1) → C(1,0) → D(1,1)  (오른쪽 아래 삼각형)
```

```
B(0,0)----C(1,0)
|       ↗  |
|    ↗     |
A(0,1)----D(1,1)
```

두 삼각형이 합쳐져 `[0,0]~[1,1]` 범위의 사각형 Quad를 이룬다.

</details></td></tr></table>

---

## Q15. `InitCommonResources()`에서 아래 패턴을 사용하는 이유는?

```cpp
BOOL CSpriteObject::InitCommonResources()
{
    if (m_dwInitRefCount)
        goto lb_true;

    InitRootSinagture();
    InitPipelineState();
    InitMesh();

lb_true:
    m_dwInitRefCount++;
    return m_dwInitRefCount;
}
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

공유 리소스(RootSig, PSO, VB, IB)를 **첫 번째 인스턴스 생성 시 1회만 초기화** 하기 위해서다.  

- `m_dwInitRefCount == 0` (첫 인스턴스): `goto` 건너뛰고 리소스를 생성, 카운트 1  
- `m_dwInitRefCount > 0` (이후 인스턴스): `goto lb_true`로 리소스 생성 건너뜀, 카운트만 증가  

소멸자에서는 카운트를 감소시키고 `0`이 되면 리소스를 해제하는 패턴과 짝을 이룬다.

</details></td></tr></table>

---

## Q16. 스프라이트 PSO에서 `NumRenderTargets = 1`로 설정한 이유는? 3D 메시 오브젝트와 차이가 있는가?

<table><tr><td><details><summary>정답 보기 ▼</summary>

**있다.**

3D 메시는 Chapter 12 이후로 **Deferred Rendering(지연 렌더링)** 을 사용한다.  
Deferred는 G-Buffer 생성을 위해 **MRT(Multiple Render Targets, 4개)** 에 동시에 출력한다 → `NumRenderTargets = 4`

스프라이트는 **Forward Rendering**으로 직접 최종 렌더 타겟에 그린다.  
→ `NumRenderTargets = 1`  

따라서 스프라이트용 PSO와 3D 메시용 PSO는 `RTVFormats` 설정이 서로 다르며, 같은 PSO를 혼용할 수 없다.

</details></td></tr></table>

---

## Q17. 픽셀 좌표 `(522, 0)`을 NDC로 변환하는 전체 과정을 서술하시오.

조건: 화면 `1280×960`, `g_Scale=(0.25, 0.25)`, `g_TexSize=(1024,1024)`

<table><tr><td><details><summary>정답 보기 ▼</summary>

```
① scale  = (g_TexSize / g_ScreenRes) * g_Scale
         = (1024/1280, 1024/960) * (0.25, 0.25)
         = (0.8, 1.0667) * (0.25, 0.25)
         = (0.200, 0.267)

② offset = g_Pos / g_ScreenRes
         = (522/1280, 0/960)
         = (0.408, 0.0)

③ Pos (좌상단 정점 B, xy=(0,0)):
   Pos = (0,0) * scale + offset = (0.408, 0.0)

④ NDC:
   result.position.x = 0.408 * 2 + (-1) = -0.184
   result.position.y = 0.0  * (-2) + 1  = +1.0

→ NDC = (-0.184, +1.0)  → 화면 픽셀 (522, 0) ✓
```

</details></td></tr></table>

---

## Q18. `tex_00.dds`는 `CreateTextureFromFile()`로 별도 로드하는데, `sprite_1024x1024.dds`는 `CreateSpriteObject()` 안에서 로드한다. 이 차이가 생긴 설계 의도는?

<table><tr><td><details><summary>정답 보기 ▼</summary>

| | tex_00.dds | sprite_1024x1024.dds |
|---|---|---|
| 소유자 | 앱(main.cpp) — 별도 핸들 `g_pTexHandle0` | `CSpriteObject` 인스턴스 내부 |
| 공유 방식 | 하나의 텍스처를 **여러 SpriteObj에 재사용 가능** | 특정 스프라이트 오브젝트에 **고정** |
| 해제 | 앱이 명시적으로 `DeleteTexture()` 호출 | SpriteObject 소멸 시 자동 해제 |

`tex_00.dds`를 공용 핸들로 관리하면 `RenderSpriteWithTex()`에서 **draw마다 다른 텍스처를 넘길 수 있다**.  
반면 sprite_1024x1024.dds처럼 내부에 묶으면 오브젝트 하나가 텍스처 하나를 전담한다.

</details></td></tr></table>

---

## Q19. 아래 두 호출에서 최종적으로 화면에 그려지는 `g_Scale`(셰이더 CB 값)을 각각 구하시오.

```cpp
// 호출 A
g_pRenderer->RenderSpriteWithTex(g_pSpriteObjCommon, 256+5, 0, 0.5f, 0.5f, &rect, 0.0f, g_pTexHandle0);
// rect = {256, 0, 512, 256},  tex_00.dds = 512×512

// 호출 B
g_pRenderer->RenderSprite(g_pSpriteObj1, 512+10+10+256, 0, 0.5f, 0.5f, 1.0f);
// sprite_1024x1024.dds, RECT={512,0,1024,512}
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

**호출 A (SpriteObjCommon)**

`DrawWithTex()`를 직접 호출하므로 `m_Scale` 합산 없음.  
→ pScale이 그대로 CB에 들어감  
→ `g_Scale = (0.5, 0.5)`

**호출 B (SpriteObj1)**

`Draw()` → 내부에서 `m_Scale * pScale` 합산  
→ `m_Scale = (512/1024, 512/1024) = (0.5, 0.5)`  
→ `g_Scale = (0.5×0.5, 0.5×0.5) = (0.25, 0.25)`

> 두 호출의 g_Scale이 다르다!  
> A는 tex_00.dds(512×512)에서 256×256 영역을 0.5배 스케일.  
> B는 sprite_1024x1024.dds(1024×1024)에서 512×512 영역을 0.25배 스케일.  
> 결과적으로 화면에 그려지는 픽셀 크기는 **둘 다 128×128px** 이 아닌, 화면해상도에 따른 다른 값을 갖는다.  
> (정확한 픽셀 크기는 셰이더 scale 공식에 g_TexSize도 포함되므로 텍스처 크기가 다른 두 경우는 동일하지 않다.)

</details></td></tr></table>

---

## Q20. 아래 코드에서 `pRect == nullptr`를 넘겼을 때 `DrawWithTex()`는 어떻게 처리하는가?

```cpp
g_pRenderer->RenderSpriteWithTex(g_pSpriteObjCommon, 512+10, 0, 1.0f, 1.0f, nullptr, 1.0f, g_pTexHandle0);
```

<table><tr><td><details><summary>정답 보기 ▼</summary>

`DrawWithTex()` 내부에서 `pRect == nullptr` 를 감지하면 텍스처 전체를 RECT로 사용한다:

```cpp
RECT rect;
if (!pRect)
{
    rect.left   = 0;
    rect.top    = 0;
    rect.right  = TexWidth;   // 텍스처 전체 너비
    rect.bottom = TexHeight;  // 텍스처 전체 높이
    pRect = &rect;
}
```

즉, `tex_00.dds`(512×512) 기준으로 `TexSampePos=(0,0)`, `TexSampleSize=(512,512)`가 CB에 설정된다.  
텍스처 전체가 스프라이트 Quad에 매핑되어 출력된다.

</details></td></tr></table>

---

*퀴즈 끝 — Chapter 13 Sprite*
