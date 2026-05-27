# Chapter 16 — Q&A

---

## Q9. `pTexHandle->pSearchHandle`에 `HashTable::Insert()`의 반환값을 저장하는 이유는 무엇인가? 1번도 맞는가?

정답은 **② Delete() 시 파일명 해시를 재계산하지 않고 O(1)로 해당 노드를 바로 제거하기 위해** 이다.  
①은 틀리다. `Select()`는 `pSearchHandle`을 전혀 사용하지 않는다.

`Select()`의 시그니처는 다음과 같다:

```cpp
DWORD Select(void** ppOutItemList, DWORD dwMaxItemNum,
       const void* pKeyData,
       DWORD dwSize);
```

즉 검색은 어디까지나 **파일명 키(`pKeyData`)** 로 한다. `pSearchHandle` 없이도 언제든 호출 가능하다.

반면 `Delete()`는 이렇게 동작한다:

```cpp
void Delete(const void* pSearchHandle)
{
  VB_BUCKET* pBucket = (VB_BUCKET*)pSearchHandle;
  BUCKET_DESC* pBucketDesc = pBucket->pBucketDesc;

  UnLinkFromLinkedList(&pBucketDesc->pBucketLinkHead, &pBucketDesc->pBucketLinkTail, &pBucket->sortLink);
  free(pBucket);
}
```

여기서 `pSearchHandle`은 실제로 `VB_BUCKET*` 포인터다.  
이 포인터를 저장해 두면 삭제할 때:

- 파일명 다시 해시 계산
- 버킷 찾기
- 버킷 링크드리스트 순회

과정을 생략하고 **노드 직접 접근 → unlink → free** 로 끝낼 수 있다.

---

## Q8. 우리 코드에서는 파일명만 같으면 같은 hash고, 같은 texture인가?

실질적으로는 **같은 파일명 문자열이면 같은 텍스처로 취급한다**고 보면 된다.

정확히는 2단계다:

```cpp
// 1) CreateKey() : 파일명 바이트 합산 % 버킷 수
DWORD dwIndex = CreateKey(pKeyData, dwSize, m_dwMaxBucketNum);

// 2) 같은 버킷 안에서 memcmp()로 파일명 완전 비교
if (memcmp(pBucket->pKeyData, pKeyData, dwSize) == 0)
{
  // 같은 파일명 = 같은 텍스처로 판정
  return pBucket->pItem; // TEXTURE_HANDLE*
}
```

즉 이 코드에서 “같은 텍스처”의 정의는:

> **파일명 문자열의 바이트가 완전히 동일하면 같은 텍스처**

GPU 리소스 내용 자체를 비교하지는 않는다. 실행 중 파일 내용이 디스크에서 바뀌어도,
같은 파일명으로 다시 요청하면 기존 `TEXTURE_HANDLE`을 재사용한다.

주의할 점은 문자열이 완전히 같아야 한다는 것이다:

```cpp
L"stone.dds"      != L"Stone.dds"
L"tex/stone.dds" != L"tex\\stone.dds"
```

즉 같은 파일을 가리키더라도 경로 표기가 다르면 캐시 MISS가 날 수 있다.

---

## Q7. 파일명만 같으면 `memcmp()` 결과도 항상 같은가?

맞다. **같은 문자열 바이트 배열이 들어오면 `memcmp()` 결과는 0** 이고, 그게 캐시 HIT 조건이다.

`Select()` 내부는 이런 구조다:

```cpp
if (pBucket->dwSize != dwSize)
  goto lb_next;

if (memcmp(pBucket->pKeyData, pKeyData, dwSize))
  goto lb_next;   // 0 아님 = 다른 파일명

// 여기 도달 = memcmp == 0 = 같은 파일명
ppOutItemList[dwSelectedItemNum] = (void*)pBucket->pItem;
```

`memcmp()` 반환값 의미:

- `0` : 두 바이트 배열이 완전히 동일
- `0 아님` : 한 바이트라도 다름

따라서 이 구현은 **호출자가 항상 동일한 형태의 경로 문자열을 넘긴다**는 전제를 둔다.

---

## Q6. `memcmp()`는 정확히 어떤 바이너리를 비교하는가?

