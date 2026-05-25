# Chapter 09 — Depth Buffer 객관식 퀴즈

> 아래 퀴즈는 `09_DepthBuffer` 프로젝트의 실제 코드를 기반으로 출제되었습니다.  
> 각 문제의 보기 중 **정답 하나**를 고르세요. 정답과 해설은 문서 맨 아래에 있습니다.

---

## Q1. Depth-Stencil 리소스 포맷

`CreateDepthStencil()` 에서 `ID3D12Resource`를 생성할 때 사용하는 실제 리소스 포맷은 무엇인가?

```cpp
CD3DX12_RESOURCE_DESC depthDesc(
    D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0,
    Width, Height, 1, 1,
    ???,   // 이 부분
    1, 0, D3D12_TEXTURE_LAYOUT_UNKNOWN,
    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
```

- A) `DXGI_FORMAT_D32_FLOAT`
- B) `DXGI_FORMAT_R32_TYPELESS`
- C) `DXGI_FORMAT_R32G32_FLOAT`
- D) `DXGI_FORMAT_D24_UNORM_S8_UINT`

---

## Q2. DSV(Depth-Stencil View) 포맷

같은 `CreateDepthStencil()` 안에서 `D3D12_DEPTH_STENCIL_VIEW_DESC`의 `Format`으로 지정하는 값은?

- A) `DXGI_FORMAT_R32_TYPELESS`
- B) `DXGI_FORMAT_R32_FLOAT`
- C) `DXGI_FORMAT_D32_FLOAT`
- D) `DXGI_FORMAT_D16_UNORM`

---

## Q3. Depth Clear 값

`BeginRender()` 에서 `ClearDepthStencilView()`를 호출할 때 Depth 클리어 값으로 무엇을 사용하는가?

```cpp
m_pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, ???, 0, 0, nullptr);
```

- A) `0.0f`
- B) `0.5f`
- C) `-1.0f`
- D) `1.0f`

---

## Q4. DepthStencilState — DepthFunc

`InitPipelineState()` 에서 PSO의 `DepthStencilState.DepthFunc`에 설정된 비교 함수는?

```cpp
psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
psoDesc.DepthStencilState.DepthFunc = ???;
```

- A) `D3D12_COMPARISON_FUNC_LESS`
- B) `D3D12_COMPARISON_FUNC_LESS_EQUAL`
- C) `D3D12_COMPARISON_FUNC_ALWAYS`
- D) `D3D12_COMPARISON_FUNC_GREATER`

---

## Q5. PSO — DSVFormat

`InitPipelineState()` 에서 `psoDesc.DSVFormat`에 설정된 포맷은?

- A) `DXGI_FORMAT_R32_TYPELESS`
- B) `DXGI_FORMAT_D24_UNORM_S8_UINT`
- C) `DXGI_FORMAT_D32_FLOAT`
- D) `DXGI_FORMAT_R32_FLOAT`

---

## Q6. PSO — CullMode

`InitPipelineState()` 에서 래스터라이저의 `CullMode` 설정은?

- A) `D3D12_CULL_MODE_BACK`
- B) `D3D12_CULL_MODE_FRONT`
- C) `D3D12_CULL_MODE_NONE`
- D) 기본값(`D3D12_DEFAULT`)을 그대로 사용 (= BACK)

---

## Q7. Root Signature — Descriptor Range 구성

`InitRootSinagture()` 에서 정의된 `ranges[]`는 몇 개의 엔트리로 구성되며, 각각 무엇인가?

```cpp
CD3DX12_DESCRIPTOR_RANGE ranges[2] = {};
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // b0
ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // t0
```

- A) 1개 — CBV(b0) 하나만
- B) 2개 — CBV(b0), UAV(u0)
- C) 2개 — CBV(b0), SRV(t0)
- D) 3개 — CBV(b0), SRV(t0), UAV(u0)

---

## Q8. Root Parameter 개수

`InitRootSinagture()` 에서 Root Parameter는 몇 개이며, 어떤 타입인가?

- A) 2개의 Root Constant
- B) 1개의 Root Descriptor (CBV)
- C) 1개의 Descriptor Table (CBV + SRV)
- D) 2개의 Descriptor Table (CBV, SRV 각각)

---

## Q9. Descriptor Pool (`CDescriptorPool`)의 역할

`CDescriptorPool`과 `CSingleDescriptorAllocator`의 차이를 가장 잘 설명한 것은?

- A) `CDescriptorPool`은 CPU-only 힙, `CSingleDescriptorAllocator`는 GPU-visible 힙이다.
- B) `CDescriptorPool`은 매 프레임 `Reset()`으로 일괄 해제하는 렌더링용 GPU-visible 힙이고, `CSingleDescriptorAllocator`는 텍스처 등 수명이 긴 리소스를 위한 CPU-only 개별 할당 힙이다.
- C) 두 클래스는 동일한 힙을 공유하며 역할만 다르다.
- D) `CSingleDescriptorAllocator`가 GPU-visible 힙을 관리한다.

