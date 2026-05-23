# Chapter 08 — Dynamic Descriptor Table (CBV + SRV) 객관식 퀴즈

> 정답 및 해설은 각 문제 하단의 `<details>` 블록에 있습니다.

---

## Q1. `DESCRIPTOR_COUNT_FOR_DRAW`가 2인 이유는?

```cpp
static const UINT DESCRIPTOR_COUNT_FOR_DRAW = 2;  // | Constant Buffer | Tex |
```

A) 스왑체인 버퍼가 2개이므로 백버퍼 인덱스만큼 맞춘 것이다  
B) 루트 파라미터가 2개이므로 파라미터 수와 일치시킨 것이다  
C) 하나의 드로우콜이 CBV 슬롯 1개와 SRV 슬롯 1개, 총 2개의 연속 디스크립터를 점유하기 때문이다  
D) `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE`의 최소 할당 단위가 2이기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: C**

`InitRootSignature()`에서 디스크립터 테이블은 두 개의 range로 구성된다.

```cpp
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // b0
ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // t0
rootParameters[0].InitAsDescriptorTable(_countof(ranges), ranges, ...);
```

두 range가 하나의 테이블로 묶이므로 GPU 힙에서 드로우당 연속 2칸이 필요하다.  
`DescriptorPool` 전체 슬롯 수는 `MAX_DRAW_COUNT_PER_FRAME × 2`로 결정된다.

</details>

---

## Q2. `DescriptorPool`이 생성하는 힙의 `Flags`는 무엇이며, 그 이유는?

A) `D3D12_DESCRIPTOR_HEAP_FLAG_NONE` — CPU에서만 쓰기 때문이다  
B) `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE` — GPU 셰이더가 직접 이 힙을 참조하기 때문이다  
C) `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE` — CPU에서 `Map()`으로 쓰기 위해 필요하기 때문이다  
D) `D3D12_DESCRIPTOR_HEAP_FLAG_NONE` — `CopyDescriptorsSimple`의 소스로 사용되기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

```cpp
commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
```

`SetDescriptorHeaps()`에 전달되어 GPU 파이프라인이 직접 읽는 힙이므로 Shader Visible이어야 한다.  
반면 `SimpleConstantBufferPool`의 CBV 힙과 `SingleDescriptorAllocator`는 `FLAG_NONE`으로 만들어지는데,  
이 힙들은 CPU 핸들로만 접근하는 **스테이징(staging)** 힙이다.

</details>

---

## Q3. 아래 코드에서 `SetDescriptorHeaps`를 `CopyDescriptorsSimple` **이전**에 호출하는 이유로 가장 적절한 것은?

```cpp
pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);          // ①
pD3DDeivce->CopyDescriptorsSimple(1, cbvDest, pCB->CBVHandle, ...); // ②
pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable); // ③
```

A) `SetDescriptorHeaps`를 먼저 호출해야 GPU 힙의 CPU 핸들이 유효해지기 때문이다  
B) `CopyDescriptorsSimple`은 내부적으로 CommandList의 바인딩된 힙으로 복사 대상을 확인하기 때문이다  
C) `cpuDescriptorTable`이 가리키는 힙이 CommandList에 바인딩된 힙과 동일함을 보장해야 이후 GPU 실행 시 정합성이 유지되기 때문이다  
D) DirectX 12 스펙상 `CopyDescriptors` 계열 함수는 반드시 `SetDescriptorHeaps` 이후에 호출해야 하기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: C**

`CopyDescriptorsSimple`은 순수 CPU 함수이며 CommandList 상태를 확인하지 않는다.  
순서의 핵심은 **논리적 정합성**이다.  
- `AllocDescriptorTable`이 반환한 `cpuDescriptorTable`은 `pDescriptorHeap` 힙 내의 CPU 핸들이다.  
- `SetDescriptorHeaps`로 해당 힙을 CommandList에 등록해야, 나중에 `SetGraphicsRootDescriptorTable`에 전달하는 `gpuDescriptorTable`(같은 힙의 GPU 핸들)이 유효하다.  
- GPU가 드로우를 실행할 때, 바인딩된 힙과 `gpuDescriptorTable`이 속한 힙이 일치해야 하므로 복사 전에 힙을 확정해 두는 것이 올바른 순서다.

