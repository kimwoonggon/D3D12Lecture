# Chapter 08 — Dynamic Descriptor Table (CBV + SRV) 학습 요약

---

## 1. 이 챕터의 핵심 주제

Chapter 07까지는 CBV(상수 버퍼) 하나만 디스크립터 테이블로 전달했다.  
Chapter 08에서는 CBV와 SRV(텍스처)를 **하나의 디스크립터 테이블**에 묶어 GPU에 전달하는 방식으로 확장한다.

---

## 2. DESCRIPTOR_COUNT_FOR_DRAW = 2 인 이유

```cpp
// BasicMeshObject.h
static const UINT DESCRIPTOR_COUNT_FOR_DRAW = 2;  // | Constant Buffer | Tex |
```

루트 시그니처에서 하나의 디스크립터 테이블이 두 개의 range로 구성된다.

```cpp
// BasicMeshObject.cpp - InitRootSignature()
CD3DX12_DESCRIPTOR_RANGE ranges[2] = {};
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // b0 : 상수 버퍼
ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // t0 : 텍스처

CD3DX12_ROOT_PARAMETER rootParameters[1] = {};
rootParameters[0].InitAsDescriptorTable(_countof(ranges), ranges, D3D12_SHADER_VISIBILITY_ALL);
```

두 range가 하나의 테이블로 묶이므로 GPU 힙에서 드로우 1회당 **연속 2슬롯**이 필요하다.  
이 값 하나가 DescriptorPool 전체 크기, AllocDescriptorTable 요청 크기, CopyDescriptors 횟수를 모두 결정한다.

```
DescriptorPool 총 슬롯 = MAX_DRAW_COUNT_PER_FRAME(256) × DESCRIPTOR_COUNT_FOR_DRAW(2) = 512
```

```cpp
// D3D12Renderer.cpp
m_pDescriptorPool->Initialize(
    m_pD3DDevice,
    MAX_DRAW_COUNT_PER_FRAME * CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW  // 512
);
```

---

## 3. 세 가지 리소스 풀의 역할

### 3-1. DescriptorPool (Shader Visible — 프레임 임시)

```cpp
// DescriptorPool.cpp
commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
```

- GPU 셰이더가 직접 읽는 힙이다.
- `SetDescriptorHeaps()`에 전달되어 CommandList에 바인딩된다.
- 드로우마다 `AllocDescriptorTable`로 2칸 블록을 할당하고, 프레임 끝에 `Reset()`으로 카운터만 0으로 되돌린다.
- CPU 핸들(쓰기)과 GPU 핸들(GPU 참조)을 동시에 반환한다.

```cpp
// DescriptorPool.cpp - AllocDescriptorTable
*pOutCPUDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cpuDescriptorHandle, m_AllocatedDescriptorCount, m_srvDescriptorSize);
*pOutGPUDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuDescriptorHandle, m_AllocatedDescriptorCount, m_srvDescriptorSize);
m_AllocatedDescriptorCount += DescriptorCount;
```

### 3-2. SingleDescriptorAllocator (Non-Shader Visible — 텍스처 영구 보관)

```cpp
// D3D12Renderer.cpp
m_pSingleDescriptorAllocator->Initialize(
    m_pD3DDevice, MAX_DESCRIPTOR_COUNT,
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE  // ← Shader Visible 아님
);
```

- 텍스처 로드 시점에 슬롯을 할당하고 `CreateShaderResourceView`로 SRV를 생성한다.
- 텍스처 수명과 동일하게 유지된다. 삭제 시 `FreeDescriptorHandle`로 슬롯을 반납한다.
- GPU 셰이더가 직접 읽지 못하므로 Draw 시점에 DescriptorPool로 복사되어 사용된다.
- `CIndexCreator`로 빈 슬롯을 재사용할 수 있다 (Pool과 달리 개별 반납 가능).

```cpp
// D3D12Renderer.cpp - CreateTiledTexture()
if (m_pSingleDescriptorAllocator->AllocDescriptorHandle(&srv))      // 영구 슬롯 확보
{
    m_pD3DDevice->CreateShaderResourceView(pTexResource, &SRVDesc, srv);
    pTexHandle->srv = srv;  // TEXTURE_HANDLE에 보관
}

// DeleteTexture() 시점
m_pSingleDescriptorAllocator->FreeDescriptorHandle(srv);            // 그때서야 반납
```

### 3-3. SimpleConstantBufferPool (Non-Shader Visible — 상수 버퍼 스테이징)

```cpp
// SimpleConstantBufferPool.cpp
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;  // CBV 힙도 Non-Shader Visible
```

- 하나의 Upload 힙 리소스를 `MAX_DRAW_COUNT_PER_FRAME` 등분하여 각 구간에 CBV를 미리 생성한다.
- 드로우마다 `Alloc()`으로 독립된 CBV 컨테이너를 받아 오프셋 데이터를 쓴다.
- CBV 힙도 Non-Shader Visible이며, Draw 시점에 DescriptorPool로 복사된다.
- 프레임 끝에 `Reset()`으로 카운터를 0으로 되돌린다.

---

## 4. Draw() 내부 4단계 순서와 이유

```cpp
// BasicMeshObject.cpp - Draw()

// ① 레이아웃 계약 선언
pCommandList->SetGraphicsRootSignature(m_pRootSignature);

// ② GPU가 참조할 힙을 CommandList에 바인딩
pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);

// ③ 이번 드로우 전용 슬롯에 CBV, SRV를 복사 (CPU 즉시 실행)
CD3DX12_CPU_DESCRIPTOR_HANDLE cbvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_CBV, srvDescriptorSize);
pD3DDeivce->CopyDescriptorsSimple(1, cbvDest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

CD3DX12_CPU_DESCRIPTOR_HANDLE srvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_TEX, srvDescriptorSize);
pD3DDeivce->CopyDescriptorsSimple(1, srvDest, srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

// ④ 루트 파라미터 슬롯 0번이 힙의 어느 위치를 가리킬지 기록
pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
```