---

## Q10. `CSimpleConstantBufferPool` — 힙 타입

`CSimpleConstantBufferPool::Initialize()`에서 상수 버퍼 리소스를 생성할 때 사용하는 힙 타입은?

- A) `D3D12_HEAP_TYPE_DEFAULT`
- B) `D3D12_HEAP_TYPE_READBACK`
- C) `D3D12_HEAP_TYPE_UPLOAD`
- D) 커스텀 힙(Custom heap)

---

## Q11. `CSimpleConstantBufferPool` — CBV 디스크립터 힙 플래그

`CSimpleConstantBufferPool`이 내부에서 생성하는 디스크립터 힙(`m_pCBVHeap`)의 `Flags`는?

```cpp
D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
heapDesc.Flags = ???;
```

- A) `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE`
- B) `D3D12_DESCRIPTOR_HEAP_FLAG_NONE`
- C) `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE | D3D12_DESCRIPTOR_HEAP_FLAG_NONE`
- D) 플래그를 설정하지 않음 (자동으로 SHADER_VISIBLE)

---

## Q12. `Draw()` — Descriptor 복사 이유

`CBasicMeshObject::Draw()`에서 매 드로우 콜마다 CBV와 SRV를 `CopyDescriptorsSimple()`로 GPU-visible 힙에 복사하는 이유는?

- A) GPU-visible 힙에 직접 Write할 수 없기 때문에 CPU-visible 힙에서 만든 후 복사한다.
- B) CBV 리소스를 GPU-visible 힙에 생성할 수 없기 때문이다.
- C) 성능을 높이기 위해 batch copy를 사용하는 것이다.
- D) GPU-visible 힙은 용량이 작아서 매번 갱신해야 한다.

---

## Q13. `Draw()` — Descriptor Table 인덱스

`Draw()` 내부에서 `SetGraphicsRootDescriptorTable()`을 호출할 때 사용하는 Root Parameter 인덱스는?

```cpp
pCommandList->SetGraphicsRootDescriptorTable(???, gpuDescriptorTable);
```

- A) `1`
- B) `2`
- C) `0`
- D) `BASIC_MESH_DESCRIPTOR_INDEX_CBV` (= 0의 의미이지만 별도 enum 값)

---

## Q14. `Draw()` — DrawIndexedInstanced 파라미터

`DrawIndexedInstanced()`의 첫 번째 인수(IndexCountPerInstance)로 넘기는 값은?

- A) `3` (삼각형 1개)
- B) `4` (정점 4개)
- C) `6` (인덱스 6개 — 사각형 2개의 삼각형)
- D) `12` (정점 × 3)

---

## Q15. 카메라 — View Matrix 생성

`InitCamera()` 에서 View Matrix를 생성하는 함수는?

```cpp
m_matView = ???(eyePos, eyeDir, upDir);
```

- A) `XMMatrixLookAtLH`
- B) `XMMatrixLookToLH`
- C) `XMMatrixPerspectiveFovLH`
- D) `XMMatrixOrthoLH`

---

## Q16. 카메라 — FOV 각도

`InitCamera()` 에서 설정하는 시야각(fovY) 값은?

```cpp
float fovY = XM_PIDIV4;
```

- A) 180도
- B) 90도
- C) 60도
- D) 45도

---

## Q17. 카메라 — 원근 투영 near/far

`InitCamera()` 에서 설정되는 near plane과 far plane 값은?

- A) near = 0.01f, far = 100.0f
- B) near = 1.0f, far = 10000.0f
- C) near = 0.1f, far = 1000.0f
- D) near = 0.001f, far = 500.0f

---

## Q18. View/Proj Matrix — Transpose 이유

`GetViewProjMatrix()`에서 `XMMatrixTranspose()`를 적용하여 반환하는 이유는?

- A) DirectXMath는 Row-major이지만 HLSL cbuffer는 Column-major를 기본으로 읽기 때문이다.
- B) HLSL에서 Transpose된 행렬을 사용해야 최적화가 된다.
- C) View matrix는 항상 전치해야 역행렬이 된다.
- D) GPU에서 Row-major 행렬을 직접 읽을 수 없기 때문이다.

---

## Q19. Swap Chain — Present 동기화

`Present()` 에서 `m_SyncInterval = 0`으로 설정할 때 함께 사용하는 Present 플래그는?

- A) `DXGI_PRESENT_RESTART`
- B) `DXGI_PRESENT_DO_NOT_WAIT`
- C) `DXGI_PRESENT_ALLOW_TEARING`
- D) `DXGI_PRESENT_STEREO_PREFER_RIGHT`

