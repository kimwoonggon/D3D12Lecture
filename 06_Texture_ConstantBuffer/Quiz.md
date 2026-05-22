# Chapter 06 - Texture + Constant Buffer 복습 퀴즈

> 답은 파일 맨 아래에 있습니다. 먼저 직접 풀어보세요.

---

## [개념 이해]

**Q1.**  
Chapter 05에서 Chapter 06으로 넘어올 때 Descriptor Table의 구조가 바뀌었다.  
Chapter 05와 Chapter 06의 Descriptor Table 슬롯 구성을 각각 써라.

---

**Q2.**  
`m_pConstantBuffer`와 `m_pSysConstBufferDefault`는 둘 다 같은 메모리를 가리킨다.  
각각의 타입과 역할을 설명하고, 둘의 관계를 만들어주는 함수 이름을 써라.

---

**Q3.**  
아래 코드에서 `Map()`을 호출한 뒤 `Unmap()`을 호출하지 않는다.  
이것이 왜 가능하며, 어떤 상황에서 문제가 생길 수 있는가?

```cpp
m_pConstantBuffer->Map(0, &readRange,
    reinterpret_cast<void**>(&m_pSysConstBufferDefault));
// Unmap 없음 - 앱 종료까지 유지
```

---

**Q4.**  
Root Signature는 언제 새로 만들어야 하는가?  
아래 두 시나리오 중 Root Signature를 새로 만들어야 하는 경우를 고르고, 이유를 설명하라.

- **(A)** 오브젝트 A는 체크무늬 텍스처, 오브젝트 B는 돌 텍스처를 사용. 둘 다 CBV 1개 + SRV 1개 구조.
- **(B)** 오브젝트 C는 CBV 1개 + SRV 2개(텍스처 2장)를 셰이더에서 사용.

---

**Q5.**  
`SetDescriptorHeaps(1, &m_pDescritorHeap)` 에 힙을 1개만 전달했는데,  
CBV와 SRV가 둘 다 셰이더에 전달되는 이유를 설명하라.

---

**Q6.**  
`m_pRootSignature`는 `static` 멤버로 선언되어 있다.  
`CBasicMeshObject` 인스턴스를 3개 생성하면 `m_pRootSignature`는 몇 개 존재하는가?  
그리고 `m_dwInitRefCount`는 이 시점에 얼마가 되는가?

---

## [코드 분석]

**Q7.**  
아래 코드에서 `constantBufferSize`를 256바이트 정렬하는 이유는 무엇인가?

```cpp
const UINT constantBufferSize =
    (UINT)AlignConstantBufferSize(sizeof(CONSTANT_BUFFER_DEFAULT));
```

`CONSTANT_BUFFER_DEFAULT`의 실제 크기는 `sizeof(XMFLOAT4) = 16바이트`인데,  
왜 그대로 사용하면 안 되는가?

---

**Q8.**  
`Draw()` 함수에서 아래 순서로 코드가 작성되어 있다.  
각 줄이 하는 일을 한 문장으로 설명하라.

```cpp
// (1)
m_pSysConstBufferDefault->offset.x = pPos->x;

// (2)
pCommandList->SetGraphicsRootSignature(m_pRootSignature);

// (3)
pCommandList->SetDescriptorHeaps(1, &m_pDescritorHeap);

// (4)
pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

// (5)
pCommandList->DrawInstanced(3, 1, 0, 0);
```

---

**Q9.**  
`CreateTexture()`에서 CPU가 이미지 데이터를 Upload Buffer에 쓸 때 아래처럼 작성한다.

```cpp
for (UINT y = 0; y < Height; y++) {
    memcpy(pDest, pSrc, Width * 4);
    pSrc  += (Width * 4);
    pDest += Footprint.Footprint.RowPitch;  // 256
}
```

`Width * 4`(= 64바이트)만 쓰고 `RowPitch`(= 256바이트) 간격으로 이동하는 이유는?  
나머지 192바이트는 어떻게 되는가?

---

**Q10.**  
`UpdateTextureForWrite()` 에서 `ResourceBarrier`를 2번 호출한다.  
각각 어떤 상태로 전환하며, 왜 이 전환이 필요한지 설명하라.

```cpp
// 첫 번째 Barrier
ResourceBarrier(Transition(pDest,
    D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_COPY_DEST));

// ... CopyTextureRegion ...

// 두 번째 Barrier
ResourceBarrier(Transition(pDest,
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
```

