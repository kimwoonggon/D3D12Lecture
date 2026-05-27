# Chapter 16 — TextureManager 퀴즈 (객관식)

> 각 문제의 **정답 보기 ▼** 를 클릭하면 정답이 나옵니다.

---

## Q1. Chapter 16에서 `CTextureManager`를 별도 클래스로 분리한 가장 핵심적인 이유는?

① 코드 줄 수를 줄이기 위해  
② 동일 파일 텍스처의 중복 GPU 리소스 생성을 막고, 참조 카운팅으로 안전한 해제를 하기 위해  
③ `CD3D12Renderer`의 클래스 크기가 너무 커서 컴파일이 느려지기 때문에  
④ D3D12 API가 텍스처 관리 클래스를 강제하기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

Chapter 15에서는 같은 파일을 두 번 `CreateTextureFromFile()`하면 GPU 리소스가 두 개 생성되었다. Chapter 16의 `CTextureManager`는 `CHashTable`로 파일명을 키로 캐싱하여, 이미 로드된 텍스처는 `dwRefCount`만 증가시키고 반환한다. 삭제 시에도 `dwRefCount`가 0이 되어야만 실제 GPU 리소스를 해제한다.

</details></td></tr></table>

---

## Q2. `CTextureManager::Initialize(this, 1024 / 16, 1024)`에서 첫 번째 숫자(64)와 두 번째 숫자(1024)는 각각 무엇인가?

① 텍스처 최대 해상도, 최대 밉맵 레벨  
② 해시 테이블의 최대 버킷 수, 최대 아이템(파일) 수  
③ 텍스처당 최대 SRV 수, 전체 최대 디스크립터 수  
④ 프레임당 최대 업로드 크기, 최대 다이나믹 텍스처 수  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
m_pHashTable->Initialize(dwMaxBucketNum, _MAX_PATH * sizeof(WCHAR), dwMaxFileNum);
//                       ↑ 버킷 64개                               ↑ 최대 1024개 파일
```

버킷은 해시 충돌 시 체이닝하는 연결 리스트 배열의 크기다. 파일 경로를 해시하여 0~63 중 하나의 버킷에 배치한다.

</details></td></tr></table>

---

## Q3. `TEXTURE_HANDLE`에 Chapter 16에서 새로 추가된 필드가 아닌 것은?

① `bFromFile`  
② `dwRefCount`  
③ `pSearchHandle`  
④ `pUploadBuffer`  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ④**

`pUploadBuffer`는 Chapter 15부터 이미 존재했다 (동적 텍스처의 CPU→GPU 업로드용).  
Chapter 16에서 추가된 필드는 다음 세 가지다:
- `bFromFile` : 파일 기반 텍스처 식별
- `dwRefCount` : 참조 카운트
- `pSearchHandle` : 해시 테이블에서의 위치 (삭제 시 O(1) 제거용)

</details></td></tr></table>

---

## Q4. `CreateTextureFromFile()`을 동일 파일명으로 3번 호출하면 `dwRefCount`는 얼마가 되는가?

① 0  
② 1  
③ 3  
④ GPU 리소스가 3개 생성된다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

첫 번째 호출: `HashTable` 미스 → GPU 리소스 생성, `dwRefCount = 1`  
두 번째 호출: `HashTable` 히트 → `dwRefCount++` → 2  
세 번째 호출: `HashTable` 히트 → `dwRefCount++` → 3  

GPU 리소스(`ID3D12Resource`)는 **1개만** 생성된다. 세 호출 모두 같은 `TEXTURE_HANDLE` 포인터를 반환한다.

</details></td></tr></table>

---

## Q5. `FreeTextureHandle()`에서 `dwRefCount`가 0이 아닐 때 실제로 일어나는 일은?

① `pTexResource->Release()`만 호출한다  
② 아무것도 하지 않고 남은 refCount를 반환한다  
③ `__debugbreak()`로 오류를 알린다  
④ `HashTable::Delete()`만 호출한다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
DWORD ref_count = --pTexHandle->dwRefCount;
if (!ref_count)
{
    // GPU 리소스 해제, HashTable 제거, delete pTexHandle
}
// ref_count > 0이면 여기서 그냥 반환
return ref_count;
```

다른 오브젝트가 아직 해당 텍스처를 사용 중이므로 GPU 리소스는 그대로 유지된다.

</details></td></tr></table>

---

## Q6. `CDescriptorPool`과 `CSingleDescriptorAllocator`의 결정적인 차이는?