---

## Q20. ConstantBufferPool과 DescriptorPool — Reset 시점

`Present()` 이후 `m_pConstantBufferPool->Reset()`과 `m_pDescriptorPool->Reset()`을 호출하는 이유는?

- A) GPU 메모리를 해제하기 위해서다.
- B) 다음 프레임에서 같은 메모리 영역을 처음부터 재사용하기 위해서다. Fence + WaitForFenceValue()로 GPU가 해당 프레임 처리를 완료한 것을 확인한 뒤 호출하므로 안전하다.
- C) 디스크립터 힙 자체를 재생성하기 위해서다.
- D) 상수 버퍼 리소스의 내용을 0으로 초기화하기 위해서다.

---

## Q21. `TEXTURE_HANDLE` 구조체

`typedef.h`에 정의된 `TEXTURE_HANDLE` 구조체가 갖는 멤버는?

- A) `ID3D12Resource* pTexResource` 하나만
- B) `ID3D12Resource* pTexResource`, `D3D12_GPU_DESCRIPTOR_HANDLE srv`
- C) `ID3D12Resource* pTexResource`, `D3D12_CPU_DESCRIPTOR_HANDLE srv`
- D) `ID3D12Resource* pTexResource`, `D3D12_CPU_DESCRIPTOR_HANDLE srv`, `D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv`

---

## Q22. `CSingleDescriptorAllocator` — 힙 플래그

`D3D12Renderer::Initialize()` 에서 `CSingleDescriptorAllocator`를 초기화할 때 넘기는 `D3D12_DESCRIPTOR_HEAP_FLAGS`는?

```cpp
m_pSingleDescriptorAllocator->Initialize(m_pD3DDevice, MAX_DESCRIPTOR_COUNT, ???);
```

- A) `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE`
- B) `D3D12_DESCRIPTOR_HEAP_FLAG_NONE`
- C) `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE | D3D12_DESCRIPTOR_HEAP_FLAG_NONE`
- D) 플래그 없이 기본값 사용

---

## Q23. 메시 정점 데이터

`CreateMesh()` 에서 생성하는 쿼드(사각형)의 정점 수와 인덱스 수는?

- A) 정점 3개, 인덱스 3개
- B) 정점 4개, 인덱스 6개
- C) 정점 6개, 인덱스 6개
- D) 정점 4개, 인덱스 4개

---

## Q24. 공유 리소스 참조 카운팅

`CBasicMeshObject`의 `m_pRootSignature`, `m_pPipelineState`가 `static`으로 선언되고, `m_dwInitRefCount`로 참조 카운트를 관리하는 이유는?

- A) 렌더러 인스턴스마다 별도의 PSO가 필요하기 때문이다.
- B) 여러 `CBasicMeshObject` 인스턴스가 동일한 Root Signature와 PSO를 공유하여 메모리와 생성 비용을 절약하기 위해서다.
- C) D3D12 API가 PSO를 static으로 강제하기 때문이다.
- D) `m_pRootSignature`는 스레드 안전성을 위해 static으로 선언된다.

---

## Q25. Depth-Stencil 초기 리소스 상태

`CreateDepthStencil()` 에서 깊이 버퍼 리소스를 `CreateCommittedResource()`로 생성할 때의 초기 상태(initialResourceState)는?

- A) `D3D12_RESOURCE_STATE_COMMON`
- B) `D3D12_RESOURCE_STATE_DEPTH_READ`
- C) `D3D12_RESOURCE_STATE_DEPTH_WRITE`
- D) `D3D12_RESOURCE_STATE_GENERIC_READ`

---

---

# 정답 및 해설