---

## [심화]

**Q11.**  
`CopyTextureRegion()`에서 목적지(Default Heap)는 `SUBRESOURCE_INDEX`로,  
출처(Upload Buffer)는 `PLACED_FOOTPRINT`로 타입을 다르게 지정한다.  
왜 같은 타입으로 통일하지 않는가?

---

**Q12.**  
삼각형의 꼭짓점 X 좌표가 아래와 같을 때, offset의 범위를 계산하라.  
화면 NDC 범위는 -1.0 ~ +1.0이다.

```
꼭짓점:
  top:          x =  0.0
  bottom-right: x = +0.25  ← 가장 오른쪽
  bottom-left:  x = -0.25  ← 가장 왼쪽
```

삼각형이 화면 밖으로 나가지 않으려면 offset의 범위는?

---

**Q13.**  
`Fence()` + `WaitForFenceValue()`의 역할을 설명하고,  
`UpdateTextureForWrite()`에서 이 두 함수를 호출하지 않으면  
어떤 문제가 발생하는지 서술하라.

---

**Q14.**  
`GetCopyableFootprints()`에 `Rows[]`와 `RowSize[]`를 출력 버퍼로 전달하지만,  
이 코드에서는 실제로 사용하지 않는다.  
그렇다면 이 값들이 실제로 필요한 상황은 언제인가? (예시 포함)

---

**Q15.**  
아래는 `InitCommonResources()`의 코드다.

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

오브젝트 인스턴스를 5개 생성한 뒤 3개를 소멸시켰다.  
`CleanupSharedResources()`를 보면 `ref_count == 0`일 때만 Root Signature를 해제한다.  
이 시점에 Root Signature는 살아있는가, 해제되었는가? 이유를 설명하라.

---

---

# 정답

---

**A1.**
- Chapter 05: `| [0] SRV(TEX) |` — 1개
- Chapter 06: `| [0] CBV | [1] SRV(TEX) |` — 2개

---

**A2.**
- `m_pConstantBuffer` : `ID3D12Resource*` — GPU Upload 힙에 할당된 D3D12 리소스 핸들. GPU 주소(`GetGPUVirtualAddress()`) 조회와 `Release()` 대상.
- `m_pSysConstBufferDefault` : `CONSTANT_BUFFER_DEFAULT*` — CPU에서 직접 값을 쓰는 포인터.
- 둘의 관계를 만드는 함수: `m_pConstantBuffer->Map()`

---

**A3.**
Upload Heap(`D3D12_HEAP_TYPE_UPLOAD`)은 CPU와 GPU가 동시에 접근 가능한 메모리이므로 Map 상태를 유지해도 된다. D3D12 사양상 Upload Heap은 앱 종료까지 Map 유지가 허용된다.  
문제가 생기는 상황: Default Heap이나 Readback Heap에서 장기 Map 유지, 또는 GPU가 읽는 도중 CPU가 값을 변경하면 레이스 컨디션 발생 가능.

---

**A4.**
**(B)** 를 새로 만들어야 한다.  
Root Signature는 "슬롯 구조 규격"이기 때문에, 자원의 **종류나 개수**가 달라지면 새로운 규격이 필요하다. (A)는 텍스처 내용만 다를 뿐 CBV 1개 + SRV 1개 구조가 동일하므로 Root Signature를 공유할 수 있다.

---

**A5.**
`m_pDescritorHeap`은 슬롯 2개짜리 힙이고, `[0] CBV`, `[1] SRV`가 이미 등록되어 있다.  
`SetGraphicsRootDescriptorTable(0, gpuHandle)`로 힙의 시작 주소를 전달하면, GPU는 Root Signature에 정의된 순서(`ranges[0]=CBV`, `ranges[1]=SRV`)대로 연속 슬롯을 읽는다. 힙 하나에 두 자원이 연속 배치되어 있으므로 충분하다.

---

**A6.**
`m_pRootSignature`는 `static`이므로 **1개**만 존재한다.  
인스턴스 3개 생성 후 `m_dwInitRefCount = 3`.  
(두 번째, 세 번째 생성 시 `InitCommonResources()`가 `goto lb_true`로 생성을 건너뛰고 카운트만 증가)

---