</details>

---

## Q4. `CopyDescriptorsSimple`이 CPU 측에서 수행되어야 하는 이유는?

A) GPU Shader Visible 힙은 GPU에서만 쓰기 가능하므로 CPU에서는 복사가 불가능하다  
B) Shader Visible 힙은 GPU에서 읽기 전용이며, CPU에서도 CPU 핸들을 통해 쓰기가 가능하다  
C) `CopyDescriptorsSimple`은 GPU 커맨드로 기록되어 GPU 타임라인에서 실행된다  
D) DirectX 12에서 CPU와 GPU가 동시에 같은 힙에 접근하는 것은 허용된다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

Shader Visible 힙은 GPU가 **읽기**에 사용하고, CPU는 **CPU 핸들(D3D12_CPU_DESCRIPTOR_HANDLE)** 을 통해 **쓰기**가 가능하다.  
`CopyDescriptorsSimple`은 CommandList에 기록되는 GPU 커맨드가 아니라 즉시 실행되는 CPU 함수다.  
따라서 `DrawInstanced`의 GPU 실행 시점에는 복사가 이미 완료되어 있다.

</details>

---

## Q5. `AllocDescriptorTable`이 프레임마다 카운터를 누적하다가 `Reset()`으로 초기화하는 방식을 사용하는 이유는?

A) 힙의 물리적 메모리를 재할당(realloc)해서 새 데이터를 쓰기 위해서다  
B) GPU가 한 프레임의 커맨드를 모두 실행하기 전까지 그 프레임에서 할당된 슬롯이 덮어써지면 안 되기 때문이다  
C) `ID3D12DescriptorHeap`은 한 번 쓰면 수정 불가능한 immutable 객체이기 때문이다  
D) `Reset()` 없이 카운터를 0으로 되돌리면 힙이 해제되기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

GPU는 CPU와 비동기로 동작한다. CPU가 N+1 프레임을 준비하는 동안 GPU는 N 프레임 커맨드를 실행 중일 수 있다.  
`Reset()`은 Fence를 통해 GPU가 N 프레임 실행을 완료했음을 확인한 **다음 프레임 시작 시점**에 호출된다.  
그 이전에 슬롯을 재사용하면 GPU가 읽는 도중 데이터가 덮어써지는 문제가 발생한다.

</details>

---

## Q6. `SimpleConstantBufferPool`의 CBV 힙 `Flags`가 `D3D12_DESCRIPTOR_HEAP_FLAG_NONE`인 이유는?

A) 상수 버퍼는 GPU 셰이더에서 직접 접근하지 않아도 되기 때문이다  
B) 이 힙은 `CopyDescriptorsSimple`의 **소스**로만 사용되며, 셰이더가 직접 읽지 않기 때문이다  
C) `D3D12_HEAP_TYPE_UPLOAD` 리소스는 `SHADER_VISIBLE` 힙과 함께 사용할 수 없기 때문이다  
D) `CreateConstantBufferView`는 `FLAG_NONE` 힙에서만 동작하기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

`SimpleConstantBufferPool`의 힙은 CPU가 CBV를 생성해 두는 **스테이징 힙**이다.  
Draw 시점에 `CopyDescriptorsSimple`로 Shader Visible 힙(`DescriptorPool`)에 복사되어 GPU가 사용한다.  
소스 역할만 하므로 Shader Visible 플래그가 불필요하다.  
`SHADER_VISIBLE` 힙은 비용이 더 크고 슬롯 수에 제한이 있을 수 있으므로 불필요한 경우 사용하지 않는 것이 좋다.

</details>

---

## Q7. 아래 루트 시그니처 설정에서 `D3D12_SHADER_VISIBILITY_ALL`을 사용할 때 발생하는 문제점은?

```cpp
rootParameters[0].InitAsDescriptorTable(_countof(ranges), ranges, D3D12_SHADER_VISIBILITY_ALL);
```

