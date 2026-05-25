# Chapter 10 — 질문 & 답변 모음

---

## Q. `subresoucesize = 10`이면 DDS에서 mip을 10개 가져온 건가?

**맞습니다.**

```cpp
std::vector<D3D12_SUBRESOURCE_DATA> subresouceData;
LoadDDSTextureFromFile(m_pD3DDevice, wchFileName, &pTexResource, ddsData, subresouceData);

UINT subresoucesize = (UINT)subresouceData.size();  // == 10
```

`subresouceData`는 DDS 파일에 저장된 mip 레벨 수만큼 원소를 갖습니다.  
`subresoucesize = 10`이면 mip0(최대 해상도) ~ mip9(1×1) 까지 10단계가 파일에 들어 있다는 뜻이며,  
`UpdateSubresources()`가 10개 전부를 GPU 메모리(DEFAULT heap)에 올립니다.

---

## Q. `SRVDesc.Texture2D.MipLevels = 1`로 세팅하면 10개 중 하나만 쓰는 건가? 10개 다 쓰려면 어떻게 해야 하나?

### 핵심 개념: GPU 리소스와 SRV는 별개 레이어

```
┌──────────────────────────────────────────────────────┐
│  ID3D12Resource (GPU DEFAULT heap 메모리)             │
│  mip0: 512×512  ─┐                                   │
│  mip1: 256×256   │  ← subresoucesize = 10개          │
│  mip2: 128×128   │    전부 GPU 메모리에 존재           │
│  ...             │                                   │
│  mip9:   1×1   ──┘                                   │
└──────────────────────────────────────────────────────┘
           ↑
           │  SRV = 이 리소스를 셰이더에서 어떻게 볼 것인가의 "창문"
           ↓
┌──────────────────────────────────────────────────────┐
│  D3D12_SHADER_RESOURCE_VIEW_DESC                     │
│  .Texture2D.MostDetailedMip = 0  (시작 mip)          │
│  .Texture2D.MipLevels        = ???                   │
│                                                      │
│  = 1  → 셰이더가 mip0만 접근 가능 (LOD 고정)          │
│  = 10 → 셰이더가 mip0~mip9 전체 접근 가능 (LOD 자동)  │
└──────────────────────────────────────────────────────┘
```

### `MipLevels = 1`로 설정한 경우

- GPU 메모리에는 mip 10개가 그대로 존재(사라지지 않음)
- 셰이더의 `Sample()`은 **항상 mip0(원본 해상도)만** 읽음
- 멀리 있는 물체도 가장 큰 해상도 텍스처를 사용 → 계단 현상(aliasing) 발생

### 10개 전부 활용하려면

추가 작업 없이 **`MipLevels` 값만 바꾸면** 됩니다.

```cpp
// MipLevels = 1 : mip0만 노출 (LOD 없음)
SRVDesc.Texture2D.MipLevels = 1;

// MipLevels = desc.MipLevels : 전체 mip 노출 (LOD 자동 선택)
SRVDesc.Texture2D.MipLevels = desc.MipLevels;   // Ch10에서 이미 이렇게 구현됨 ✔
```

셰이더 코드는 변경 불필요합니다.

```hlsl
// shaders.hlsl — 변경 없이 그대로 동작
float4 texColor = texDiffuse.Sample(samplerDiffuse, input.TexCoord);
//                           ↑ GPU가 화면 픽셀당 uv 변화량(ddx/ddy)을 계산해
//                             가장 적합한 mip 레벨을 자동으로 선택함
```

### LOD 선택 원리

```
카메라에서 가까움          카메라에서 멀리 있음
       ↓                           ↓
uv 변화량(ddx/ddy) 작음    uv 변화량 큼
       ↓                           ↓
mip0 (512×512) 사용        mip4~mip9 (작은 해상도) 사용
→ 선명하게 표시             → 흐릿하게 표시, aliasing 없음
```

### Ch10 코드 정리

| 함수 | `MipLevels` 설정 | 이유 |
|------|------------------|------|
| `CreateTiledTexture()` | `= 1` (고정) | 리소스 자체를 mip 1개로 만들었으므로 올바름 |
| `CreateTextureFromFile()` | `= desc.MipLevels` (동적) | DDS에서 전체 mip을 불러왔으므로 전체를 노출해야 올바름 |

---

## Q. `UpdateTextureForWrite`와 `subresoucesize`는 관련 있나?

**관련 없습니다.** 두 경로는 완전히 분리되어 있습니다.

