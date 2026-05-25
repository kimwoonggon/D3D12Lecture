# Chapter 10 vs Chapter 9 코드 비교 분석

## 핵심 주제

| 챕터 | 주제 |
|------|------|
| **Ch09** `09_DepthBuffer` | 깊이 버퍼(Depth Buffer) 적용 + 프로시저럴(절차적) 타일 텍스처 생성 |
| **Ch10** `10_LoadTex_DirectXTex` | DirectXTex / DDSTextureLoader12 라이브러리를 이용한 **DDS 텍스처 파일 로드** |

Ch09까지 텍스처는 CPU에서 직접 픽셀 데이터를 채워 업로드하는 방식(`CreateTiledTexture`)만 존재했다.  
Ch10의 핵심은 DDS 파일(mipmap, 압축 포맷 포함)을 디스크에서 읽어 GPU 리소스로 올리는 파이프라인을 추가한 것이다.

---

## 1. 새로 추가된 파일

| 파일 | 설명 |
|------|------|
| `DDSTextureLoader12.h` / `DDSTextureLoader12.cpp` | Microsoft 공식 DDS 로더. `LoadDDSTextureFromFile()`을 제공한다. DDS 헤더를 파싱하고 D3D12 리소스를 생성 + subresource 데이터(mip별 포인터·크기)를 반환한다. |
| `miku.dds` | 샘플 DDS 텍스처 파일. BC 계열 압축 포맷 + mipmap이 포함된 경우 그 데이터도 함께 로드된다. |

---

## 2. `main.cpp` 변경점

### 2-1. DirectXTex.lib 링크 지시자 추가 (Ch10 신규)

```cpp
// Ch10 main.cpp 상단에 추가된 플랫폼·빌드 구성별 조건부 링크
#if defined(_M_ARM64EC) || defined(_M_ARM64)
    #ifdef _DEBUG
        #pragma comment(lib, "../DirectXTex/DirectXTex/Bin/Desktop_2022/ARM64/debug/DirectXTex.lib")
    #else
        #pragma comment(lib, "../DirectXTex/DirectXTex/Bin/Desktop_2022/ARM64/release/DirectXTex.lib")
    #endif
#elif defined(_M_AMD64)
    #ifdef _DEBUG
        #pragma comment(lib, "../DirectXTex/DirectXTex/Bin/Desktop_2022/x64/debug/DirectXTex.lib")
    #else
        #pragma comment(lib, "../DirectXTex/DirectXTex/Bin/Desktop_2022/x64/release/DirectXTex.lib")
    #endif
#elif defined(_M_IX86)
    ...
#endif
```

> DirectXTex는 별도 정적 라이브러리(.lib)로 제공되므로 플랫폼(x64/ARM64/x86)과 빌드 구성(Debug/Release)에 맞는 lib을 명시적으로 링크해야 한다.

---

### 2-2. 텍스처 생성 방식 변경

```cpp
// Ch09: CPU에서 절차적으로 생성한 타일 텍스처 2개를 사용
g_pTexHandle0 = g_pRenderer->CreateTiledTexture(16, 16, 192, 128, 255);
g_pTexHandle1 = g_pRenderer->CreateTiledTexture(32, 32, 128, 255, 192);

// Ch10: 두 번째 텍스처를 DDS 파일에서 로드하도록 변경
g_pTexHandle0 = g_pRenderer->CreateTiledTexture(16, 16, 192, 128, 255);  // 변경 없음
g_pTexHandle1 = g_pRenderer->CreateTextureFromFile(L"miku.dds");          // 신규: DDS 파일 로드
```

> `g_pTexHandle0`: 런타임에 생성한 체크보드 패턴 텍스처 (기존 유지)  
> `g_pTexHandle1`: 디스크에서 읽은 DDS 텍스처 (`miku.dds`)로 교체

---

### 2-3. 디버그 레이어 설정 변경

```cpp
// Ch09: 디버그 레이어 ON, GPU-Based Validation ON
g_pRenderer->Initialize(g_hMainWindow, TRUE, TRUE);

// Ch10: 디버그 레이어 OFF (DDS 로드 테스트에 집중)
g_pRenderer->Initialize(g_hMainWindow, FALSE, FALSE);
```

---

### 2-4. 창 타이틀 변경

```cpp
// Ch09
SetWindowText(g_hMainWindow, L"DepthBuffer");

// Ch10
SetWindowText(g_hMainWindow, L"LoadTex_DirectXTex");
```

---

## 3. `D3D12Renderer.h` 변경점

```cpp
// Ch09: 파일 기반 텍스처 로드 함수 없음

// Ch10: CreateTextureFromFile() 추가
void*   CreateTiledTexture(UINT TexWidth, UINT TexHeight, DWORD r, DWORD g, DWORD b);
void*   CreateTextureFromFile(const WCHAR* wchFileName);   // ★ 신규
void    DeleteTexture(void* pTexHandle);
```