A) 픽셀 셰이더가 CBV를 읽지 못한다  
B) 텍스처(SRV)를 버텍스 셰이더에서도 읽을 수 있게 되어 보안 취약점이 발생한다  
C) 코드는 동작하지만 불필요한 셰이더 스테이지까지 접근이 허용되어 성능 최적화 기회가 줄어든다  
D) `DENY_PIXEL_SHADER_ROOT_ACCESS` 플래그와 충돌하여 런타임 오류가 발생한다  

<details>
<summary>정답 및 해설</summary>

**정답: C**

`SHADER_VISIBILITY_ALL`은 모든 셰이더 스테이지에서 이 테이블에 접근을 허용한다.  
코드의 `rootSignatureFlags`에는 `DENY_PIXEL_SHADER_ROOT_ACCESS`가 설정되어 있어 **플래그 간 모순**이 있지만 실제로는 동작한다.  
이상적으로는 CBV와 텍스처를 버텍스/픽셀 셰이더 용도에 맞게 `SHADER_VISIBILITY_VERTEX` / `SHADER_VISIBILITY_PIXEL`로 나누는 것이 GPU 최적화에 유리하다.

</details>

---

## Q8. `SetGraphicsRootSignature`를 `Draw()` 호출마다 반복하는 코드의 문제점과 현실적 이유는?

A) 루트 시그니처는 CommandList당 한 번만 설정 가능하므로 매번 호출하면 오류가 발생한다  
B) 동일한 PSO를 사용하는 드로우콜들은 루트 시그니처가 같으므로 중복 호출이 되지만, 단순화를 위해 허용하고 있다  
C) 오브젝트마다 다른 루트 시그니처를 사용할 수도 있으므로 매번 명시적으로 설정하는 것은 정확한 방식이다  
D) B와 C 모두 맞다  

<details>
<summary>정답 및 해설</summary>

**정답: D**

- 같은 오브젝트 타입이라면 루트 시그니처가 동일하므로 매번 호출하면 중복 설정이다 (B).  
- 그러나 오브젝트 종류가 달라지면 루트 시그니처가 달라질 수 있으므로 각 Draw 함수가 자신의 루트 시그니처를 명시적으로 설정하는 것은 구조적으로 올바른 패턴이다 (C).  
- 성능이 중요한 프로덕션 코드에서는 `m_pRootSignature`가 이미 바인딩된 것과 동일할 때 `SetGraphicsRootSignature` 호출을 생략하는 최적화를 적용한다.

</details>

---

## Q9. `DescriptorPool::AllocDescriptorTable`이 CPU 핸들과 GPU 핸들을 **동시에** 반환하는 이유는?

A) CPU 핸들은 `CopyDescriptorsSimple` 대상에, GPU 핸들은 `SetGraphicsRootDescriptorTable`에 각각 사용되기 때문이다  
B) CPU 핸들은 `SetDescriptorHeaps`에, GPU 핸들은 `CopyDescriptorsSimple`에 사용되기 때문이다  
C) 두 핸들은 서로 다른 힙의 핸들이므로 별도로 관리해야 하기 때문이다  
D) GPU 핸들만으로는 `CreateConstantBufferView`를 호출할 수 없기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: A**

같은 힙의 같은 위치를 가리키지만 용도가 다르다.  
- `cpuDescriptorTable(CPU 핸들)` → `CopyDescriptorsSimple`의 **목적지**: CPU가 이 위치에 CBV/SRV를 복사한다.  
- `gpuDescriptorTable(GPU 핸들)` → `SetGraphicsRootDescriptorTable`의 **인자**: GPU가 드로우 시 이 위치부터 디스크립터를 읽기 시작한다.

</details>

---

## Q10. `HLSL`의 `texDiffuse.Sample(samplerDiffuse, input.TexCoord)`에서 `samplerDiffuse`는 어떻게 바인딩되는가?

```hlsl
Texture2D texDiffuse : register(t0);
SamplerState samplerDiffuse : register(s0);
```

```cpp
D3D12_STATIC_SAMPLER_DESC sampler = {};
SetDefaultSamplerDesc(&sampler, 0);
sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
rootSignatureDesc.Init(..., 1, &sampler, ...);
```

