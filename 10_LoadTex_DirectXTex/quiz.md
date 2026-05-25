# Chapter 10 — LoadTex_DirectXTex 객관식 퀴즈

> 아래 퀴즈는 `10_LoadTex_DirectXTex` 프로젝트의 실제 코드를 기반으로 출제되었습니다.  
> 각 문제의 보기 중 **정답 하나**를 고르고, 아래 **▶ 정답 및 해설** 을 클릭하면 확인할 수 있습니다.

---

## Q1. Ch10에서 추가된 파일

Ch10에서 Ch09에 없던 파일로 **새롭게 추가된** 것을 모두 고른 것은?

- A) `DDSTextureLoader12.h`, `DDSTextureLoader12.cpp`, `miku.dds`
- B) `DirectXTex.h`, `DirectXTex.cpp`, `miku.dds`
- C) `DDSTextureLoader12.h`, `DDSTextureLoader12.cpp`
- D) `miku.dds`만 추가됨

<details>
<summary>▶ 정답 및 해설</summary>

**정답: A**

`DDSTextureLoader12.h/.cpp`와 `miku.dds` 3개 모두 추가됨. DirectXTex 라이브러리 자체 파일은 공용 폴더(`../DirectXTex/`)에 있어 프로젝트에 직접 추가되지 않는다.

</details>

---

## Q2. `LoadDDSTextureFromFile` 반환값

`LoadDDSTextureFromFile(m_pD3DDevice, wchFileName, &pTexResource, ddsData, subresouceData)` 호출 직후,  
`pTexResource`가 가리키는 GPU 리소스의 상태는?

- A) `D3D12_RESOURCE_STATE_COPY_DEST` — 복사 대상 상태로 생성됨
- B) `D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE` — 셰이더 읽기 상태로 생성되지만 아직 데이터가 없음
- C) `D3D12_RESOURCE_STATE_COMMON` — 범용 초기 상태
- D) `D3D12_RESOURCE_STATE_GENERIC_READ` — 업로드 힙 상태

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`LoadDDSTextureFromFile`은 DEFAULT heap에 리소스를 `D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE` 상태로 생성하되, 데이터를 채우지 않는다. 이후 코드에서 `COPY_DEST`로 전환 후 업로드한다.

</details>

---

## Q3. `ddsData`의 수명 관리

`CreateTextureFromFile()` 내부에서 `ddsData`를 `std::unique_ptr<uint8_t[]>`로 선언하는 이유는?

- A) GPU 메모리 할당에 `unique_ptr`가 필요하기 때문이다.
- B) `subresouceData` 벡터의 각 원소 안 `pData` 포인터가 `ddsData` 내부를 가리키므로, `ddsData`가 먼저 해제되면 댕글링 포인터가 된다.
- C) DDS 파일 크기가 크기 때문에 힙 할당이 필요하다.
- D) `LoadDDSTextureFromFile` API가 `unique_ptr` 타입만 받기 때문이다.

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`subresouceData`의 `D3D12_SUBRESOURCE_DATA::pData`는 `ddsData` 버퍼 내부를 직접 가리킨다. `ddsData`가 먼저 파괴되면 댕글링 포인터가 된다. `unique_ptr`는 이 수명을 함수 종료까지 보장한다.

</details>

---

## Q4. `subresouceData`의 원소 타입과 의미

`std::vector<D3D12_SUBRESOURCE_DATA> subresouceData`에서 각 원소가 담고 있는 정보는?

- A) GPU 텍스처 리소스의 핸들과 크기
- B) mip 레벨별 CPU 데이터 포인터(`pData`), 행 피치(`RowPitch`), 슬라이스 피치(`SlicePitch`)
- C) mip 레벨별 DXGI 포맷과 해상도
- D) 업로드 버퍼의 GPU 가상 주소

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`D3D12_SUBRESOURCE_DATA`는 `pData`(CPU 포인터), `RowPitch`(행 바이트), `SlicePitch`(전체 슬라이스 바이트)로 구성된다. `subresouceData[i]`가 mip i의 픽셀 데이터 위치와 레이아웃 정보를 담고 있다.

</details>

---

## Q5. `GetRequiredIntermediateSize`의 역할