| # | 정답 | 해설 |
|---|------|------|
| Q1 | **B** | D3D12에서 깊이 버퍼 리소스 자체는 `DXGI_FORMAT_R32_TYPELESS`로 생성해야 한다. DSV 생성 시 `DXGI_FORMAT_D32_FLOAT`로 해석(view)한다. |
| Q2 | **C** | `D3D12_DEPTH_STENCIL_VIEW_DESC.Format = DXGI_FORMAT_D32_FLOAT`. Typeless 리소스를 DSV로 바인딩하려면 실제 depth 포맷을 지정해야 한다. |
| Q3 | **D** | `ClearDepthStencilView(..., 1.0f, ...)`. 1.0f는 원근 투영에서 가장 먼(far) 깊이 값으로, 매 프레임 초기화에 사용된다. |
| Q4 | **B** | `D3D12_COMPARISON_FUNC_LESS_EQUAL`. "새 픽셀의 depth ≤ 기존 depth"이면 통과. LESS와 비교해 같은 depth 값도 허용한다. |
| Q5 | **C** | `psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT`. PSO는 깊이 버퍼를 읽고 쓸 포맷을 명시해야 한다. |
| Q6 | **C** | `D3D12_CULL_MODE_NONE`. 뒷면 컬링을 끄면 쿼드를 어느 방향에서 봐도 렌더링된다. |
| Q7 | **C** | `ranges[2]`: `CBV(b0)`, `SRV(t0)`. 상수 버퍼와 텍스처 하나씩 묶어서 하나의 Descriptor Table로 구성한다. |
| Q8 | **C** | `rootParameters[1]`로 1개만 선언되며, `InitAsDescriptorTable(2, ranges, ...)`로 CBV+SRV를 하나의 Table로 묶는다. |
| Q9 | **B** | `CDescriptorPool`은 `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE` 힙을 사용하며 매 프레임 `Reset()`으로 재사용. `CSingleDescriptorAllocator`는 `FLAG_NONE`(CPU-only) 힙으로 텍스처 SRV 등 수명이 긴 디스크립터를 개별 관리한다. |
| Q10 | **C** | 상수 버퍼는 CPU에서 매 프레임 쓰므로 `D3D12_HEAP_TYPE_UPLOAD` 힙에 생성하고 `Map()`으로 CPU 포인터를 유지한다. |
| Q11 | **B** | `D3D12_DESCRIPTOR_HEAP_FLAG_NONE`. CBV 디스크립터는 CPU-only 힙에 저장되고, 렌더링 시 GPU-visible 힙(`CDescriptorPool`)으로 `CopyDescriptorsSimple()`을 통해 복사된다. |
| Q12 | **A** | D3D12에서 CPU(non-shader-visible) 힙에 있는 디스크립터를 GPU(shader-visible) 힙으로 복사해야 셰이더가 접근할 수 있다. CPU 측에서는 CPU 핸들에만 직접 Write할 수 있다. |
| Q13 | **C** | `SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)`. Root Parameter가 1개이므로 인덱스 0. |
| Q14 | **C** | `DrawIndexedInstanced(6, ...)`. 쿼드(사각형) = 삼각형 2개 = 인덱스 6개(0,1,2 / 0,2,3). |
| Q15 | **B** | `XMMatrixLookToLH(eyePos, eyeDir, upDir)`. 카메라 위치 + 방향 벡터로 View Matrix를 생성. (`LookAt`은 타겟 위치를 받는 함수다.) |
| Q16 | **D** | `XM_PIDIV4 = π/4 라디안 = 45도`. (XM_PI = 180°, XM_PIDIV2 = 90°, XM_PIDIV4 = 45°) |
| Q17 | **C** | `fNear = 0.1f`, `fFar = 1000.0f`. |
| Q18 | **A** | DirectXMath의 XMMATRIX는 Row-major 순서로 저장된다. 반면 HLSL의 `matrix`(cbuffer)는 Column-major로 해석하므로, CPU→GPU 전달 전에 `Transpose()`로 전치해야 올바른 행렬 연산이 된다. |
| Q19 | **C** | `DXGI_PRESENT_ALLOW_TEARING`. VSync를 끄고(`SyncInterval=0`) 화면 tear를 허용하는 플래그. Swap Chain 생성 시 `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`과 쌍으로 사용된다. |
| Q20 | **B** | `Fence()`·`WaitForFenceValue()`로 GPU 실행 완료를 보장한 뒤 `Reset()`을 호출하므로 CPU가 이미 GPU가 사용을 마친 메모리 영역을 안전하게 처음부터 재사용한다. |
| Q21 | **C** | `TEXTURE_HANDLE { ID3D12Resource* pTexResource; D3D12_CPU_DESCRIPTOR_HANDLE srv; }`. SRV는 CPU 핸들로 저장되고, Draw 시 GPU-visible 힙에 복사된다. |
| Q22 | **B** | `D3D12_DESCRIPTOR_HEAP_FLAG_NONE`. 텍스처 SRV를 위한 CPU-only 힙으로, 셰이더가 직접 접근할 수 없다. 렌더링 시 `CDescriptorPool`(GPU-visible)로 복사하여 사용한다. |
| Q23 | **B** | 정점 4개(`BasicVertex Vertices[4]`), 인덱스 6개(`WORD Indices[6] = {0,1,2, 0,2,3}`). |
| Q24 | **B** | `static` 멤버로 공유하여 동일한 Root Signature·PSO를 여러 인스턴스가 공유한다. `m_dwInitRefCount`가 0이 될 때만 실제로 해제한다. |
| Q25 | **C** | `D3D12_RESOURCE_STATE_DEPTH_WRITE`. 깊이 버퍼는 생성 즉시 쓰기 가능 상태로 만들어야 `ClearDepthStencilView()`와 깊이 테스트가 정상 동작한다. |