**A7.**
D3D12 하드웨어 사양상 Constant Buffer의 크기는 **256바이트 배수**여야 한다.  
`CONSTANT_BUFFER_DEFAULT`는 16바이트지만 그대로 사용하면 GPU 드라이버가 거부하거나 미정의 동작이 발생한다. `AlignConstantBufferSize(16)` → 256바이트로 정렬 후 사용.

---

**A8.**
1. CPU에서 Upload 힙에 직접 오프셋 값을 기록한다.
2. 커맨드 리스트에 사용할 Root Signature를 설정한다.
3. GPU에게 이번 드로우콜에서 사용할 Descriptor Heap을 장착한다.
4. Root Parameter 0번에 힙의 시작 GPU 주소를 바인딩한다.
5. 버텍스 3개짜리 삼각형을 1회 그린다.

---

**A9.**
GPU는 각 행을 256바이트 경계(RowPitch)에 맞춰 저장해야 한다.  
실제 데이터(64바이트)만 쓰고 나머지 192바이트는 패딩으로 비워둔다.  
`memcpy`로 데이터만 채우고 `pDest += RowPitch(256)`으로 다음 행 시작점으로 이동하므로, 패딩 영역은 값이 무엇이든 GPU가 읽지 않는다.

---

**A10.**
- 첫 번째: `SHADER_RESOURCE → COPY_DEST`  
  GPU에게 "이 텍스처는 지금부터 셰이더 읽기 금지, 복사 목적지로 사용한다"고 알림. 없으면 셰이더와 복사 작업이 동시에 접근하는 충돌 발생.

- 두 번째: `COPY_DEST → SHADER_RESOURCE`  
  복사 완료 후 다시 셰이더가 읽을 수 있게 복구. 없으면 이후 렌더링에서 GPU가 이 텍스처를 SRV로 읽지 못한다.

---

**A11.**
- Default Heap 텍스처는 GPU가 내부적으로 타일링/스왑 최적화된 배치로 저장하며, CPU는 그 내부 구조를 알 수 없다. 따라서 "MipLevel 번호(SUBRESOURCE_INDEX)"라는 논리적 주소만 사용할 수 있다.
- Upload Buffer는 단순한 1차원 바이트 배열이므로 2D 레이아웃 정보(오프셋, RowPitch, 크기)를 직접 명시(PLACED_FOOTPRINT)해야 한다.
- 두 메모리의 저장 방식이 근본적으로 달라서 같은 타입으로 통일할 수 없다.

---

**A12.**
$$-0.25 + \text{offset} \geq -1.0 \implies \text{offset} \geq -0.75$$
$$+0.25 + \text{offset} \leq +1.0 \implies \text{offset} \leq +0.75$$

따라서 offset 범위: **-0.75 ~ +0.75**

---

**A13.**
`Fence()`는 GPU에게 "이 커맨드가 완료되면 펜스 값을 올려라"는 신호를 보낸다.  
`WaitForFenceValue()`는 CPU가 그 펜스 값이 올라올 때까지 블로킹 대기한다.  

호출하지 않으면: GPU가 아직 Upload Buffer에서 텍스처로 복사하는 도중에 CPU가 `pUploadBuffer->Release()`를 호출하게 되어, GPU가 해제된 메모리에 접근하는 **크래시 또는 메모리 오염**이 발생한다.

---

**A14.**
BC1, BC3 같은 블록 압축 포맷을 사용할 때 `Rows`가 필요하다.  
이 경우 실제 행 수 = `Height / 4`이므로, `Height` 그대로 루프를 돌면 범위를 초과한다.  

예시:
```cpp
// 압축 포맷에서는 Rows[i]를 사용해야 정확함
for (UINT y = 0; y < Rows[i]; y++) {
    memcpy(pDest, pSrc, (SIZE_T)RowSize[i]);
    pSrc  += RowSize[i];
    pDest += Footprint[i].Footprint.RowPitch;
}
```

---

**A15.**
살아있다.  
인스턴스 5개 생성 → `m_dwInitRefCount = 5`.  
3개 소멸 → `CleanupSharedResources()` 3번 호출 → `m_dwInitRefCount = 2`.  
`ref_count == 0`이 아니므로 Root Signature는 해제되지 않는다.  
나머지 2개 인스턴스가 살아있는 동안 Root Signature도 유지된다.