> `wchFileName`: 로드할 DDS 파일의 경로 (wide-string).  
> 반환값 `void*`: 내부적으로 `TEXTURE_HANDLE*` 포인터이며, SRV 핸들과 ID3D12Resource 포인터를 감싼 구조체이다.

---

## 4. `D3D12Renderer.cpp` 변경점

### 4-1. `CreateTiledTexture()` 사소한 정리

```cpp
// Ch09: 미사용 지역 변수 존재
TEXTURE_HANDLE* pTexHandle = nullptr;
BOOL bResult = FALSE;        // ← 실제로 사용되지 않는 변수

// Ch10: 불필요한 변수 제거
TEXTURE_HANDLE* pTexHandle = nullptr;
// bResult 변수 제거됨
```

---

### 4-2. `CreateTiledTexture()` - SRV MipLevels 처리

```cpp
// Ch09 / Ch10 동일: 타일 텍스처는 mip이 1개이므로 고정값 사용
SRVDesc.Texture2D.MipLevels = 1;
```

---

### 4-3. `CreateTextureFromFile()` 신규 추가 (Ch10 핵심 추가분)

```cpp
void* CD3D12Renderer::CreateTextureFromFile(const WCHAR* wchFileName)
{
    TEXTURE_HANDLE* pTexHandle = nullptr;

    ID3D12Resource* pTexResource = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE srv = {};

    // ① 로드 후 실제 리소스 디스크립션을 받아온다 (포맷, MipLevels 등이 파일마다 다름)
    DXGI_FORMAT TexFormat = DXGI_FORMAT_R8G8B8A8_UNORM;  // 초기값(실제로 desc.Format이 덮어씀)
    D3D12_RESOURCE_DESC desc = {};

    if (m_pResourceManager->CreateTextureFromFile(&pTexResource, &desc, wchFileName))
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = desc.Format;                      // ② 파일의 실제 포맷(BC1~BC7, RGBA 등) 사용
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = desc.MipLevels;     // ③ 파일에 포함된 mip 수 전체를 SRV에 등록

        if (m_pSingleDescriptorAllocator->AllocDescriptorHandle(&srv))
        {
            m_pD3DDevice->CreateShaderResourceView(pTexResource, &SRVDesc, srv);

            pTexHandle = new TEXTURE_HANDLE;
            pTexHandle->pTexResource = pTexResource;       // GPU 텍스처 리소스
            pTexHandle->srv = srv;                         // 이 텍스처를 가리키는 SRV 핸들
        }
        else
        {
            pTexResource->Release();
            pTexResource = nullptr;
        }
    }
    return pTexHandle;
}
```

**핵심 변수/파라미터 설명:**

| 변수 | 타입 | 설명 |
|------|------|------|
| `wchFileName` | `const WCHAR*` | 로드할 DDS 파일 경로 |
| `pTexResource` | `ID3D12Resource*` | DDSTextureLoader12가 생성한 GPU 텍스처 리소스 (DEFAULT heap) |
| `desc` | `D3D12_RESOURCE_DESC` | 리소스의 실제 속성: `Format`(압축 포맷), `Width`, `Height`, `MipLevels` 포함 |
| `SRVDesc.Format` | `DXGI_FORMAT` | `desc.Format` 그대로 사용 → 파일 포맷이 BC1이면 BC1, RGBA면 RGBA |
| `SRVDesc.Texture2D.MipLevels` | `UINT` | `desc.MipLevels`: DDS 파일에 저장된 mip 단계 수. SRV에 전달해야 GPU가 mipmap LOD를 사용 가능 |
| `pTexHandle` | `TEXTURE_HANDLE*` | GPU 리소스(`pTexResource`)와 SRV 핸들(`srv`)을 묶은 핸들 구조체 |

**Ch09 `CreateTiledTexture()`와의 SRV 생성 차이:**

```
Ch09 (CreateTiledTexture):   SRVDesc.Format = TexFormat;           // DXGI_FORMAT_R8G8B8A8_UNORM 고정
                              SRVDesc.Texture2D.MipLevels = 1;       // mip 1개 고정

Ch10 (CreateTextureFromFile): SRVDesc.Format = desc.Format;          // 파일 포맷 동적 결정
                               SRVDesc.Texture2D.MipLevels = desc.MipLevels; // 파일 mip 수 동적 결정
```

---

## 5. `D3D12ResourceManager.h` 변경점

```cpp
// Ch09: CreateTextureFromFile() 없음

// Ch10: 신규 메서드 선언
BOOL CreateTextureFromFile(
    ID3D12Resource** ppOutResource, // [out] 생성된 GPU 텍스처 리소스
    D3D12_RESOURCE_DESC* pOutDesc,  // [out] 리소스의 포맷·MipLevels 등 속성
    const WCHAR* wchFileName        // [in]  DDS 파일 경로
);
```