```cpp
UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTexResource, 0, subresoucesize);
```

이 함수가 반환하는 값은?

- A) 텍스처 원본(mip0)의 바이트 크기만
- B) mip0의 행(row) 하나의 바이트 크기
- C) 전체 subresource(mip 전체)를 GPU alignment에 맞게 패딩한 총 업로드 버퍼 크기
- D) 텍스처 리소스의 GPU 메모리 주소

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

`GetRequiredIntermediateSize()`는 각 mip의 GPU alignment(256바이트 정렬 등)를 포함한 전체 업로드 버퍼 필요 크기를 반환한다. 단순한 픽셀 바이트 합계가 아니라 정렬 패딩이 포함된다.

</details>

---

## Q6. 업로드 버퍼의 힙 타입

`CreateTextureFromFile()` 안에서 업로드 버퍼(`pUploadBuffer`)를 생성할 때 사용하는 힙 타입은?

- A) `D3D12_HEAP_TYPE_DEFAULT`
- B) `D3D12_HEAP_TYPE_READBACK`
- C) `D3D12_HEAP_TYPE_CUSTOM`
- D) `D3D12_HEAP_TYPE_UPLOAD`

<details>
<summary>▶ 정답 및 해설</summary>

**정답: D**

업로드 버퍼는 CPU에서 데이터를 쓰고 GPU가 읽어가는 스테이징 역할이므로 `D3D12_HEAP_TYPE_UPLOAD`를 사용한다. DEFAULT 힙은 CPU가 직접 쓸 수 없다.

</details>

---

## Q7. 업로드 전 Resource Barrier 전환 방향

`UpdateSubresources()` 호출 **직전**에 수행하는 Resource Barrier 전환은?

- A) `COMMON` → `COPY_DEST`
- B) `ALL_SHADER_RESOURCE` → `COPY_DEST`
- C) `COPY_DEST` → `ALL_SHADER_RESOURCE`
- D) `GENERIC_READ` → `COPY_DEST`

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`LoadDDSTextureFromFile`이 리소스를 `ALL_SHADER_RESOURCE` 상태로 생성했으므로, 복사 작업을 시작하려면 반드시 `COPY_DEST`로 전환해야 한다. 상태가 맞지 않으면 D3D12 디버그 레이어가 오류를 보고한다.

</details>

---

## Q8. 업로드 완료 후 Resource Barrier 전환 방향

`UpdateSubresources()` 호출 **직후** 수행하는 Resource Barrier 전환은?

- A) `COPY_DEST` → `ALL_SHADER_RESOURCE`
- B) `ALL_SHADER_RESOURCE` → `COPY_DEST`
- C) `COPY_DEST` → `GENERIC_READ`
- D) `COPY_DEST` → `COMMON`

<details>
<summary>▶ 정답 및 해설</summary>

**정답: A**

복사 완료 후 셰이더에서 읽을 수 있도록 `COPY_DEST` → `ALL_SHADER_RESOURCE`로 복귀한다. 이 상태에서 `Sample()` 호출이 가능해진다.

</details>

---

## Q9. `UpdateSubresources`의 역할

`UpdateSubresources(m_pCommandList, pTexResource, pUploadBuffer, 0, 0, subresoucesize, &subresouceData[0])` 호출이 하는 작업은?

- A) GPU에서 직접 DDS 파일을 읽어 텍스처를 채운다.
- B) `subresouceData`의 각 mip 데이터를 `pUploadBuffer`(UPLOAD)에 memcpy한 뒤, `CopyTextureRegion` 커맨드를 커맨드 리스트에 기록한다.
- C) 텍스처 리소스를 초기화(0으로 클리어)한다.
- D) mip 레벨을 실시간으로 생성(generate)한다.

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`UpdateSubresources()`(d3dx12.h 인라인 함수)는 ① `pUploadBuffer`에 CPU 데이터를 memcpy, ② 각 subresource에 대해 `CopyTextureRegion` 커맨드를 커맨드 리스트에 기록한다. 실제 GPU 실행은 이후 `ExecuteCommandLists()` 호출 시 일어난다.

</details>

---

