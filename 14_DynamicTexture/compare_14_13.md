# Chapter 14 vs Chapter 13 비교 분석

## 주제 요약

| 항목 | Chapter 13 (Sprite) | Chapter 14 (DynamicTexture) |
|------|--------------------|-----------------------------|
| 핵심 주제 | 스프라이트 렌더링 (정적 텍스처) | 동적 텍스처 (매 프레임 CPU에서 텍스처 갱신) |
| 윈도우 제목 | `"Sprite"` | `"DynamicTexture"` |

---

## 1. 개념적 차이

### Chapter 13 (Sprite)
- 텍스처는 파일에서 로드되어 GPU에 한 번 업로드되면 변경되지 않는다 (정적 텍스처).
- `CreateTextureFromFile()` 또는 `CreateTiledTexture()`로 텍스처를 생성한다.

### Chapter 14 (DynamicTexture)
- **동적 텍스처(Dynamic Texture)** 개념이 추가된다: CPU에서 매 프레임 픽셀 데이터를 쓰면, GPU 텍스처가 실시간으로 갱신된다.
- 핵심 메커니즘은 **리소스 쌍(Resource Pair)**:
  - `D3D12_HEAP_TYPE_DEFAULT` 힙의 GPU 텍스처 — 렌더링 시 쉐이더가 읽음
  - `D3D12_HEAP_TYPE_UPLOAD` 힙의 업로드 버퍼 — CPU가 매 프레임 픽셀 데이터를 씀
- 매 프레임 CPU가 업로드 버퍼에 데이터를 쓰고(`Map`/`Unmap`), `bUpdated = TRUE`를 설정한다.
- `RenderSpriteWithTex()` 호출 시 `bUpdated`가 `TRUE`이면 `UpdateTexture()`를 호출하여 업로드 버퍼 → GPU 텍스처로 `CopyTextureRegion` 명령을 기록한다.

---

## 2. 파일별 상세 변경 내용

### 2-1. `typedef.h`

**Ch13**: `LinkedList.h` 미포함. `TEXTURE_HANDLE` 구조체 없음.

**Ch14**: 아래 내용이 추가됨.

```cpp
// 추가된 include
#include "../Util/LinkedList.h"

// 추가된 구조체
struct TEXTURE_HANDLE
{
    ID3D12Resource*             pTexResource;   // GPU 텍스처 리소스
    ID3D12Resource*             pUploadBuffer;  // CPU 쓰기 가능한 업로드 버퍼 (동적 텍스처만 사용)
    D3D12_CPU_DESCRIPTOR_HANDLE srv;            // Shader Resource View
    BOOL                        bUpdated;       // 이번 프레임에 갱신 여부
    SORT_LINK                   Link;           // 연결 리스트 노드
};
```

`pUploadBuffer`는 동적 텍스처(`CreateDynamicTexture`)에서만 할당된다.  
정적 텍스처(`CreateTextureFromFile`, `CreateTiledTexture`)는 `pUploadBuffer = nullptr`.

---

### 2-2. `D3D12Renderer.h`

**Ch14에 추가된 멤버 변수** (텍스처 핸들 연결 리스트 관리):
```cpp
SORT_LINK* m_pTexLinkHead = nullptr;
SORT_LINK* m_pTexLinkTail = nullptr;
```

**Ch14에 추가된 private 메서드** (텍스처 핸들 할당/해제):
```cpp
TEXTURE_HANDLE* AllocTextureHandle();
void            FreeTextureHandle(TEXTURE_HANDLE* pTexHandle);
```

**Ch14에 추가된 public 메서드**:
```cpp
void* CreateDynamicTexture(UINT TexWidth, UINT TexHeight);
void  UpdateTextureWithImage(void* pTexHandle, const BYTE* pSrcBits, UINT SrcWidth, UINT SrcHeight);
```

---

### 2-3. `D3D12Renderer.cpp`

#### 추가된 함수