A) 드로우마다 `SetGraphicsRootDescriptorTable`로 동적으로 바인딩된다  
B) `DescriptorPool`의 힙에 슬롯 하나를 더 할당하여 바인딩된다  
C) 루트 시그니처에 Static Sampler로 직접 내장되어 있어 별도의 디스크립터 힙 슬롯이 필요 없다  
D) `SimpleConstantBufferPool`처럼 별도의 Sampler 전용 Pool에서 할당된다  

<details>
<summary>정답 및 해설</summary>

**정답: C**

`D3D12_STATIC_SAMPLER_DESC`는 루트 시그니처 자체에 포함되어 파이프라인 생성 시 고정된다.  
따라서 `DESCRIPTOR_COUNT_FOR_DRAW`가 3이 아닌 **2**인 것이다. 샘플러는 힙 슬롯을 소비하지 않는다.  
만약 텍스처마다 다른 샘플러가 필요하다면 Dynamic Sampler(별도 힙 필요)를 써야 한다.

</details>

---

## Q11. `InitCommonResources()`에서 레퍼런스 카운트(`m_dwInitRefCount`)를 사용하는 이유는?

```cpp
BOOL CBasicMeshObject::InitCommonResources()
{
    if (m_dwInitRefCount)
        goto lb_true;

    InitRootSinagture();
    InitPipelineState();

lb_true:
    m_dwInitRefCount++;
    return m_dwInitRefCount;
}
```

A) 루트 시그니처와 PSO는 오브젝트마다 독립적으로 생성해야 하기 때문이다  
B) 루트 시그니처와 PSO는 `static`으로 모든 인스턴스가 공유하므로 첫 번째 인스턴스 생성 시에만 초기화하기 위해서다  
C) DirectX 12에서 `CreateRootSignature`는 동일한 설명으로 두 번 호출하면 오류를 반환하기 때문이다  
D) 멀티스레드 안전(thread-safe)을 위한 원자적 카운터로 사용하기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

`m_pRootSignature`와 `m_pPipelineState`는 `static` 멤버 변수다. 즉, 모든 `CBasicMeshObject` 인스턴스가 하나의 루트 시그니처와 PSO를 공유한다.  
`m_dwInitRefCount`가 0일 때만 초기화하고 이후에는 카운트만 증가시켜 불필요한 중복 생성을 방지한다.  
`CleanupSharedResources()`에서 카운트가 0이 될 때 비로소 해제된다.

</details>

---

## Q12. `Draw()` 함수 내에서 매번 `pConstantBufferPool->Alloc()`으로 새 CBV 컨테이너를 할당하는 이유는?

A) GPU가 CommandList를 실행하는 동안 CPU에서 상수 버퍼 내용을 덮어쓰면 안 되기 때문이다  
B) `CSimpleConstantBufferPool`이 내부적으로 리소스를 재사용하지 못하도록 설계되어 있기 때문이다  
C) 상수 버퍼는 드로우마다 다른 GPU 가상 주소를 요구하기 때문이다  
D) DirectX 12의 Constant Buffer는 1회용이어서 같은 버퍼를 두 번 사용할 수 없기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: A**

상수 버퍼는 `UPLOAD` 힙에 CPU-GPU 공유 메모리로 존재한다.  
만약 같은 버퍼를 재사용하면 GPU가 이전 드로우콜을 처리하는 도중 CPU가 값을 덮어써 데이터 오염이 발생한다.  
각 드로우마다 독립된 CBV 슬롯(다른 메모리 영역)을 할당하면 GPU가 N번 드로우의 데이터를 읽는 동안 CPU는 N+1번 드로우를 위해 다른 슬롯에 쓸 수 있다.

</details>

---

## Q13. `DescriptorPool`의 총 용량을 계산하는 공식으로 올바른 것은?

```cpp
m_pDescriptorPool->Initialize(m_pD3DDevice,
    MAX_DRAW_COUNT_PER_FRAME * CBasicMeshObject::DESCRIPTOR_COUNT_FOR_DRAW);
```

`MAX_DRAW_COUNT_PER_FRAME = 256`, `DESCRIPTOR_COUNT_FOR_DRAW = 2`일 때 정답은?

A) 256 슬롯 (드로우 수와 같다)  
B) 512 슬롯 (드로우당 2슬롯)  
C) 1024 슬롯 (스왑체인 버퍼 2개 × 드로우당 2슬롯 × 드로우 수)  
D) 258 슬롯 (RTV 2개 추가)  