① CDescriptorPool은 SRV만, CSingleDescriptorAllocator는 CBV만 담는다  
② CDescriptorPool은 `SHADER_VISIBLE`(GPU가 직접 읽음), CSingleDescriptorAllocator는 `FLAG_NONE`(CPU 측 보관용)이다  
③ CDescriptorPool은 영구 보관, CSingleDescriptorAllocator는 프레임마다 Reset된다  
④ CDescriptorPool은 힙을 쓰지 않고 스택 메모리를 사용한다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```cpp
// CDescriptorPool
commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // GPU가 직접 읽음, 프레임마다 Reset

// CSingleDescriptorAllocator
commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;           // CPU 측 창고, 텍스처 수명만큼 유지
```

GPU는 `SHADER_VISIBLE` 힙만 직접 참조할 수 있다. 따라서 렌더링 직전에 `CopyDescriptorsSimple()`로 non-visible → visible 복사가 필요하다.

</details></td></tr></table>

---

## Q7. `D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV` 타입의 힙 하나에 CBV와 SRV를 섞어서 넣을 수 있는가?

① 없다. CBV 전용 힙과 SRV 전용 힙을 분리해야 한다  
② 있다. 이 타입은 CBV, SRV, UAV 세 종류를 같은 힙의 서로 다른 슬롯에 혼합 배치할 수 있다  
③ 있다. 단, CBV와 SRV를 같은 슬롯에 겹쳐서 넣을 수도 있다  
④ 없다. UAV가 있을 때만 혼합이 가능하다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`CBV_SRV_UAV` 는 세 종류를 하나의 힙 타입으로 통합한 것이다. 슬롯별로 어떤 타입이 들어가는지는 루트 시그니처가 정의하고, `CopyDescriptorsSimple()`은 슬롯 인덱스만으로 복사한다. `DescriptorPool`에서 드로우콜마다 `슬롯[0]=CBV, 슬롯[1~N]=SRV` 형태로 혼합 배치하는 것이 이 때문이다.

</details></td></tr></table>

---

## Q8. `CDescriptorPool`이 2개(`[0]`, `[1]`)인 이유는?

① 박스의 앞면/뒷면을 각각 다른 풀에서 처리하기 위해  
② CBV용과 SRV용을 분리하기 위해  
③ 스왑체인 더블버퍼링처럼 이전 프레임 GPU 작업이 끝나기 전에 다음 프레임을 준비하기 위해  
④ D3D12 API가 최소 2개의 descriptor pool을 요구하기 때문에  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

`MAX_PENDING_FRAME_COUNT = SWAP_CHAIN_FRAME_COUNT - 1 = 2`이다.  
프레임 N을 GPU가 실행하는 동안 CPU는 프레임 N+1의 드로우콜을 준비한다. 두 프레임이 서로 다른 `DescriptorPool` 인덱스를 사용하므로 GPU가 읽는 도중 CPU가 같은 슬롯을 덮어쓰는 일이 없다.  
`Present()` 안에서 `WaitForFenceValue()`로 이전 프레임 완료를 확인한 뒤 `Reset()`한다.

</details></td></tr></table>

---

## Q9. `AllocDescriptorTable()`이 `cpuDescriptorTable`과 `gpuDescriptorTable`을 둘 다 반환하는 이유는?

① CPU 핸들은 디버그용, GPU 핸들은 실제 렌더링용으로 용도가 다르기 때문이다  
② 같은 힙의 같은 슬롯을 CPU 주소로는 쓰기(CopyDescriptorsSimple), GPU 주소로는 읽기(SetGraphicsRootDescriptorTable)에 각각 사용하기 때문이다  
③ D3D12 규칙상 두 핸들 중 하나가 유효하지 않을 경우를 대비한 백업이다  
④ cpuDescriptorTable은 CBV, gpuDescriptorTable은 SRV 전용이기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`cpuDescriptorTable`과 `gpuDescriptorTable`은 같은 힙, 같은 슬롯의 두 가지 이름이다.  
- CPU 주소 → `CopyDescriptorsSimple(Dest=cpuDescriptorTable, ...)` 로 내용을 쓴다  
- GPU 주소 → `SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)` 로 GPU에 전달한다  

CPU에서 쓴 내용이 GPU 주소에서 그대로 읽히는 구조이며, 복사나 전송이 추가로 발생하지 않는다.

</details></td></tr></table>

---

## Q10. 박스 오브젝트(`m_dwTriGroupCount = 6`) 1개를 Draw할 때 `DescriptorPool`에서 소비되는 슬롯 수는?

① 1개 (CBV 1개)  
② 6개 (SRV 6개)  
③ 7개 (CBV 1 + SRV 6)  
④ 9개 (CBV 1 + SRV 8, 빈 슬롯 포함)  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
DWORD dwRequiredDescriptorCount =
    DESCRIPTOR_COUNT_PER_OBJ + (m_dwTriGroupCount * DESCRIPTOR_COUNT_PER_TRI_GROUP);
    // = 1 + (6 × 1) = 7