---

## 6. `D3D12ResourceManager.cpp` 변경점

### 6-1. 헤더 추가 (Ch10 신규)

```cpp
// Ch09
#include "D3D12ResourceManager.h"

// Ch10: DirectXTex 관련 헤더 3개 추가
#include "../DirectXTex/DirectXTex/dds.h"          // DDS 파일 포맷 정의 (매직 번호, 헤더 구조체 등)
#include "../DirectXTex/DirectXTex/DirectXTex.h"   // DirectXTex 라이브러리 전체 API
#include "DDSTextureLoader12.h"                     // LoadDDSTextureFromFile() 선언

using namespace DirectX;                            // DirectXTex 네임스페이스 사용
```

---

### 6-2. `CreateTextureFromFile()` 구현 (Ch10 핵심 추가분)

```cpp
BOOL CD3D12ResourceManager::CreateTextureFromFile(
    ID3D12Resource** ppOutResource,
    D3D12_RESOURCE_DESC* pOutDesc,
    const WCHAR* wchFileName)
{
    BOOL bResult = FALSE;

    ID3D12Resource* pTexResource = nullptr;
    ID3D12Resource* pUploadBuffer = nullptr;    // 스테이징 버퍼 (UPLOAD heap)

    // ① DDSTextureLoader12를 사용해 DDS 파일 파싱
    //    - pTexResource: DEFAULT heap에 생성된 텍스처 리소스 (아직 데이터 없음)
    //    - ddsData: DDS 파일 전체 raw 바이트 (unique_ptr로 수명 관리)
    //    - subresouceData: mip 레벨별 D3D12_SUBRESOURCE_DATA 배열 (pData 포인터는 ddsData 내부를 가리킴)
    std::unique_ptr<uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresouceData;
    if (FAILED(LoadDDSTextureFromFile(m_pD3DDevice, wchFileName, &pTexResource, ddsData, subresouceData)))
    {
        goto lb_return;
    }

    {
        D3D12_RESOURCE_DESC textureDesc = pTexResource->GetDesc();

        // ② 전체 subresource(mip 전체) 업로드에 필요한 UPLOAD 버퍼 크기 계산
        UINT subresoucesize = (UINT)subresouceData.size();
        UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTexResource, 0, subresoucesize);
        //   GetRequiredIntermediateSize: 각 mip의 aligned 크기를 모두 합산한 값

        // ③ UPLOAD heap에 스테이징 버퍼 생성
        if (FAILED(m_pD3DDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pUploadBuffer))))
        {
            __debugbreak();
        }

        // ④ 커맨드 리스트 초기화
        if (FAILED(m_pCommandAllocator->Reset())) __debugbreak();
        if (FAILED(m_pCommandList->Reset(m_pCommandAllocator, nullptr))) __debugbreak();

        // ⑤ 텍스처를 COPY_DEST 상태로 전환
        m_pCommandList->ResourceBarrier(1,
            &CD3DX12_RESOURCE_BARRIER::Transition(pTexResource,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_COPY_DEST));

        // ⑥ mip 전체를 한 번에 업로드
        //    UpdateSubresources: subresouceData 배열의 각 mip 데이터를
        //    pUploadBuffer(UPLOAD)에 memcpy한 뒤 CopyTextureRegion 커맨드를 기록
        UpdateSubresources(m_pCommandList, pTexResource, pUploadBuffer,
                           0,               // UploadOffset
                           0,               // FirstSubresource 시작 인덱스
                           subresoucesize,  // 업로드할 subresource 수 (mip 개수)
                           &subresouceData[0]);

        // ⑦ 텍스처를 쉐이더 읽기 상태로 복귀
        m_pCommandList->ResourceBarrier(1,
            &CD3DX12_RESOURCE_BARRIER::Transition(pTexResource,
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));

        m_pCommandList->Close();

        ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
        m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // ⑧ GPU 완료 대기
        Fence();
        WaitForFenceValue();

        if (pUploadBuffer)
        {
            pUploadBuffer->Release();  // UPLOAD 버퍼는 GPU 업로드 후 해제
            pUploadBuffer = nullptr;
        }

        *ppOutResource = pTexResource;
        *pOutDesc = textureDesc;       // 포맷·MipLevels 등을 호출자에게 반환
        bResult = TRUE;
    }
lb_return:
    return bResult;
}
```

**핵심 변수/함수 설명:**