<details>
<summary>정답 및 해설</summary>

**정답: B**

`256 × 2 = 512 슬롯`.  
RTV는 별도의 RTV 힙에 관리되며 이 힙과 무관하다.  
스왑체인 버퍼 수는 CPU-GPU 동기화(Fence)로 처리되므로 힙을 n배 키울 필요가 없다.

</details>

---

## Q14. `SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)`에서 `0`은 무엇을 의미하는가?

A) 힙에서 첫 번째 슬롯의 인덱스  
B) 루트 파라미터 배열의 인덱스 (0번 루트 파라미터)  
C) 디스크립터 테이블에서 CBV가 위치한 오프셋  
D) `SetDescriptorHeaps`에 전달한 힙 배열의 인덱스  

<details>
<summary>정답 및 해설</summary>

**정답: B**

`rootParameters[0]`으로 등록한 루트 파라미터의 인덱스다.  

```cpp
CD3DX12_ROOT_PARAMETER rootParameters[1] = {};
rootParameters[0].InitAsDescriptorTable(...);   // ← 인덱스 0
...
pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);
//                                           ↑ rootParameters[0]에 바인딩
```

</details>

---

## Q15. `CD3DX12_CPU_DESCRIPTOR_HANDLE cbvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_CBV, srvDescriptorSize)`에서 세 번째 인자 `srvDescriptorSize`의 역할은?

A) 힙의 전체 크기를 바이트 단위로 지정한다  
B) 디스크립터 하나의 크기(stride)를 곱해 `BASIC_MESH_DESCRIPTOR_INDEX_CBV` 번째 슬롯의 실제 주소를 계산한다  
C) CBV와 SRV의 크기가 달라서 변환 계수로 사용한다  
D) `CopyDescriptorsSimple`이 복사할 바이트 수를 지정한다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

`CD3DX12_CPU_DESCRIPTOR_HANDLE`의 오프셋 생성자 시그니처:

```cpp
CD3DX12_CPU_DESCRIPTOR_HANDLE(base, offsetInDescriptors, descriptorIncrementSize)
// 실제 주소 = base.ptr + offsetInDescriptors × descriptorIncrementSize
```

`GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)`로 얻은 값을 stride로 사용하여  
슬롯 번호(`BASIC_MESH_DESCRIPTOR_INDEX_CBV = 0`, `BASIC_MESH_DESCRIPTOR_INDEX_TEX = 1`)를 실제 메모리 오프셋으로 변환한다.

</details>

---

## Q16. `D3D12_FILTER_MIN_MAG_MIP_POINT`를 사용한 경우 텍스처 표현의 특징은?

A) 텍스처가 확대될 때 인접 픽셀 간 선형 보간이 적용되어 부드럽게 보인다  
B) 텍스처가 확대·축소될 때 가장 가까운 텍셀 하나만 선택하여 픽셀화된(blocky) 외형이 된다  
C) 밉맵 레벨 간에만 선형 보간이 적용된다  
D) 이방성 필터링(Anisotropic)의 일종으로 경사진 표면에 최적화된다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

`POINT` 필터는 Nearest-neighbor 방식이다. Minification, Magnification, Mip 모두 가장 가까운 값을 선택하므로 확대 시 도트(pixel art) 느낌이 난다.  
부드러운 표현이 필요하면 `D3D12_FILTER_MIN_MAG_MIP_LINEAR`를 사용한다.

</details>

---

## Q17. 이 챕터에서 `SingleDescriptorAllocator`와 `DescriptorPool`의 역할 차이는?

A) `SingleDescriptorAllocator`는 Shader Visible 힙, `DescriptorPool`은 Non-Shader Visible 힙을 관리한다  
B) `SingleDescriptorAllocator`는 Non-Shader Visible 힙에서 개별 디스크립터(예: 텍스처 SRV)를 영구 할당하고, `DescriptorPool`은 Shader Visible 힙에서 드로우당 임시 테이블을 할당한다  
C) 둘 다 동일한 역할이며 백업용으로 중복 존재한다  
D) `SingleDescriptorAllocator`는 CBV 전용, `DescriptorPool`은 SRV 전용이다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

