# Chapter 14 — DynamicTexture 퀴즈 (객관식)

> 각 문제의 **정답 보기 ▼** 를 클릭하면 정답이 나옵니다.

---

## Q1. `UpdateTextureWithImage()`를 호출하면 GPU 텍스처에 언제 반영되는가?

① 호출 즉시 GPU 텍스처가 갱신된다  
② `Map()`/`Unmap()` 완료 시점에 GPU 텍스처가 갱신된다  
③ 다음 `RenderSpriteWithTex()` 호출 시 `bUpdated` 플래그를 보고 GPU 커맨드로 복사된다  
④ `Present()` 호출 시 자동으로 복사된다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

`UpdateTextureWithImage()`는 `pUploadBuffer`에 CPU 데이터를 쓰고 `bUpdated = TRUE`만 세팅한다.  
실제 GPU 복사(`CopyTextureRegion`)는 다음 프레임 `RenderSpriteWithTex()` 내부에서 `bUpdated == TRUE`일 때 커맨드 리스트에 기록된다.  
`pTexResource`가 `HEAP_TYPE_DEFAULT`(GPU 전용)이라 CPU가 직접 쓸 수 없어 이 2단계 구조가 필요하다.

</details></td></tr></table>

---

## Q2. `pTexResource`와 `pUploadBuffer`에 대한 설명으로 올바른 것은?

① 둘 다 `D3D12_HEAP_TYPE_DEFAULT`이며 CPU에서 직접 쓸 수 있다  
② `pTexResource`는 CPU가 `Map()`으로 직접 쓰고, `pUploadBuffer`는 셰이더가 샘플링한다  
③ `pTexResource`는 GPU 전용(DEFAULT Heap), `pUploadBuffer`는 CPU 쓰기 가능(UPLOAD Heap)이다  
④ `pUploadBuffer`는 선택적 최적화 버퍼이며 없어도 동작한다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

| | pTexResource | pUploadBuffer |
|---|---|---|
| Heap 타입 | `HEAP_TYPE_DEFAULT` (GPU 전용) | `HEAP_TYPE_UPLOAD` (CPU 쓰기 가능) |
| CPU 접근 | 불가 | `Map()`/`Unmap()` 가능 |
| 셰이더 접근 | SRV로 샘플링 | 불가 |

셰이더가 고속으로 접근하려면 반드시 DEFAULT Heap이어야 하고, CPU는 UPLOAD Heap을 경유해야만 데이터를 전달할 수 있다.

</details></td></tr></table>

---

## Q3. `g_ImageWidth=512`, `g_ImageHeight=256`, `g_dwCount=100`일 때 `StartX`와 `StartY`는?

① `StartX=48, StartY=64`  
② `StartX=64, StartY=48`  
③ `StartX=100, StartY=0`  
④ `StartX=16, StartY=48`  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

```
TILE_WIDTH_COUNT = 512 / 16 = 32

TileX = 100 % 32 = 4  →  StartX = 4 * 16 = 64
TileY = 100 / 32 = 3  →  StartY = 3 * 16 = 48
```

</details></td></tr></table>

---

## Q4. 아래 공식에서 `TILE_WIDTH_COUNT`(가로 타일 수)로 나누는 이유로 올바른 것은?

```cpp
TileY = g_dwCount / TILE_WIDTH_COUNT;
TileX = g_dwCount % TILE_WIDTH_COUNT;
```

① 세로 타일 수가 항상 가로보다 크기 때문이다  
② 1D 인덱스를 2D로 변환할 때 한 행이 끝나는 기준은 가로 타일 수이기 때문이다  
③ `TILE_HEIGHT_COUNT`로 나누면 나머지가 발생하기 때문이다  
④ 텍스처는 항상 정사각형이므로 어느 쪽으로 나눠도 같다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

왼쪽→오른쪽, 위→아래로 채울 때 `g_dwCount`가 가로 타일 수만큼 증가하면 한 행이 끝난다.  
- **행 번호** = count / 가로 타일 수  
- **열 번호** = count % 가로 타일 수  

`TILE_HEIGHT_COUNT`로 나누면 행/열이 뒤바뀌어 틀린 좌표가 나온다.

</details></td></tr></table>

---

## Q5. `UpdateTextureWithImage()` 내부에서 `memcpy`를 전체 한 번이 아니라 행 단위로 반복하는 이유는?

① GPU 커맨드 큐가 행 단위 전송만 지원하기 때문이다  
② CPU 캐시 효율을 높이기 위해서다  
③ GPU의 `RowPitch`가 CPU 이미지의 행 크기보다 클 수 있어 패딩을 맞춰야 하기 때문이다  
④ `memcpy`의 최대 크기 제한 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

GPU 텍스처 리소스는 하드웨어 요구사항에 따라 행 간격(`RowPitch`)이 256바이트 정렬 등으로 패딩된다.  
`RowPitch > SrcWidth * 4`이면 한 번에 전체 복사 시 행 사이 패딩이 없어 데이터가 어긋난다.  
행마다 `SrcWidth * 4`만 복사하고 dest 포인터를 `RowPitch`씩 전진시켜야 GPU 메모리 레이아웃에 정확히 맞춰진다.

</details></td></tr></table>

---

## Q6. `TEXTURE_HANDLE` 구조체 내부에 `SORT_LINK`를 직접 내장한 목적으로 가장 적절한 것은?

① `SORT_LINK`는 크기가 커서 동적 할당 비용을 줄이기 위해서다  
② `TEXTURE_HANDLE` 1회 할당으로 노드까지 포함하고, `Link.pItem`으로 자기 자신을 역참조하기 위해서다  
③ C++ 표준 컨테이너를 사용할 수 없는 환경이기 때문이다  
④ 렌더러가 텍스처를 정렬된 순서로 그리기 위해서다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