| 이름 | 타입 | 설명 |
|------|------|------|
| `LoadDDSTextureFromFile` | 함수 | DDSTextureLoader12의 API. DDS 헤더 파싱 → D3D12 리소스 생성 + subresource 데이터 반환. |
| `ddsData` | `std::unique_ptr<uint8_t[]>` | DDS 파일의 전체 바이너리 데이터. `subresouceData` 내부 포인터들이 이 버퍼를 참조하므로 수명을 함께 유지해야 한다. |
| `subresouceData` | `std::vector<D3D12_SUBRESOURCE_DATA>` | mip 레벨별 업로드 정보 배열. 각 원소는 `pData`(CPU 포인터), `RowPitch`, `SlicePitch`를 포함한다. |
| `subresoucesize` | `UINT` | `subresouceData.size()`. mip 레벨 수 (= mip 0부터 최고 레벨까지의 개수). |
| `uploadBufferSize` | `UINT64` | `GetRequiredIntermediateSize()`가 계산한 전체 업로드 버퍼 크기. 각 mip의 GPU alignment를 포함한다. |
| `pUploadBuffer` | `ID3D12Resource*` | UPLOAD heap 스테이징 버퍼. CPU→GPU 데이터 전달 후 바로 해제. |
| `UpdateSubresources` | 함수 (d3dx12.h 인라인) | `subresouceData`의 각 mip를 `pUploadBuffer`에 복사한 뒤 `CopyTextureRegion` 커맨드를 커맨드 리스트에 기록한다. |
| `*pOutDesc` | `D3D12_RESOURCE_DESC` | 호출자(`CreateTextureFromFile`)가 실제 포맷·MipLevels를 알 수 있도록 전달. |

---

### 6-3. `CreateTiledTexture()` vs `CreateTextureFromFile()` 업로드 방식 비교

```
┌───────────────────────────┬──────────────────────────────────────────────────────┐
│ CreateTiledTexture (Ch09) │ CreateTextureFromFile (Ch10)                         │
├───────────────────────────┼──────────────────────────────────────────────────────┤
│ CPU에서 픽셀 직접 생성     │ DDS 파일에서 읽어옴                                   │
│ mip 없음 (MipLevels = 1)  │ mip 전체 포함 (MipLevels >= 1)                       │
│ 포맷 고정 (RGBA8)          │ 포맷 동적 결정 (BC1, BC3, BC7, RGBA 등)              │
│ 수동 Map/memcpy/Unmap      │ UpdateSubresources() 헬퍼로 mip 전체 일괄 업로드     │
│ UpdateTextureForWrite()   │ 직접 ResourceBarrier + UpdateSubresources + Close    │
└───────────────────────────┴──────────────────────────────────────────────────────┘
```

---

## 7. 셰이더 (`Shaders/shaders.hlsl`)

**변경 없음.** Ch09와 Ch10의 셰이더 코드는 완전히 동일하다.  
`texDiffuse.Sample(samplerDiffuse, input.TexCoord)`는 mip를 지원하므로 `CreateTextureFromFile`로 로드한 mipmap 텍스처도 GPU가 자동으로 LOD를 선택하여 사용한다.

---

## 8. 전체 변경 흐름 요약

```
main.cpp
  └─ g_pRenderer->CreateTextureFromFile(L"miku.dds")    ← Ch10 신규 호출
         │
         ▼
  D3D12Renderer::CreateTextureFromFile()                 ← Ch10 신규 메서드
         │  (m_pResourceManager->CreateTextureFromFile() 호출)
         │  (desc.Format, desc.MipLevels를 받아 SRV 생성)
         ▼
  D3D12ResourceManager::CreateTextureFromFile()          ← Ch10 신규 메서드
         │  LoadDDSTextureFromFile() 호출 (DDSTextureLoader12)
         │  GetRequiredIntermediateSize() → UPLOAD 버퍼 생성
         │  UpdateSubresources() → mip 전체 GPU 업로드
         ▼
  ID3D12Resource (DEFAULT heap, ALL_SHADER_RESOURCE 상태)
         │
         ▼
  TEXTURE_HANDLE { pTexResource, srv }                   ← void*로 반환
```

---

## 9. 학습 포인트

1. **DDS 파일 포맷**: 압축 텍스처(BC1~BC7)와 mipmap을 하나의 파일에 저장하는 표준 포맷. DirectXTex/DDSTextureLoader12로 쉽게 로드 가능.
2. **mipmap 업로드**: `UpdateSubresources()`는 `subresouceData` 배열을 순회하며 mip별로 업로드 버퍼에 복사 + CopyTextureRegion을 커맨드 리스트에 기록한다.
3. **동적 포맷**: 파일마다 포맷이 다를 수 있으므로 SRV 생성 시 `desc.Format`과 `desc.MipLevels`를 사용해야 한다.
4. **DirectXTex 링크**: 플랫폼(x64/ARM64) + 빌드 구성(Debug/Release)에 맞게 조건부 `#pragma comment(lib, ...)` 추가가 필요하다.