```

`MAX_DESCRIPTOR_COUNT_FOR_DRAW = 9`는 최대 상한(TriGroup 8개 기준)이고, 실제 할당은 `m_dwTriGroupCount`에 따라 달라진다. 박스는 6면이므로 7슬롯이 소비된다.

</details></td></tr></table>

---

## Q11. `CHashTable::Select()`에서 `CreateKey()`로 버킷을 찾은 뒤에도 `memcmp()`를 반드시 하는 이유는?

① 키 데이터의 대소문자를 구분하기 위해  
② 서로 다른 파일명이 같은 버킷 인덱스(해시 충돌)를 가질 수 있기 때문에  
③ `CreateKey()`가 항상 정확한 결과를 보장하지 않기 때문에  
④ 동일 파일을 두 번 Insert한 경우를 탐지하기 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`CreateKey()`는 파일명 바이트를 합산하여 버킷 수로 나머지를 구하는 단순 해시다. 서로 다른 파일명이 같은 버킷 인덱스를 가질 수 있다(해시 충돌). 버킷 안의 연결 리스트에 충돌 항목들이 체이닝되므로, 리스트를 순회하면서 `memcmp()`로 실제 키를 완전 비교해야 정확한 항목을 찾을 수 있다.

</details></td></tr></table>

---

## Q12. `pTexHandle->pSearchHandle`에 `HashTable::Insert()`의 반환값을 저장하는 이유는?

① pSearchHandle이 없으면 Select()를 호출할 수 없기 때문이다  
② Delete() 시 파일명 해시를 재계산하지 않고 O(1)로 해당 노드를 바로 제거하기 위해  
③ pSearchHandle은 텍스처의 GPU 메모리 주소를 가리키기 때문이다  
④ COM 참조 카운트와 연동되어 자동 해제를 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`HashTable::Insert()`는 삽입된 `VB_BUCKET` 노드의 포인터를 반환한다. 이를 저장해두면 `Delete(pSearchHandle)` 호출 시 파일명을 다시 해시하거나 리스트를 순회할 필요 없이 해당 노드를 즉시 연결 리스트에서 제거하고 `free()`할 수 있다.

</details></td></tr></table>

---

## Q13. `shader-visible` 힙에 바인딩할 수 있는 `CBV_SRV_UAV` 타입 힙은 커맨드 리스트당 몇 개인가?

① 제한 없음  
② 최대 4개  
③ 타입당 1개 (CBV_SRV_UAV 1개 + Sampler 1개)  
④ 최대 2개  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

```cpp
// SetDescriptorHeaps의 제약: 타입당 1개
ID3D12DescriptorHeap* ppHeaps[] = { pDescriptorPool->INL_GetDescriptorHeap() };
pCommandList->SetDescriptorHeaps(1, ppHeaps);
```

D3D12는 `SetDescriptorHeaps()`에 `CBV_SRV_UAV` 타입 1개, `Sampler` 타입 1개, 합계 최대 2개만 허용한다. 이 제약 때문에 non-visible 창고(SingleDescriptorAllocator, SimpleConstantBufferPool의 힙)에서 shader-visible 힙(DescriptorPool) 하나로 `CopyDescriptorsSimple()`이 필요한 것이다.

</details></td></tr></table>

---

## Q14. Chapter 15에서 `CD3D12Renderer`가 직접 가지고 있던 `m_pTexLinkHead/Tail`이 Chapter 16에서 어디로 이동했는가?

① `CD3D12ResourceManager`  
② `CSingleDescriptorAllocator`  
③ `CTextureManager`  
④ `CDescriptorPool`  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

Chapter 16에서 텍스처 생명주기 관리 전체(`SORT_LINK` 연결 리스트, `AllocTextureHandle`, `FreeTextureHandle`)가 `CTextureManager`로 이전되었다. `CD3D12Renderer`는 `m_pTextureManager` 하나만 유지하고 텍스처 관련 모든 작업을 위임한다.

</details></td></tr></table>

---

## Q15. `CreateDynamicTexture()`로 만든 텍스처를 `CreateTextureFromFile()`처럼 동일 크기로 두 번 호출하면 어떻게 되는가?

① 두 번째 호출 시 해시 테이블에서 히트하여 같은 핸들을 반환한다  
② 매번 새 GPU 리소스가 생성되어 독립적인 핸들 두 개가 반환된다  
③ 두 번째 호출은 실패하고 nullptr를 반환한다  
④ 두 번째 호출 시 첫 번째 텍스처의 내용을 덮어쓴다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

`CreateDynamicTexture()`는 해시 테이블 조회 코드가 없다. 파일명이라는 고정 키가 없고, CPU에서 매 프레임 다른 내용을 쓰는 텍스처이므로 공유가 불가능하다. 호출할 때마다 `ResourceManager::CreateTexturePair()`로 새 GPU 리소스와 Upload 버퍼를 생성하고 독립된 `TEXTURE_HANDLE`을 반환한다.

</details></td></tr></table>