비교 대상은 **파일명 `WCHAR` 문자열의 raw 바이트 배열**이다.

`CreateTextureFromFile()`에서 키를 이렇게 넘긴다:

```cpp
DWORD dwFileNameLen = (DWORD)wcslen(wchFileName);
DWORD dwKeySize = dwFileNameLen * sizeof(WCHAR);

m_pHashTable->Select((void**)&pTexHandle, 1, wchFileName, dwKeySize);
```

그리고 `Insert()` 때 그 바이트를 버킷 노드에 복사 저장한다:

```cpp
memcpy(pBucket->pKeyData, pKeyData, dwSize);
```

따라서 `Select()`의 `memcmp()`는 다음 둘을 비교한다:

```cpp
memcmp(pBucket->pKeyData, pKeyData, dwSize)
```

- `pBucket->pKeyData` : 예전에 저장된 파일명 문자열의 바이트
- `pKeyData` : 지금 조회 중인 파일명 문자열의 바이트

예를 들어 `L"stone.dds"` 라면 실제 메모리 비교는 대략 이런 식이다:

```text
's',0,'t',0,'o',0,'n',0,'e',0,'.',0,'d',0,'d',0,'s',0
```

즉 텍스처 픽셀 데이터나 GPU 버퍼를 비교하는 것이 아니라,
**UTF-16LE 형태의 파일명 문자열 바이트**를 비교하는 것이다.

---

## Q5. Root descriptor table은 shader-visible heap의 어느 슬롯을 가리키며, sampler는 왜 같이 안 들어가는가?

핵심은 **루트 파라미터가 “힙 전체”를 뜻하는 것이 아니라, shader-visible heap 안의 특정 시작 슬롯 주소를 뜻한다**는 점이다.

Chapter 16의 `BasicMeshObject` 루트 시그니처는 이렇게 되어 있다:

```cpp
CD3DX12_DESCRIPTOR_RANGE rangesPerObj[1] = {};
rangesPerObj[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0

CD3DX12_DESCRIPTOR_RANGE rangesPerTriGroup[1] = {};
rangesPerTriGroup[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

rootParameters[0].InitAsDescriptorTable(_countof(rangesPerObj), rangesPerObj, D3D12_SHADER_VISIBILITY_ALL);
rootParameters[1].InitAsDescriptorTable(_countof(rangesPerTriGroup), rangesPerTriGroup, D3D12_SHADER_VISIBILITY_ALL);
```

의미는 다음과 같다:

- `root parameter 0` : **CBV 1개짜리 테이블**을 기대한다. 셰이더 레지스터 기준 `b0`.
- `root parameter 1` : **SRV 1개짜리 테이블**을 기대한다. 셰이더 레지스터 기준 `t0`.

드로우 직전 shader-visible heap(`DescriptorPool`)에는 이런 식으로 슬롯을 채운다:

```cpp
// 슬롯[0] = 오브젝트용 CBV
CopyDescriptorsSimple(1, Dest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

// 슬롯[1], [2], [3] ... = TriGroup별 SRV
CopyDescriptorsSimple(1, Dest, pTexHandle->srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
```

예를 들어 TriGroup이 3개면 실제 배치는:

```
slot[0] = CBV for b0
slot[1] = SRV for tri-group 0 (t0)
slot[2] = SRV for tri-group 1 (t0)
slot[3] = SRV for tri-group 2 (t0)
```

그 다음 바인딩은 이렇게 된다:

```cpp
SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);                  // slot[0] 시작 → b0
SetGraphicsRootDescriptorTable(1, gpuDescriptorTable + 1);              // slot[1] 시작 → t0
SetGraphicsRootDescriptorTable(1, gpuDescriptorTable + 2);              // 다음 draw에서 tri-group 1의 t0
SetGraphicsRootDescriptorTable(1, gpuDescriptorTable + 3);              // 다음 draw에서 tri-group 2의 t0
```

즉 같은 shader-visible heap 안에 **CBV와 SRV는 섞어 넣을 수 있다.**  
하지만 **Sampler는 안 된다.** 이유는 sampler가 다른 heap 타입이기 때문이다.

### 왜 sampler는 같은 슬롯에 못 넣는가?