**Intrusive Linked List** 패턴으로, 구조체 내부에 노드를 내장함으로써:
- `TEXTURE_HANDLE` 1회 `new`로 노드까지 포함해 할당 횟수 절감
- `Link.pItem = pTexHandle`로 세팅해 리스트 순회 시 `Link` 포인터에서 `TEXTURE_HANDLE`을 즉시 역참조 가능

</details></td></tr></table>

---

## Q7. `AllocTextureHandle()`과 `FreeTextureHandle()`의 역할에 대한 설명으로 올바른 것은?

① `AllocTextureHandle()`이 `pTexResource`와 `pUploadBuffer`를 GPU에 생성한다  
② `FreeTextureHandle()`이 `pTexResource->Release()`를 호출해 GPU 리소스를 해제한다  
③ 두 함수는 `TEXTURE_HANDLE` 구조체 메모리와 Linked List 등록/해제만 담당하며 GPU 리소스는 건드리지 않는다  
④ `FreeTextureHandle()`은 `DeleteTexture()` 이전에 호출해야 한다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ③**

- `AllocTextureHandle()` → `new TEXTURE_HANDLE` + `LinkToLinkedListFIFO()`
- `FreeTextureHandle()` → `UnLinkFromLinkedList()` + `delete pTexHandle`

GPU 리소스 해제는 `DeleteTexture()`에서 담당한다:
```
DeleteTexture()
  → pTexResource->Release()
  → pUploadBuffer->Release()
  → FreeDescriptorHandle(srv)
  → FreeTextureHandle()  ← 마지막에 구조체 해제
```

④는 반대다. `DeleteTexture()`가 `FreeTextureHandle()`을 마지막에 호출한다.

</details></td></tr></table>

---

## Q8. 아래 코드에서 나눗셈이 항상 나누어 떨어지는 이유는?

```cpp
DWORD dwIndex = (DescriptorHandle.ptr - base.ptr) / m_DescriptorSize;
```

① GPU가 자동으로 정렬을 맞춰주기 때문이다  
② 할당 시 `base.ptr + index * m_DescriptorSize`로 주소를 계산했기 때문이다  
③ `ptr`은 바이트가 아니라 슬롯 단위이기 때문이다  
④ `m_DescriptorSize`가 항상 2의 거듭제곱이기 때문이다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

할당 시:
```cpp
handle.ptr = base.ptr + index * m_DescriptorSize
```
이므로 `handle.ptr - base.ptr`은 항상 `m_DescriptorSize`의 배수다.  
`ptr`은 `SIZE_T` 타입의 바이트 단위 주소값이며, `m_DescriptorSize`는 슬롯 1개의 바이트 크기다.  
`GetDescriptorHandleIncrementSize()`로 런타임에 GPU 벤더별 크기를 조회해서 사용한다.

</details></td></tr></table>

---

## Q9. Chapter 14에서 텍스처가 2개뿐임에도 Linked List를 도입한 이유로 올바른 것을 모두 고르시오.

① 2개의 텍스처를 정렬된 순서로 렌더링하기 위해  
② 이후 챕터에서 텍스처 수가 늘어날 때 가변 크기 관리와 O(1) 중간 삭제를 지원하기 위해  
③ 렌더러 종료 시 리스트가 비어있지 않으면 텍스처 누수로 감지하기 위해  
④ GPU 리소스 복사 순서를 보장하기 위해  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②, ③**

② Ch16 이후 텍스처가 수십~수백 개로 늘어나고 런타임 생성/삭제 발생 시 Linked List가 필수.  
배열은 가변 크기에 부적합하고 중간 삭제가 O(n)이다.

③ Ch16 `Cleanup()`에서:
```cpp
if (m_pTexLinkHead)
{
    // texture resource leak!!!
    __debugbreak();
}
```
리스트가 남아있으면 `DeleteTexture()`를 호출하지 않은 누수로 즉시 감지한다.

</details></td></tr></table>

---

## Q10. Ch16 TextureManager에서 오브젝트 A, B가 같은 파일로 `CreateTextureFromFile()`을 호출한 뒤 A가 먼저 소멸했다. 이때 GPU 리소스 상태로 올바른 것은?

① A 소멸 시 GPU 리소스가 즉시 해제된다  
② A 소멸 시 GPU 리소스는 유지되고, B까지 소멸해야 비로소 해제된다  
③ 두 오브젝트가 각각 별도의 GPU 리소스를 가지므로 A 소멸 시 A의 리소스만 해제된다  
④ `Cleanup()` 호출 전까지는 절대 해제되지 않는다  

<table><tr><td><details><summary>정답 보기 ▼</summary>

**정답: ②**

같은 파일명이면 HashTable에서 동일한 `TEXTURE_HANDLE`을 반환하고 `dwRefCount`만 증가시킨다.

```
CreateTextureFromFile("stone.dds")  A → dwRefCount = 1  (GPU 리소스 1개 생성)
CreateTextureFromFile("stone.dds")  B → dwRefCount = 2  (GPU 리소스 재사용)

A 소멸 → DeleteTexture() → dwRefCount = 1  (GPU 리소스 유지)
B 소멸 → DeleteTexture() → dwRefCount = 0  → pTexResource->Release() 실제 해제
```

마지막 참조자가 반납할 때만 실제 GPU 리소스가 해제된다.

</details></td></tr></table>

---

*퀴즈 끝 — Chapter 14 DynamicTexture*