| | 경로 | mip 업로드 방식 |
|---|------|----------------|
| `CreateTiledTexture()` | Ch09 방식 유지 | 수동 `Map/memcpy/Unmap` → `UpdateTextureForWrite()` 호출 |
| `CreateTextureFromFile()` | Ch10 신규 | `UpdateSubresources()` 헬퍼로 mip 전체 일괄 업로드, `UpdateTextureForWrite()` 미사용 |

`subresoucesize` 변수는 `CreateTextureFromFile()` 안에만 존재합니다.  
`UpdateTextureForWrite()`는 내부에서 `Desc.MipLevels`로 루프를 돌지만, DDS 로드 경로에서는 호출되지 않습니다.

---

## Q. `ddx(uv)` / `ddy(uv)`에서 왜 u, v가 각각 x, y만의 함수가 아니라 x와 y 모두의 함수인가?

`u = f(x)`, `v = g(y)` 처럼 분리될 수 있다면 편리하겠지만, 3D 그래픽스에서는 그 조건이 일반적으로 성립하지 않습니다.

### 성립 조건

`u = f(x)`, `v = g(y)` 만 성립하려면:
- 텍스처 맵핑의 가로 방향이 화면 x축과 **완벽하게 평행**
- 텍스처 맵핑의 세로 방향이 화면 y축과 **완벽하게 평행**

이 조건은 텍스처가 화면에 **정면으로 정렬된 매우 특수한 경우**에만 해당합니다.

### 반례 1: 회전된 쿼드

```
화면 공간에서 45도 회전된 쿼드

화면(screen)     UV
 (0,1) ──→ UV(0,0)
  ↑  \              ← 대각선 방향으로 맵핑됨
 (1,0) ──→ UV(1,0)

→ 화면에서 오른쪽(x+1)으로 이동하면 텍스처에서 대각선(u+, v+)으로 이동
→ x만 바뀌어도 v가 바뀜  ∴ v는 x의 함수이기도 함
```

### 반례 2: 비스듬히 바라보는 벽 (Perspective)

```
카메라 ───────────────────→ 화면
        /
       /  벽 (텍스처 붙어 있음)
      /
```

Perspective projection 이후, 화면에서 **x 방향으로 1픽셀** 이동하면 텍스처에서 **u도 바뀌고 v도 바뀝니다.** 벽이 기울어져 있기 때문입니다.

### 수학적 표현 — 야코비안(Jacobian)

화면 좌표 → 텍스처 좌표 변환의 야코비안은 일반적으로 2×2 행렬:

```
J = | ∂u/∂x   ∂u/∂y |
    | ∂v/∂x   ∂v/∂y |
```

분리 가능한 경우(비대각 항 = 0)는 아래처럼 특수한 경우입니다:

```
J_simple = | ∂u/∂x    0    |    ← 이 경우에만 u = f(x)
           |   0    ∂v/∂y |    ← 이 경우에만 v = g(y)
```

### LOD 계산식에 4개의 편미분이 모두 포함되는 이유

$$\rho = \max\left(\sqrt{\left(\frac{\partial u}{\partial x}\right)^2 + \left(\frac{\partial v}{\partial x}\right)^2},\ \sqrt{\left(\frac{\partial u}{\partial y}\right)^2 + \left(\frac{\partial v}{\partial y}\right)^2}\right)$$

- 첫 번째 항: 화면 x 방향 1픽셀 이동 시 **텍스처 공간에서의 이동 거리** (u, v 모두 포함)
- 두 번째 항: 화면 y 방향 1픽셀 이동 시 **텍스처 공간에서의 이동 거리** (u, v 모두 포함)

`∂u/∂y = 0`, `∂v/∂x = 0`이 보장되지 않으므로 4개의 편미분을 모두 사용해야 합니다.

### HLSL에서 `ddx`/`ddy`가 하는 일

```hlsl
float4 texColor = texDiffuse.Sample(samplerDiffuse, input.TexCoord);
// GPU 내부적으로:
//   float2 dx = ddx(input.TexCoord);  // 오른쪽 인접 픽셀과의 uv 차이 (∂u/∂x, ∂v/∂x)
//   float2 dy = ddy(input.TexCoord);  // 아래쪽 인접 픽셀과의 uv 차이 (∂u/∂y, ∂v/∂y)
//   LOD = log2(max(length(dx), length(dy)) * textureSize)
```