| 함수 | 설명 |
|------|------|
| `AllocTextureHandle()` | `TEXTURE_HANDLE`을 `new`로 할당하고 연결 리스트(`FIFO`)에 추가 |
| `FreeTextureHandle()` | 연결 리스트에서 제거하고 `delete` |
| `CreateDynamicTexture()` | `CreateTexturePair()`로 GPU 텍스처 + 업로드 버퍼 쌍을 생성하고 SRV 등록 |
| `UpdateTextureWithImage()` | 업로드 버퍼를 `Map`하여 CPU 픽셀 데이터를 행(Row) 단위로 복사(`RowPitch` 고려), 완료 후 `bUpdated = TRUE` |

#### 변경된 함수

**`RenderSpriteWithTex()`**: 동적 텍스처 감지 및 GPU 업로드 처리 추가.
```cpp
// Ch14에서 추가된 로직
if (pTexureHandle->pUploadBuffer)
{
    if (pTexureHandle->bUpdated)
    {
        // 업로드 버퍼 → GPU 텍스처 복사 (CopyTextureRegion 커맨드 기록)
        UpdateTexture(m_pD3DDevice, pCommandList,
                      pTexureHandle->pTexResource,
                      pTexureHandle->pUploadBuffer);
    }
    pTexureHandle->bUpdated = FALSE;
}
```

**`DeleteTexture()`**: `pUploadBuffer`가 있으면 함께 해제 + `FreeTextureHandle()` 호출.
```cpp
// Ch14에서 추가된 해제 로직
if (pUploadBuffer)
    pUploadBuffer->Release();
FreeTextureHandle(pTexHandle);  // 연결 리스트에서도 제거
```

**`CreateTextureFromFile()`**, **`CreateTiledTexture()`**: 기존에는 직접 포인터를 반환했으나, Ch14에서는 `AllocTextureHandle()`을 통해 연결 리스트에 등록하도록 리팩터링됨.

---

### 2-4. `D3D12ResourceManager.h` / `.cpp`

**Ch13**: `CreateTexturePair()` 없음.

**Ch14에 추가된 public 메서드**:
```cpp
BOOL CreateTexturePair(ID3D12Resource** ppOutResource,
                       ID3D12Resource** ppOutUploadBuffer,
                       UINT Width, UINT Height, DXGI_FORMAT format);
```

구현 내용:
1. `D3D12_HEAP_TYPE_DEFAULT` 힙에 GPU 텍스처 생성 (초기 상태: `D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE`)
2. `GetRequiredIntermediateSize()`로 필요한 업로드 버퍼 크기를 계산
3. `D3D12_HEAP_TYPE_UPLOAD` 힙에 업로드 버퍼 생성 (초기 상태: `D3D12_RESOURCE_STATE_GENERIC_READ`)
4. 초기 데이터 복사 없이 빈 쌍을 반환 (초기 데이터는 CPU가 나중에 채움)

> `CreateTexture()`는 초기 이미지 데이터를 받아 즉시 GPU에 업로드한 후 업로드 버퍼를 해제하는 반면,  
> `CreateTexturePair()`는 업로드 버퍼를 계속 유지하여 매 프레임 CPU가 재사용할 수 있게 한다.

---

### 2-5. `main.cpp`

#### 추가된 전역 변수
```cpp
void* g_pDynamicTexHandle = nullptr;  // 동적 텍스처 핸들
BYTE* g_pImage = nullptr;             // CPU 측 픽셀 데이터 버퍼
UINT  g_ImageWidth  = 0;
UINT  g_ImageHeight = 0;
```

#### 초기화 (wWinMain)
```cpp
// 512x256 RGBA 이미지 CPU 메모리 할당, 빨간색으로 초기화
g_ImageWidth = 512;
g_ImageHeight = 256;
g_pImage = (BYTE*)malloc(g_ImageWidth * g_ImageHeight * 4);
// 0xff0000ff = R:255, G:0, B:0, A:255 (RGBA 리틀 엔디언)
for (DWORD y = 0; y < g_ImageHeight; y++)
    for (DWORD x = 0; x < g_ImageWidth; x++)
        pDest[x + g_ImageWidth * y] = 0xff0000ff;

g_pDynamicTexHandle = g_pRenderer->CreateDynamicTexture(g_ImageWidth, g_ImageHeight);
```