D3D12의 descriptor heap 타입은 분리되어 있다:

- `D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV` : CBV / SRV / UAV 전용
- `D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER` : Sampler 전용

따라서 다음은 가능하다:

```
CBV_SRV_UAV heap:
  [0] CBV
  [1] SRV
  [2] UAV
  [3] SRV
```

다음은 불가능하다:

```
CBV_SRV_UAV heap:
  [0] CBV
  [1] Sampler   // 불가
```

Sampler를 쓰려면 별도의 `SAMPLER` 타입 shader-visible heap을 바인딩해야 한다.  
다만 Chapter 16의 `BasicMeshObject`는 sampler를 heap에서 동적으로 올리지 않고,
루트 시그니처 안의 **static sampler** 로 넣고 있다:

```cpp
rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler, ...);
```

그래서 이 챕터에서는 `SetDescriptorHeaps()`에 sampler heap을 따로 넘기지 않아도 된다.

### 정리

- `SetDescriptorHeaps()`의 제한은 **힙 개수 제한**이다.
- `CBV_SRV_UAV` shader-visible heap 1개 안에는 **CBV / SRV / UAV를 섞어서** 넣을 수 있다.
- Sampler는 **다른 heap 타입**이라 그 슬롯들 사이에 섞어 넣을 수 없다.
- Chapter 16은 sampler를 static sampler로 처리하므로, 실제 런타임 descriptor table에는 CBV와 SRV만 올리고 있다.

---

## Q4. `TEXTURE_HANDLE*`로 접근할 수 있는 정보는 어떤 것들인가?

```cpp
struct TEXTURE_HANDLE
{
    ID3D12Resource*             pTexResource;   // GPU 텍스처 리소스 (실제 픽셀 데이터)
    ID3D12Resource*             pUploadBuffer;  // CPU→GPU 업로드 임시 버퍼 (Dynamic만 사용)
    D3D12_CPU_DESCRIPTOR_HANDLE srv;            // SRV 슬롯 주소 (CopyDescriptorsSimple 원본)
    BOOL                        bUpdated;       // 이번 프레임에 픽셀 데이터 갱신 여부
    BOOL                        bFromFile;      // 파일 기반 텍스처 여부 (= 캐싱 대상)
    DWORD                       dwRefCount;     // 참조 카운트 (0이 되어야 GPU 리소스 해제)
    void*                       pSearchHandle;  // HashTable 노드 포인터 (O(1) 삭제용)
    SORT_LINK                   Link;           // TextureManager 전체 텍스처 연결 리스트
};
```

단순한 텍스처 버퍼 포인터가 아니라, 렌더링·메모리 관리·캐시 관리·리스트 연결 전부를 포함하는 중앙 관리 구조체다.

| 필드 | 역할 |
|------|------|
| `pTexResource` | GPU 메모리의 실제 텍스처 — 항상 존재 |
| `pUploadBuffer` | Dynamic 텍스처만 가짐, 파일 텍스처는 nullptr |
| `srv` | `CopyDescriptorsSimple()`의 복사 **원본** 주소 |
| `bFromFile` | `DeleteTexture()`에서 HashTable 조회 여부 분기 |
| `dwRefCount` | 같은 파일 텍스처를 몇 개 오브젝트가 공유하는지 |
| `pSearchHandle` | `HashTable::Delete(pSearchHandle)` 로 O(1) 제거 |
| `Link` | `m_pTexLinkHead/Tail` 리스트에 연결 → `Cleanup()` 시 전체 순회 |

---

## Q3. 같은 파일 텍스처가 이미 존재하는지 어떻게 판단하는가 — 포인터 주소인가, 파일명인가?

**A.** **파일명 문자열(`memcmp`)로 판단**하고, 매칭되면 저장해둔 `TEXTURE_HANDLE*` 포인터를 꺼낸다.

### HashTable::Select() 내부