## Q10. 업로드 완료 후 `pUploadBuffer` 처리

`Fence()` + `WaitForFenceValue()` 이후 `pUploadBuffer`를 `Release()`하는 이유는?

- A) UPLOAD 힙 리소스는 렌더링에 직접 사용할 수 없고, GPU가 DEFAULT 힙으로 복사를 완료했으므로 더 이상 필요 없다.
- B) 메모리 누수를 방지하기 위해 항상 즉시 해제해야 한다.
- C) D3D12 규칙상 커맨드 리스트가 닫히면 관련 리소스를 즉시 해제해야 한다.
- D) `pUploadBuffer`를 해제하지 않으면 GPU가 텍스처 읽기를 거부한다.

<details>
<summary>▶ 정답 및 해설</summary>

**정답: A**

UPLOAD 힙 버퍼는 CPU→GPU 스테이징 역할만 한다. `Fence()+WaitForFenceValue()`로 GPU가 DEFAULT 힙으로의 복사를 완료한 것을 확인한 뒤 해제하므로 안전하다. 이후 텍스처는 `pTexResource`(DEFAULT 힙)에서 직접 읽힌다.

</details>

---

## Q11. `pOutDesc`를 반환하는 이유

`CreateTextureFromFile(ID3D12Resource**, D3D12_RESOURCE_DESC* pOutDesc, ...)`에서 `pOutDesc`를 출력 파라미터로 반환하는 이유는?

- A) 리소스 크기를 호출자가 직접 계산하게 하기 위해서다.
- B) 호출자(`CD3D12Renderer::CreateTextureFromFile`)가 실제 텍스처 포맷과 MipLevels를 알아야 올바른 SRV를 만들 수 있기 때문이다.
- C) `ID3D12Resource`에서 `GetDesc()`를 호출하면 비용이 크기 때문이다.
- D) DDS 파일마다 desc가 다르므로 디버깅용으로 반환하는 것이다.

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

파일마다 포맷(BC1, BC3, BC7, RGBA 등)과 MipLevels가 다르므로, SRV를 올바르게 생성하려면 실제 `D3D12_RESOURCE_DESC`가 필요하다. `SRVDesc.Format = desc.Format`, `SRVDesc.Texture2D.MipLevels = desc.MipLevels`에 그대로 사용된다.

</details>

---

## Q12. `SRVDesc.Format` 설정 — Ch09 vs Ch10 차이

Ch09 `CreateTiledTexture()`와 Ch10 `CreateTextureFromFile()`의 `SRVDesc.Format` 설정 차이는?

- A) 두 함수 모두 `DXGI_FORMAT_R8G8B8A8_UNORM`으로 고정
- B) Ch09는 `DXGI_FORMAT_R8G8B8A8_UNORM` 고정, Ch10은 `desc.Format`으로 동적 결정
- C) Ch09는 `desc.Format`으로 동적 결정, Ch10은 `DXGI_FORMAT_BC1_UNORM`으로 고정
- D) 두 함수 모두 `desc.Format`으로 동적 결정

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

Ch09 `CreateTiledTexture`는 CPU에서 직접 RGBA 픽셀을 만들므로 포맷이 `DXGI_FORMAT_R8G8B8A8_UNORM`으로 고정된다. Ch10 `CreateTextureFromFile`은 파일 포맷이 BC1, BC3, BC7, RGBA 등 다양할 수 있으므로 `desc.Format`을 그대로 사용해야 한다.

</details>

---

## Q13. `SRVDesc.Texture2D.MipLevels = 1`의 효과

SRV를 `SRVDesc.Texture2D.MipLevels = 1`로 생성했을 때, GPU 메모리에 mip이 10개 있어도 어떻게 되는가?

- A) GPU가 자동으로 mip 10개를 모두 사용한다.
- B) GPU 메모리에서 mip1~mip9가 삭제된다.
- C) 셰이더의 `Sample()`은 항상 mip0(원본 해상도)만 읽으며, 나머지 mip은 메모리에 존재하지만 접근 불가하다.
- D) 렌더링 오류가 발생하여 화면이 검게 나온다.

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