### 각 단계가 관장하는 범위

| 단계 | 함수 | 범위 | 실행 주체 |
|---|---|---|---|
| ① | `SetGraphicsRootSignature` | 루트 파라미터 레이아웃 정의 | CPU → CommandList 기록 |
| ② | `SetDescriptorHeaps` | CommandList에 사용할 힙 등록 | CPU → CommandList 기록 |
| ③ | `CopyDescriptorsSimple` | 힙의 지정 슬롯에 데이터 복사 | **CPU 즉시 실행** |
| ④ | `SetGraphicsRootDescriptorTable` | 루트 슬롯 0번 → GPU 힙 주소 연결 | CPU → CommandList 기록 |

### 순서가 강제되는 이유

```
① 루트 시그니처가 없으면 "슬롯 0번"의 의미가 정의되지 않는다
        ↓
② CommandList에 힙이 바인딩되어야 cpuDescriptorTable/gpuDescriptorTable이
   그 힙 내의 유효한 위치를 가리킨다는 것이 보장된다
        ↓
③ CopyDescriptorsSimple은 CPU 즉시 실행이므로 DrawInstanced의
   GPU 실행 시점에는 이미 데이터가 완성되어 있다
        ↓
④ 완성된 위치의 GPU 핸들을 CommandList에 기록한다
```

> `CopyDescriptorsSimple`은 CommandList에 기록되는 GPU 커맨드가 아니다.  
> CPU가 즉시 메모리에 쓰는 함수이므로, GPU가 실제로 힙을 읽는 `DrawInstanced` 실행 시점에는 이미 복사가 완료되어 있다.

---

## 5. Static Sampler — DescriptorPool 슬롯을 소비하지 않는 이유

```cpp
// BasicMeshObject.cpp - InitRootSignature()
D3D12_STATIC_SAMPLER_DESC sampler = {};
SetDefaultSamplerDesc(&sampler, 0);
sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

rootSignatureDesc.Init(_countof(rootParameters), rootParameters,
    1, &sampler,   // ← 루트 시그니처 자체에 내장
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
```

샘플러는 `D3D12_STATIC_SAMPLER_DESC`로 루트 시그니처에 포함되어 PSO 생성 시 고정된다.  
별도의 디스크립터 힙 슬롯이 필요 없으므로 `DESCRIPTOR_COUNT_FOR_DRAW`가 3이 아닌 **2**다.  
텍스처마다 다른 샘플러가 필요한 경우에만 Dynamic Sampler(별도 힙 슬롯 소비)를 사용한다.

---

## 6. 전체 데이터 흐름

```
──── 앱 초기화 시 ────────────────────────────────────────────────

GPU Resource (Texture)
    └──CreateShaderResourceView──▶ SingleDescriptorAllocator 힙 [슬롯 N]
                                   (FLAG_NONE, 텍스처 수명 동안 유지)

──── 매 프레임 렌더링 ────────────────────────────────────────────

ConstantBufferPool::Alloc()
    └── 상수 버퍼 메모리에 offset.x, offset.y 기록
    └── CBVHandle (Non-Shader Visible 힙 슬롯 M)

DescriptorPool::AllocDescriptorTable(2칸)
    └── cpuDescriptorTable [슬롯 K  ] ← CBV 복사 목적지
    └── cpuDescriptorTable [슬롯 K+1] ← SRV 복사 목적지
    └── gpuDescriptorTable [슬롯 K  ] ← SetGraphicsRootDescriptorTable 인자

CopyDescriptorsSimple
    ├── CBVHandle(슬롯 M)  ──복사──▶ DescriptorPool[슬롯 K  ]
    └── SRV(슬롯 N)        ──복사──▶ DescriptorPool[슬롯 K+1]

SetGraphicsRootDescriptorTable(0, gpuDescriptorTable[슬롯 K])
    └── GPU 셰이더: b0 ← [슬롯 K], t0 ← [슬롯 K+1]

DrawInstanced() → GPU 실행

──── 프레임 끝 (GPU 완료 확인 후) ──────────────────────────────

DescriptorPool::Reset()       → m_AllocatedDescriptorCount = 0
ConstantBufferPool::Reset()   → m_AllocatedCBVNum = 0
```

---

## 7. 핵심 설계 원칙 요약

| 원칙 | 내용 |
|---|---|
| 드로우당 독립성 | 매 드로우마다 별도의 CBV 슬롯과 DescriptorPool 슬롯을 할당하여 GPU 실행 중 데이터 오염을 방지한다 |
| 소스/목적지 힙 분리 | Non-Shader Visible(원본 보관) → `CopyDescriptorsSimple` → Shader Visible(GPU 읽기) |
| 프레임 단위 Reset | Fence로 GPU 완료를 확인한 뒤에만 Pool 카운터를 리셋한다. GPU가 읽는 도중 덮어쓰면 안 된다 |
| Static Sampler 활용 | 공통 샘플러는 루트 시그니처에 내장하여 힙 슬롯 소비를 줄인다 |
| DESCRIPTOR_COUNT_FOR_DRAW 단일 관리 | 이 값 하나가 Pool 크기, 테이블 슬롯 수, CopyDescriptors 횟수를 연쇄적으로 결정한다 |