```cpp
// CreateKey() 로 버킷 인덱스 계산 (파일명 바이트 합산 % 버킷 수)
DWORD dwIndex = CreateKey(pKeyData, dwSize, m_dwMaxBucketNum);

// 해당 버킷의 링크드리스트 순회
while (pCur)
{
    if (pBucket->dwSize != dwSize) goto lb_next;
    if (memcmp(pBucket->pKeyData, pKeyData, dwSize)) goto lb_next; // ← 파일명 문자열 비교

    ppOutItemList[dwSelectedItemNum] = (void*)pBucket->pItem;       // ← TEXTURE_HANDLE* 반환
    dwSelectedItemNum++;
    ...
}
```

### CreateTextureFromFile() 호출 흐름

```
Insert 때:
  pBucket->pKeyData = "stone.dds" 복사 저장  (key = 파일명 WCHAR 문자열)
  pBucket->pItem    = pTexHandle             (value = TEXTURE_HANDLE*)

Select 때:
  CreateKey("stone.dds") → 버킷 인덱스
  버킷 내 리스트 순회 → memcmp()로 파일명 완전 비교
  일치하면 pBucket->pItem(= TEXTURE_HANDLE*) 반환
```

포인터 주소 비교가 아니다. 다른 파일명이 같은 버킷 인덱스(해시 충돌)에 들어갈 수 있으므로
버킷을 찾은 뒤에도 반드시 `memcmp()`로 키 내용을 완전 비교해야 한다.

---

## Q2. CDescriptorPool, CConstantBufferManager, CSingleDescriptorAllocator 각각의 역할

### 결론 먼저

```
CDescriptorPool[i]          → 매 드로우콜에 GPU가 읽을 CBV+SRV 슬롯을 프레임마다 임시 할당
CConstantBufferManager[i]   → 행렬 등 상수 데이터를 담는 GPU 업로드 버퍼 풀 (CBV 전용)
CSingleDescriptorAllocator  → 텍스처 SRV를 영구 보관하는 창고 (GPU에서 직접 못 읽음)
```

---

### CDescriptorPool[i] — 프레임당 임시 Shader-Visible 힙

```cpp
m_ppDescriptorPool[i]->Initialize(
    m_pD3DDevice,
    MAX_DRAW_COUNT_PER_FRAME * CBasicMeshObject::MAX_DESCRIPTOR_COUNT_FOR_DRAW
    // = 256 * 9 = 2304 슬롯
);
```

- `D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE` → GPU가 직접 읽는 힙
- `AllocDescriptorTable()` 로 드로우콜마다 연속 슬롯을 bump 할당
- 프레임 끝에 `Reset()` → 포인터만 0으로 초기화 (실제 해제 없음)
- 한 오브젝트의 드로우콜당 레이아웃:

```
| CBV (행렬) | SRV (tex0) | SRV (tex1) | ... | SRV (tex7) |
  [0]          [1]          [2]                 [8]
  1슬롯        triGroup 수만큼 SRV 슬롯
```

DescriptorPool이 2개(`[0]`, `[1]`)인 이유는 스왑체인처럼 더블버퍼링(MAX_PENDING_FRAME_COUNT = 2)으로
이전 프레임 GPU 작업이 끝나기 전에 다음 프레임 할당을 시작하기 위해서다.

---

### CConstantBufferManager[i] — 상수버퍼 풀

- `CONSTANT_BUFFER_TYPE_DEFAULT` (matWorld + matView + matProj) 풀
- `CONSTANT_BUFFER_TYPE_SPRITE` (스크린 좌표, 스케일 등) 풀
- 내부적으로 GPU Upload 힙에 미리 할당된 상수버퍼 배열을 가짐
- `Alloc()` 호출 → `CB_CONTAINER` 반환:

```cpp
struct CB_CONTAINER
{
    D3D12_CPU_DESCRIPTOR_HANDLE CBVHandle;     // non-shader-visible CBV 핸들
    D3D12_GPU_VIRTUAL_ADDRESS   pGPUMemAddr;   // 사용 안 함 (CBVHandle로 대체)
    UINT8*                      pSystemMemAddr; // CPU에서 행렬 값을 memcpy로 쓰는 주소
};
```

- 드로우콜 직전 `pSystemMemAddr`에 matWorld 등을 직접 쓰면 GPU가 바로 읽음
- `Reset()` 으로 프레임당 초기화