SRV의 `MipLevels`는 GPU 메모리가 아닌 **셰이더에서 보이는 뷰 범위**를 결정한다. GPU 메모리에는 10개가 그대로 존재하지만, SRV가 mip0만 노출하므로 `Sample()`은 LOD 계산 결과에 관계없이 항상 mip0만 읽는다. 멀리서 aliasing이 발생할 수 있다.

</details>

---

## Q14. mipmap LOD 자동 선택 원리

`texDiffuse.Sample(samplerDiffuse, input.TexCoord)`에서 GPU가 적절한 mip 레벨을 선택하는 기준은?

- A) 텍스처 좌표(`TexCoord`)의 절대값 크기
- B) 화면 픽셀 당 UV 변화량(`ddx`, `ddy`)을 계산하여 LOD를 결정
- C) 카메라의 월드 위치와 메시의 월드 위치 간 거리를 직접 계산
- D) 셰이더가 `SampleLevel()`을 명시적으로 호출해야 LOD가 선택됨

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

GPU는 인접 픽셀(2×2 쿼드)간 UV 변화량 `ddx(uv)`, `ddy(uv)`를 계산하여 LOD(λ = log₂(ρ × texSize))를 결정한다. 물체가 멀수록 같은 화면 픽셀 이동에 UV 변화량이 커지므로 더 작은(낮은 해상도의) mip이 선택된다.

</details>

---

## Q15. DDS 텍스처 샘플러 필터 설정

`InitRootSinagture()`에서 설정된 정적 샘플러(`D3D12_STATIC_SAMPLER_DESC`)의 `Filter`는?

```cpp
sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
```

이 필터의 의미는?

- A) 확대·축소·mip 선택 모두 **선형 보간(Bilinear/Trilinear)**
- B) 확대·축소·mip 선택 모두 **포인트 샘플링(Nearest Neighbor)**
- C) 확대·축소는 선형, mip은 포인트
- D) 이방성 필터링(Anisotropic)

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`D3D12_FILTER_MIN_MAG_MIP_POINT`는 축소(Min), 확대(Mag), mip 선택(Mip) 세 단계 모두 가장 가까운 텍셀을 선택한다. 계산 비용이 가장 낮지만 픽셀화된 느낌이 날 수 있다. 부드러운 mip 전환을 원하면 `D3D12_FILTER_MIN_MAG_MIP_LINEAR`(Trilinear)를 사용한다.

</details>

---

## Q16. `main.cpp` — DirectXTex.lib 링크 방식

Ch10 `main.cpp`에서 DirectXTex 라이브러리를 링크하는 방식은?

- A) 프로젝트 속성의 링커 설정에서 단일 경로로 추가
- B) `#pragma comment(lib, ...)` 를 플랫폼(x64/ARM64/x86)과 Debug/Release 구성에 따라 조건부로 추가
- C) CMakeLists.txt에서 `target_link_libraries()`로 추가
- D) 런타임에 `LoadLibrary()`로 동적 로드

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`#pragma comment(lib, ...)`를 `#if defined(_M_AMD64)` / `#ifdef _DEBUG` 등의 전처리기 조건으로 감싸서 빌드 구성에 맞는 .lib를 자동으로 링크한다. 소스 코드에 링크 정보를 포함시키는 MSVC 방식이다.

</details>

---

## Q17. `CreateTextureFromFile()` — SRV 생성 실패 시 처리

`m_pSingleDescriptorAllocator->AllocDescriptorHandle(&srv)` 가 실패했을 때 Ch10 코드는 어떻게 처리하는가?

```cpp
if (m_pSingleDescriptorAllocator->AllocDescriptorHandle(&srv))
{
    // 성공 경로 ...
}
else
{
    ???
}
```

- A) `__debugbreak()`로 즉시 중단
- B) `pTexResource->Release()`하고 `nullptr`을 반환
- C) 재시도(retry) 루프를 실행
- D) 로그를 출력하고 기본 텍스처를 반환

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`AllocDescriptorHandle` 실패 시 `pTexResource->Release()`로 이미 생성된 GPU 리소스를 해제하고, `pTexHandle`은 `nullptr`인 채로 반환된다. 호출자는 반환값이 `nullptr`인지 확인해야 한다.

</details>

---