#### 정리 (wWinMain)
```cpp
// Ch14에서 추가된 정리
if (g_pDynamicTexHandle) {
    g_pRenderer->DeleteTexture(g_pDynamicTexHandle);
    g_pDynamicTexHandle = nullptr;
}
if (g_pImage) {
    free(g_pImage);
    g_pImage = nullptr;
}
```

#### `Update()` 함수 — 동적 텍스처 갱신 로직 추가

Ch13의 `Update()`는 월드 행렬 갱신과 회전값 증가만 수행한다.

Ch14의 `Update()`에 **16×16 타일 순차 채색 애니메이션**이 추가되었다:

```cpp
// 정적 카운터와 색상값
static DWORD g_dwCount = 0;
static DWORD g_dwTileColorR = 0, g_dwTileColorG = 0, g_dwTileColorB = 0;

const DWORD TILE_WIDTH  = 16;
const DWORD TILE_HEIGHT = 16;
// 전체 타일 수만큼 순환하며 한 타일씩 색상 채움
DWORD TileX = g_dwCount % (g_ImageWidth / TILE_WIDTH);
DWORD TileY = g_dwCount / (g_ImageWidth / TILE_WIDTH);
// ... 해당 타일 픽셀에 RGB 색상 기록 ...
g_dwCount++;
// RGB 색상값을 8씩 증가시켜 전체 색상 공간을 순회

// CPU 픽셀 데이터를 업로드 버퍼에 복사 요청
g_pRenderer->UpdateTextureWithImage(g_pDynamicTexHandle, g_pImage, g_ImageWidth, g_ImageHeight);
```

#### `RunGame()` 함수 — 동적 텍스처 스프라이트 렌더링 추가

```cpp
// Ch14에서 추가된 렌더링 (동적 텍스처를 스프라이트로 렌더링)
g_pRenderer->RenderSpriteWithTex(g_pSpriteObjCommon,
    0, 256 + 5 + 256 + 5,  // 화면 왼쪽, 기존 스프라이트 아래
    0.5f, 0.5f, nullptr, 0.0f,
    g_pDynamicTexHandle);
```

---

## 3. 동적 텍스처 업데이트 흐름 (Ch14)

```
[매 프레임]
  1. Update()
       └─ CPU: g_pImage 픽셀 데이터 수정
       └─ UpdateTextureWithImage()
             ├─ pUploadBuffer->Map()
             ├─ memcpy(CPU 이미지 → 업로드 버퍼, RowPitch 단위)
             ├─ pUploadBuffer->Unmap()
             └─ bUpdated = TRUE

  2. RenderSpriteWithTex() with g_pDynamicTexHandle
       └─ bUpdated == TRUE 이면:
             UpdateTexture(device, commandList, pTexResource, pUploadBuffer)
             ├─ ResourceBarrier: SHADER_RESOURCE → COPY_DEST
             ├─ CopyTextureRegion (업로드 버퍼 → GPU 텍스처)
             └─ ResourceBarrier: COPY_DEST → SHADER_RESOURCE
       └─ bUpdated = FALSE
       └─ Sprite Draw (GPU 텍스처로 렌더링)
```

---

## 4. 변경 파일 목록 요약

| 파일 | 변경 유형 | 주요 내용 |
|------|-----------|-----------|
| `typedef.h` | 수정 | `#include LinkedList.h` 추가, `TEXTURE_HANDLE` 구조체 신규 |
| `D3D12Renderer.h` | 수정 | 텍스처 연결 리스트 멤버, `AllocTextureHandle/Free`, `CreateDynamicTexture`, `UpdateTextureWithImage` 추가 |
| `D3D12Renderer.cpp` | 수정 | `AllocTextureHandle/Free`, `CreateDynamicTexture`, `UpdateTextureWithImage` 구현; `RenderSpriteWithTex`, `DeleteTexture`, `CreateTextureFromFile`, `CreateTiledTexture` 수정 |
| `D3D12ResourceManager.h` | 수정 | `CreateTexturePair()` 선언 추가 |
| `D3D12ResourceManager.cpp` | 수정 | `CreateTexturePair()` 구현 추가 |
| `main.cpp` | 수정 | 동적 텍스처 전역 변수, 초기화/정리, 타일 채색 `Update()`, 동적 텍스처 렌더링 추가 |