> **SRV는 상수버퍼가 아니다.**  
> CBV(Constant Buffer View) = 행렬·파라미터 등 셰이더 상수  
> SRV(Shader Resource View) = 텍스처 샘플링용. ConstantBufferManager와 무관하고 TextureManager가 관리한다.

---

### CSingleDescriptorAllocator — 텍스처 SRV 영구 창고

```cpp
m_pSingleDescriptorAllocator->Initialize(
    m_pD3DDevice,
    MAX_DESCRIPTOR_COUNT,          // 4096 슬롯
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE // ← shader-visible 아님
);
```

- `D3D12_DESCRIPTOR_HEAP_FLAG_NONE` → GPU가 직접 읽지 **못하는** 힙
- 텍스처가 로드될 때 SRV 슬롯 1개를 `AllocDescriptorHandle()`로 할당 → `TEXTURE_HANDLE::srv`에 저장
- 텍스처가 삭제될 때 `FreeDescriptorHandle()` 로 슬롯 반환 (IndexCreator 기반 개별 alloc/free)
- 프레임마다 Reset 하지 않음 — 텍스처 생존 기간 동안 영구 유지

---

### 세 가지가 함께 동작하는 드로우콜 흐름

```
[텍스처 로드 시 1회]
SingleDescriptorAllocator::Alloc() → TEXTURE_HANDLE::srv (non-visible, 영구)

[매 드로우콜]
ConstantBufferManager::Alloc()
  → CB_CONTAINER::pSystemMemAddr 에 matWorld 등 CPU write

DescriptorPool::AllocDescriptorTable()
  → shader-visible 힙에 연속 슬롯 확보

CopyDescriptorsSimple(CBVHandle → DescriptorPool 슬롯[0])  ← CBV 복사
CopyDescriptorsSimple(srv      → DescriptorPool 슬롯[1])  ← SRV 복사 (non-visible → visible)

SetGraphicsRootDescriptorTable(DescriptorPool GPU 핸들)
DrawIndexedInstanced()
```

`CopyDescriptorsSimple()`이 핵심이다.  
GPU는 shader-visible 힙만 읽을 수 있으므로, non-visible 창고(SingleDescriptorAllocator)에서
매 프레임 DescriptorPool(shader-visible)로 필요한 것만 복사해서 사용한다.

---

## Q1. `CreateTextureFromFile`일 때만 캐시 HIT/MISS 판단해서 해시 테이블을 사용하나?

**A.** 맞다. 해시 테이블 조회는 `CreateTextureFromFile()`에서만 사용한다.

이유는 텍스처 종류별 특성이 다르기 때문이다:

| 생성 함수 | 해시 테이블 사용 | 이유 |
|-----------|:--------------:|------|
| `CreateTextureFromFile()` | **Yes** | 파일명이 고정된 키 — 동일 파일을 여러 오브젝트가 공유 가능 |
| `CreateDynamicTexture()` | **No** | CPU에서 매 프레임 내용을 동적으로 쓰는 텍스처 — 공유 불가 |
| `CreateImmutableTexture()` | **No** | 런타임에 생성된 이미지(타일 등) — 고정 키가 없음 |

`CreateTextureFromFile()`의 캐시 분기:

```cpp
TEXTURE_HANDLE* CTextureManager::CreateTextureFromFile(const WCHAR* wchFileName)
{
    DWORD dwKeySize = (DWORD)wcslen(wchFileName) * sizeof(WCHAR);

    if (m_pHashTable->Select((void**)&pTexHandle, 1, wchFileName, dwKeySize))
    {
        // 캐시 HIT: GPU 리소스 생성 없이 dwRefCount++ 후 반환
        pTexHandle->dwRefCount++;
    }
    else
    {
        // 캐시 MISS: GPU 리소스 생성 + HashTable::Insert()
        // bFromFile = TRUE, pSearchHandle 저장
    }
}
```

`CreateDynamicTexture()`와 `CreateImmutableTexture()`는 해시 테이블 조회 코드 자체가 없고,
`bFromFile`도 `FALSE`(기본값), `pSearchHandle`도 `nullptr`로 남는다.

`FreeTextureHandle()` 안에서 `pSearchHandle != nullptr` 조건으로 파일 기반 텍스처만
`HashTable::Delete()`를 호출해 캐시에서 제거한다.