## Q18. Ch10에서 셰이더 변경 여부

Ch09와 Ch10의 `Shaders/shaders.hlsl`은 어떻게 달라졌는가?

- A) mipmap LOD를 지원하는 `SampleLevel()` 호출로 변경됨
- B) 픽셀 셰이더에 LOD bias 인자가 추가됨
- C) 완전히 동일하며 변경 없음
- D) `texDiffuse.SampleBias()`로 교체됨

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

Ch09와 Ch10의 `shaders.hlsl`은 완전히 동일하다. `Sample()`은 SRV에 노출된 mip 범위 내에서 GPU가 자동으로 LOD를 선택하므로 셰이더 수정이 불필요하다. mipmap 지원은 CPU 측(SRV 설정)과 GPU 하드웨어가 처리한다.

</details>

---

## Q19. `TEXTURE_HANDLE` 구조체 — SRV 핸들 타입

Ch10의 `TEXTURE_HANDLE`에서 `srv` 멤버의 타입은?

- A) `D3D12_GPU_DESCRIPTOR_HANDLE`
- B) `D3D12_CPU_DESCRIPTOR_HANDLE`
- C) `ID3D12DescriptorHeap*`
- D) `D3D12_SHADER_RESOURCE_VIEW_DESC`

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`TEXTURE_HANDLE.srv`는 `D3D12_CPU_DESCRIPTOR_HANDLE` 타입이다. CPU-only 힙(`CSingleDescriptorAllocator`)에 저장되어 있으며, 렌더링 시 `CopyDescriptorsSimple()`로 GPU-visible 힙(`CDescriptorPool`)에 복사한 뒤 셰이더에 바인딩된다.

</details>

---

## Q20. `CreateTiledTexture` vs `CreateTextureFromFile` — 업로드 방식 차이

두 함수의 텍스처 데이터 업로드 방식 차이를 가장 잘 설명한 것은?

- A) 둘 다 `UpdateSubresources()` 헬퍼를 사용한다.
- B) `CreateTiledTexture`는 `UpdateTextureForWrite()`(수동 Map/memcpy 기반)를 사용하고, `CreateTextureFromFile`은 `UpdateSubresources()` 헬퍼로 mip 전체를 일괄 업로드한다.
- C) `CreateTiledTexture`는 `UpdateSubresources()`를 사용하고, `CreateTextureFromFile`은 직접 `CopyTextureRegion()`을 호출한다.
- D) 둘 다 `Map()/Unmap()` 방식으로 업로드한다.

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

`CreateTiledTexture`는 CPU에서 직접 UPLOAD 버퍼에 `Map/memcpy/Unmap` 후 `UpdateTextureForWrite()`로 복사한다. `CreateTextureFromFile`은 DDS의 mip 데이터가 `subresouceData` 배열로 준비되어 있으므로 `UpdateSubresources()` 헬퍼가 한 번에 처리한다.

</details>

---

## Q21. `CreateTextureFromFile` — `SRVDesc.Texture2D.MipLevels` 설정

Ch10의 `CD3D12Renderer::CreateTextureFromFile()`에서 SRV의 `MipLevels`를 설정하는 코드는?

- A) `SRVDesc.Texture2D.MipLevels = 1;`
- B) `SRVDesc.Texture2D.MipLevels = -1;`
- C) `SRVDesc.Texture2D.MipLevels = desc.MipLevels;`
- D) `SRVDesc.Texture2D.MipLevels = subresoucesize;`

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

`SRVDesc.Texture2D.MipLevels = desc.MipLevels` — DDS 파일의 실제 mip 개수를 SRV에 그대로 반영하여 셰이더가 전체 mipmap LOD를 사용할 수 있게 한다. 참고로 `-1`(또는 `0xFFFFFFFF`)을 지정하면 `MostDetailedMip`부터 남은 모든 mip을 자동으로 포함시키는 특수 값이다.

</details>

---

## Q22. DDS 파일 포맷의 특징

DDS(DirectDraw Surface) 파일 포맷에 대한 설명으로 옳은 것은?