`ddx(uv)`는 2D 벡터 `(∂u/∂x, ∂v/∂x)`를 반환하고, `ddy(uv)`는 `(∂u/∂y, ∂v/∂y)`를 반환합니다. 이 두 벡터 각각의 크기(length)로 mip LOD를 결정합니다.

---

## Q. `subresouceData[i].pData`가 `ddsData` 내부를 가리킨다는 게 무슨 뜻인가? `ddsData`가 먼저 사라지면 왜 문제가 되는가?

`subresouceData`는 픽셀 데이터를 **복사해서 보유하는 것이 아니라 주소(포인터)만** 가집니다.

### 메모리 구조

```
ddsData (unique_ptr<uint8_t[]>) → 힙 메모리
┌─────────────────────────────────────────────────┐
│  [DDS 헤더][mip0 픽셀...][mip1 픽셀...][mip2...] │
│  ^          ^0x1080       ^0x5080       ^0x6080  │
└─────────────────────────────────────────────────┘

subresouceData[0].pData ──→ 0x1080  (ddsData 내부 주소)
subresouceData[1].pData ──→ 0x5080  (ddsData 내부 주소)
subresouceData[2].pData ──→ 0x6080  (ddsData 내부 주소)
```

### ddsData가 먼저 소멸되면

```cpp
// 잘못된 예시
{
    auto ddsData = std::make_unique<uint8_t[]>(...);
    LoadDDSTextureFromFile(..., ddsData, subresouceData);
} // ← ddsData 소멸, 힙 메모리 해제

UpdateSubresources(..., &subresouceData[0]);
// subresouceData[0].pData = 0x1080 → 이미 해제된 메모리 → 댕글링 포인터 💥
```

### 실제 코드에서 안전한 이유

```cpp
HRESULT CreateTextureFromFile(...) {
    std::unique_ptr<uint8_t[]> ddsData;          // ─┐ 같은 스코프
    std::vector<D3D12_SUBRESOURCE_DATA> subData; //  │
    LoadDDSTextureFromFile(..., ddsData, subData);//  │
    UpdateSubresources(..., &subData[0]);         //  │ ← ddsData 살아있음
    Fence(); WaitForFenceValue();                 //  │
}                                                // ─┘ 동시에 소멸 → 안전
```

`unique_ptr`가 특별한 게 아니라, **같은 함수 스코프** 안에 두 변수를 두어 수명을 일치시키는 것입니다.

---

## Q. UPLOAD 힙으로 선언했는데 DEFAULT 힙이라니? 버퍼가 하나 아닌가?

버퍼가 **두 개** 존재합니다. 이름이 비슷해서 혼동하기 쉽습니다.

```
버퍼 1: pTexResource  (DEFAULT 힙)
  → LoadDDSTextureFromFile이 생성
  → GPU 전용 (CPU 직접 쓰기 불가)
  → 텍스처의 "최종 거주지" — 렌더링에 사용

버퍼 2: pUploadBuffer  (UPLOAD 힙)
  → 코드에서 직접 CreateCommittedResource로 생성
  → CPU 쓰기 가능, GPU 읽기 가능
  → 버퍼 1로 데이터를 전달하기 위한 "임시 택배 박스"
```

### 데이터 이동 경로

```
ddsData(CPU RAM) → pUploadBuffer(UPLOAD 힙) → pTexResource(DEFAULT 힙)
                   ① CPU가 memcpy          ② GPU가 CopyTextureRegion
                   (즉시 실행)               (ExecuteCommandLists 이후)
```

### DEFAULT 힙에 CPU가 직접 못 쓰는 이유

| 힙 타입 | CPU 쓰기 | GPU 읽기 | 용도 |
|---------|---------|---------|------|
| UPLOAD | ✅ 가능 | ✅ 가능 | 데이터 전달용 임시 버퍼 |
| DEFAULT | ❌ 불가 | ✅ 빠름 | 실제 GPU 리소스 (VRAM) |

DEFAULT 힙은 VRAM에 직접 올라가므로 CPU가 접근할 수 없습니다. 반드시 UPLOAD 힙을 중간 다리로 사용해야 합니다.

### Fence 이후 pUploadBuffer를 해제하는 이유

`Fence() + WaitForFenceValue()`는 GPU가 ② 단계(UPLOAD→DEFAULT 복사)를 완료했음을 CPU가 확인하는 동기화입니다. 이 확인 없이 해제하면 GPU가 아직 읽는 도중에 메모리가 사라집니다.

---