| 구분 | `SingleDescriptorAllocator` | `DescriptorPool` |
|---|---|---|
| 힙 종류 | Non-Shader Visible (`FLAG_NONE`) | Shader Visible (`SHADER_VISIBLE`) |
| 수명 | 텍스처 리소스 수명과 동일 (영구) | 프레임 단위 (`Reset()`으로 초기화) |
| 역할 | 텍스처 SRV 원본 보관 | 드로우당 CBV+SRV를 복사받아 GPU에 제공 |

텍스처 SRV는 `SingleDescriptorAllocator`에 한 번 만들어 두고, Draw 시마다 `CopyDescriptorsSimple`로 `DescriptorPool` 힙에 복사한다.

</details>

---

## Q18. 아래 PSO 설정에서 `DepthStencilState.DepthEnable = FALSE`로 설정한 상태로 삼각형 두 개를 겹쳐서 그릴 때 결과는?

```cpp
psoDesc.DepthStencilState.DepthEnable = FALSE;
psoDesc.DepthStencilState.StencilEnable = FALSE;
```

A) 먼저 그린 삼각형이 항상 위에 보인다 (Painter's Algorithm)  
B) 나중에 그린 삼각형이 항상 위에 보인다 (단순 덮어쓰기)  
C) 두 삼각형이 Z값에 따라 자동으로 정렬되어 올바른 깊이 순서로 그려진다  
D) 렌더링이 실패하고 검은 화면이 출력된다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

Depth Test가 비활성화되면 각 픽셀의 깊이 비교 없이 래스터라이저가 렌더 타겟에 그대로 쓴다.  
따라서 CommandList에서 나중에 `DrawInstanced`로 호출된 오브젝트가 픽셀을 덮어쓰게 된다.  
이 챕터의 예제는 단일 삼각형이라 깊이 버퍼가 필요 없으므로 의도적으로 비활성화했다.

</details>

---

## Q19. `D3D12SerializeRootSignature` 직후 `CreateRootSignature`를 호출한 뒤 `pSignature` blob을 즉시 `Release()`하는 이유는?

A) `pSignature`를 해제하지 않으면 루트 시그니처 객체가 무효화되기 때문이다  
B) `CreateRootSignature`가 blob의 데이터를 내부적으로 복사해 두므로 blob은 더 이상 필요 없기 때문이다  
C) blob을 유지하면 GPU 메모리가 낭비되기 때문이다  
D) `D3DBlob`은 단일 참조만 허용하므로 소유권이 `CreateRootSignature`로 이전되기 때문이다  

<details>
<summary>정답 및 해설</summary>

**정답: B**

`CreateRootSignature`는 호출 시점에 blob에서 바이너리 데이터를 파싱하여 내부 GPU 객체를 생성한다.  
생성 완료 후 blob은 더 이상 참조되지 않으므로 COM 참조 카운트를 줄여(`Release`) 메모리를 반환하는 것이 올바르다.  
`ID3D12RootSignature` 객체 자체의 수명은 `m_pRootSignature` 포인터가 관리한다.

</details>

---

## Q20. 한 프레임에서 `DescriptorPool::Reset()`을 호출하기에 **가장 안전한 시점**은?

A) `DrawInstanced` 직후  
B) `ExecuteCommandLists` 직후  
C) Fence를 통해 GPU가 해당 프레임의 CommandList 실행을 완료했음을 확인한 후  
D) 다음 프레임의 `BeginRender` 시작 직전이면 언제든 상관없다  

<details>
<summary>정답 및 해설</summary>

**정답: C**

`ExecuteCommandLists`는 GPU에 작업을 **제출**할 뿐이며 완료를 보장하지 않는다.  
GPU가 CommandList를 실행하는 동안 `Reset()`으로 카운터를 0으로 되돌리면 다음 프레임의 `AllocDescriptorTable`이 같은 슬롯을 재사용하게 되어, 현재 프레임 실행 중인 GPU가 엉뚱한 데이터를 읽게 된다.  
반드시 Fence(`WaitForFenceValue` 등)로 GPU 완료를 확인한 뒤 `Reset()`을 호출해야 한다.

</details>