- A) 항상 `DXGI_FORMAT_R8G8B8A8_UNORM` 포맷으로만 저장된다.
- B) mipmap을 포함할 수 없고 단일 이미지만 저장된다.
- C) BC1~BC7 등 GPU 압축 포맷과 mipmap을 하나의 파일에 저장할 수 있다.
- D) JPEG처럼 손실 압축을 사용한다.

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

DDS 포맷은 BC1(DXT1)~BC7 등 GPU 하드웨어 압축 포맷과 mipmap 체인, 큐브맵, 텍스처 배열까지 하나의 파일에 저장할 수 있는 DirectX 표준 텍스처 포맷이다. BC 압축은 GPU가 직접 압축 데이터를 읽으므로 압축 해제 과정이 없다.

</details>

---

## Q23. `LoadDDSTextureFromFile` — 리소스 생성 위치

`LoadDDSTextureFromFile()`이 내부에서 생성하는 `ID3D12Resource`(`pTexResource`)는 어느 힙에 생성되는가?

- A) `D3D12_HEAP_TYPE_UPLOAD` — CPU 접근 가능 힙
- B) `D3D12_HEAP_TYPE_READBACK` — CPU 읽기 전용 힙
- C) `D3D12_HEAP_TYPE_DEFAULT` — GPU 전용 힙 (데이터는 아직 없음)
- D) 시스템 RAM에 직접 할당

<details>
<summary>▶ 정답 및 해설</summary>

**정답: C**

`LoadDDSTextureFromFile`은 리소스를 `D3D12_HEAP_TYPE_DEFAULT`에 생성한다. 초기에는 데이터가 없으며, 이후 UPLOAD 힙 스테이징 버퍼 → `UpdateSubresources()`를 통해 데이터가 채워진다.

</details>

---

## Q24. `subresoucesize`가 10일 때 `UpdateSubresources` 호출

```cpp
UpdateSubresources(m_pCommandList, pTexResource, pUploadBuffer, 0, 0, subresoucesize, &subresouceData[0]);
//                                                                  ↑  ↑
//                                                           FirstSub  NumSub
```

세 번째 `0`(FirstSubresource)과 `subresoucesize`(NumSubresources)의 의미는?

- A) FirstSubresource=0: mip0부터, NumSubresources=10: 10개의 mip 전체를 업로드
- B) FirstSubresource=0: 첫 번째 텍스처 배열 슬롯, NumSubresources=10: 배열 크기
- C) FirstSubresource=0: 오프셋 0바이트, NumSubresources=10: 총 바이트 수
- D) FirstSubresource=0: 첫 번째 버텍스, NumSubresources=10: 버텍스 수

<details>
<summary>▶ 정답 및 해설</summary>

**정답: A**

`FirstSubresource = 0`은 mip0(가장 큰 해상도)부터 시작, `NumSubresources = 10`은 mip0~mip9까지 10개 전부를 업로드한다는 의미다. 텍스처 배열의 경우 subresource 인덱스 = MipSlice + ArraySlice × MipLevels로 계산된다.

</details>

---

## Q25. Ch10에서 디버그 레이어를 OFF로 바꾼 이유

```cpp
// Ch09: g_pRenderer->Initialize(g_hMainWindow, TRUE, TRUE);
// Ch10: g_pRenderer->Initialize(g_hMainWindow, FALSE, FALSE);
```

Ch10에서 디버그 레이어와 GBV를 끈 직접적인 이유로 가장 적절한 것은?

- A) 디버그 레이어가 DDS 파일 로드를 차단하기 때문이다.
- B) DDS 텍스처 로드 기능 테스트에 집중하기 위한 것으로, 디버그 오버헤드 없이 실행하려는 의도이다.
- C) Ch10부터 디버그 레이어가 지원되지 않는다.
- D) GBV(GPU-Based Validation)가 DirectXTex와 충돌하기 때문이다.

<details>
<summary>▶ 정답 및 해설</summary>

**정답: B**

코드에 이전 설정이 주석으로 남아 있다(`//g_pRenderer->Initialize(g_hMainWindow, TRUE, TRUE);`). 디버그 레이어와 GBV는 DDS 로드와 충돌하지 않으나, 오버헤드 없이 신규 기능 테스트에 집중하기 위해 끈 것이다.

</details>