## Q. DDS 파일이 DEFAULT 힙을 거쳐 최종 DEFAULT 힙으로 복사되는 전체 과정을 코드 기준으로

### 단계별 흐름

```
DDS 파일(디스크)
  ↓ ① LoadDDSTextureFromFile
ddsData(CPU RAM) + pTexResource(DEFAULT 힙, 빈 껍데기)
  ↓ ② CreateCommittedResource(UPLOAD)
pUploadBuffer(UPLOAD 힙, 빈 껍데기)
  ↓ ③ UpdateSubresources 내부 memcpy (CPU가 즉시 실행)
ddsData → pUploadBuffer
  ↓ ④ ExecuteCommandLists (GPU가 비동기 실행)
pUploadBuffer → pTexResource
  ↓ ⑤ Fence + WaitForFenceValue (CPU가 GPU 완료 대기)
pUploadBuffer->Release() + ddsData 소멸 (함수 종료)
pTexResource만 남아 렌더링에 사용
```

### 핵심 코드 위치 (D3D12ResourceManager.cpp:306~)

```cpp
// ① DDS 파일 읽기 + DEFAULT 힙 리소스 생성
LoadDDSTextureFromFile(m_pD3DDevice, wchFileName, &pTexResource, ddsData, subresouceData);

// ② UPLOAD 힙 스테이징 버퍼 생성
UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTexResource, 0, subresoucesize);
m_pD3DDevice->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), ...
    IID_PPV_ARGS(&pUploadBuffer));

// ③+④ Barrier + memcpy(CPU) + CopyTextureRegion 커맨드 기록(GPU 예약)
m_pCommandList->ResourceBarrier(... ALL_SHADER_RESOURCE → COPY_DEST);
UpdateSubresources(m_pCommandList, pTexResource, pUploadBuffer, 0, 0, subresoucesize, &subresouceData[0]);
m_pCommandList->ResourceBarrier(... COPY_DEST → ALL_SHADER_RESOURCE);

// ④ GPU 실제 실행
m_pCommandQueue->ExecuteCommandLists(...);

// ⑤ CPU가 GPU 완료 대기
Fence(); WaitForFenceValue();

// ⑥ 스테이징 버퍼 해제
pUploadBuffer->Release();
// 함수 반환 시 ddsData(unique_ptr) 자동 소멸
```

`UpdateSubresources()` 내부에서 일어나는 두 가지 작업:
- **즉시(CPU)**: `ddsData` → `pUploadBuffer` memcpy
- **기록만(GPU 예약)**: `CopyTextureRegion` 커맨드를 커맨드 리스트에 추가 (실제 실행은 `ExecuteCommandLists` 이후)

---

## Q. Ch09와 Ch10의 shaders.hlsl이 동일한 이유 — `Sample()`이 mip을 자동 선택하는 원리

```hlsl
// Ch09, Ch10 완전히 동일
float4 texColor = texDiffuse.Sample(samplerDiffuse, input.TexCoord);
```

### 역할 분리

```
CPU 측 (D3D12ResourceManager.cpp)
  SRVDesc.Texture2D.MipLevels = desc.MipLevels;  // "mip0~mip9까지 노출"
  ↓ SRV가 셰이더에서 볼 수 있는 mip 범위를 결정

GPU 하드웨어 (Fixed Function TMU)
  Sample() 호출 시 2×2 quad의 uv 차이로 LOD 자동 계산
  SRV가 허용한 mip 범위 안에서 최적 mip 선택

셰이더 코드 (shaders.hlsl)
  "이 UV로 샘플링해줘" 만 요청, 어떤 mip인지 신경 안 씀
```

| | Ch09 | Ch10 |
|--|------|------|
| GPU 메모리 mip 수 | 1개 | 10개 |
| SRVDesc.MipLevels | `= 1` | `= desc.MipLevels` |
| shaders.hlsl | `Sample(...)` | `Sample(...)` 동일 |
| 결과 | 항상 mip0 고정 | 거리에 따라 자동 LOD |

### LOD를 셰이더에서 직접 제어하는 변형 함수들

```hlsl
// 특수 효과가 필요할 때만 사용 (Ch10에서는 미사용)
texDiffuse.SampleLevel(..., 3.0f);       // 항상 mip3 강제
texDiffuse.SampleBias(...,  -1.0f);      // LOD를 1단계 낮춤 (더 선명)
texDiffuse.SampleGrad(..., ddx(uv), ddy(uv)); // ddx/ddy 수동 전달
```

일반 렌더링에서는 `Sample()` 하나로 충분하며, mipmap 지원은 SRV 설정과 GPU 하드웨어가 전담합니다.

---

## Q. BC(Block Compression) 압축은 언제, 누가 하는가? NVIDIA 전용인가?

### 압축 시점: 오프라인 (에셋 제작 시, 런타임 아님)

```
[에셋 제작 파이프라인 — 오프라인]
원본 PNG/TGA (RGBA 32bit)
  ↓ texconv.exe 등 툴이 CPU로 수 초~수 분간 압축
miku.dds (BC1/BC3/BC7 압축 완료 + mipmap 포함)
  ↓ 게임 배포

[런타임]
miku.dds 읽기 → CPU RAM (압축 상태 그대로)
  → UPLOAD 힙 (압축 상태 그대로)
  → DEFAULT 힙 VRAM (압축 상태 그대로)
  → Sample() 시 GPU TMU 하드웨어가 즉석 압축 해제
```

CPU도, 드라이버도 압축 해제를 하지 않습니다. 압축 데이터가 VRAM에 그대로 올라가고 GPU 하드웨어가 처리합니다.

### NVIDIA 전용인가?

**아닙니다. BC 포맷은 DirectX 표준 사양이며 모든 DX11/DX12 GPU가 지원합니다.**

| BC 포맷 | 지원 조건 | NVIDIA | AMD | Intel |
|---------|-----------|--------|-----|-------|
| BC1~BC3 | D3D9.1 이상 | ✅ | ✅ | ✅ |
| BC4~BC5 | D3D10 이상 | ✅ | ✅ | ✅ |
| BC6H, BC7 | D3D11 이상 | ✅ | ✅ | ✅ |

### BC1 압축 원리 (4×4 블록 = 8바이트, 8:1 압축)

```
원본 4×4 픽셀 블록 (64바이트)
  ↓
[color0: 2바이트][color1: 2바이트][각 픽셀 인덱스: 4바이트]
  color0, color1 = 블록 내 대표 색상 2개
  나머지 픽셀 = 두 색상의 보간값 중 하나 (2비트 인덱스)
```

GPU TMU는 Sample() 시 이 8바이트로 원본 픽셀을 즉석 복원합니다. VRAM 사용량 8배 절감 효과.

### BC 포맷 종류별 용도

| 포맷 | 압축비 | 주 용도 |
|------|--------|---------|
| BC1 (DXT1) | 8:1 | 불투명 디퓨즈 |
| BC3 (DXT5) | 4:1 | 반투명 텍스처, 나뭇잎 |
| BC4 | 8:1 | 높이맵, 거칠기 맵 (R만) |
| BC5 | 4:1 | 노멀맵 (RG) |
| BC6H | 4:1 | HDR 환경맵 |
| BC7 | 4:1 | 최고 품질 RGBA |

---

## Q. 하드웨어 고정 회로(Fixed Function)란 무엇인가?

### 프로그래머블 vs 고정 회로

```
[셰이더 코어 — 프로그래머블]
  메모리에서 명령어 fetch → decode → execute
  어떤 연산이든 가능, 유연하지만 여러 사이클 소요

[고정 회로 — Fixed Function]
  입력 → [배선된 트랜지스터 로직] → 출력
  명령어 없음, 항상 같은 연산만, 1~수 클럭 안에 완료
```

BC 압축 해제 연산(color0/color1 보간)은 절대 바뀌지 않으므로 실리콘에 배선으로 굳혀두면:
- 셰이더 코어 점유 없음
- 초당 수억 번 텍스처 샘플링을 병렬 처리 가능

### GPU에서 고정 회로인 것들

| 유닛 | 하는 일 | 프로그래머 제어 |
|------|---------|----------------|
| 래스터라이저 | 삼각형 → 픽셀 변환 | 설정값만 (뷰포트, 컬링) |
| TMU (텍스처 매핑 유닛) | UV 보간, BC 압축 해제, 필터링 | 샘플러 설정값만 |
| ROP (출력 병합 유닛) | 알파 블렌딩, 뎁스 테스트 | 블렌드 스테이트 설정값만 |
| 셰이더 코어 | 정점/픽셀 연산 | HLSL 코드 전체 |

BC 압축 해제는 TMU 안의 고정 회로이므로, `Sample()` 호출 시 **셰이더 코어를 전혀 사용하지 않고** TMU가 독립적으로 처리합니다.
