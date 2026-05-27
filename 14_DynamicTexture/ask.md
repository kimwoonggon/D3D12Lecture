# Chapter 14 - DynamicTexture Q&A

## Q. UpdateTexture 코드 (RunGame 내부)는 무슨 의미인가?

매 프레임 CPU 이미지 버퍼(`g_pImage`)의 16×16 타일을 하나씩 단색으로 칠하고, 전체 버퍼를 GPU 텍스처에 업로드하는 Dynamic Texture 데모이다.

- 텍스처를 16×16 픽셀 타일로 나눠 `g_dwCount`번째 타일을 칠한다.
- 색상은 R → G → B 순으로 8씩 증가하며 무지개처럼 변한다.
- `UpdateTextureWithImage()`로 CPU 버퍼 전체를 GPU Upload Buffer에 복사 후 `bUpdated = TRUE` 세팅.
- 렌더 시 `bUpdated`가 TRUE이면 Upload Buffer → pTexResource(DEFAULT Heap)으로 CopyTextureRegion 실행.

---

## Q. 타일 인덱스 → 2D 좌표 변환 공식은 맞는가? (g_ImageWidth=512, g_ImageHeight=256)

```
TILE_WIDTH_COUNT = 512 / 16 = 32
TILE_HEIGHT_COUNT = 256 / 16 = 16
총 타일 수 = 512

TileY = g_dwCount / TILE_WIDTH_COUNT
TileX = g_dwCount % TILE_WIDTH_COUNT
StartX = TileX * TILE_WIDTH
StartY = TileY * TILE_HEIGHT
```

공식 맞음. 1D 인덱스를 2D 좌표로 변환하는 표준 공식이다.  
마지막 타일(count=511): StartX+15=511 < 512, StartY+15=255 < 256 — 범위 초과 없음.

---

## Q. pTexResource와 pUploadBuffer의 차이는?

| | pTexResource | pUploadBuffer |
|---|---|---|
| Heap 타입 | `D3D12_HEAP_TYPE_DEFAULT` (GPU 전용) | `D3D12_HEAP_TYPE_UPLOAD` (CPU 쓰기 가능) |
| CPU 직접 접근 | 불가 | `Map()`/`Unmap()`으로 가능 |
| 셰이더 접근 | SRV로 샘플링 | 불가 |
| 리소스 종류 | Texture2D | Buffer |

**데이터 흐름:**
```
CPU (g_pImage) → memcpy → pUploadBuffer (Map/Unmap) → CopyTextureRegion → pTexResource
```

GPU 셰이더가 사용하는 텍스처는 반드시 DEFAULT Heap에 있어야 고속 접근 가능하므로 2단계 구조가 필요하다.

---

## Q. Texture Handle을 Linked List로 관리하는 이유는?

- 텍스처 개수가 런타임에 가변적이므로 배열보다 Linked List가 적합하다.
- 중간 삭제 O(1), 최대 개수 제한 없음.
- `TEXTURE_HANDLE` 구조체 내부에 `SORT_LINK` 노드가 내장되어 별도 노드 할당 없이 리스트 조작 가능.
- Chapter 14에서는 순회 코드가 없지만, 이후 챕터에서 Cleanup 시 일괄 해제, 디바이스 로스트 시 전체 재생성 등에 활용된다.

---

## Q. SingleDescriptorAllocator에서 인덱스 역산 공식

```cpp
DWORD dwIndex = (handle.ptr - base.ptr) / m_DescriptorSize;
```

Descriptor Heap은 슬롯이 `m_DescriptorSize` 바이트 간격으로 선형 배치된다.  
`ptr`은 바이트 단위 주소값(`SIZE_T`)이므로:

```
slotN.ptr = base.ptr + N * m_DescriptorSize
→ N = (slotN.ptr - base.ptr) / m_DescriptorSize
```

`m_DescriptorSize`는 GPU 벤더마다 달라서 `GetDescriptorHandleIncrementSize()`로 런타임에 조회한다.

---

## Q. AllocTextureHandle / FreeTextureHandle 역할

**AllocTextureHandle:**
1. `new TEXTURE_HANDLE` — 힙에 구조체 할당
2. `memset 0` — 전체 초기화
3. `Link.pItem = pTexHandle` — Link 노드에서 자기 자신을 역참조할 수 있도록 세팅
4. `LinkToLinkedListFIFO` — 렌더러의 텍스처 리스트 꼬리에 추가

**FreeTextureHandle:**
1. `UnLinkFromLinkedList` — 리스트에서 제거
2. `delete pTexHandle` — 메모리 해제

GPU 리소스(`pTexResource`, `pUploadBuffer`, `srv`)는 이 함수들이 담당하지 않는다.  
`DeleteTexture()`에서 먼저 `Release()` 후 `FreeTextureHandle()`을 호출하는 순서로 처리된다.

---

## Q. 텍스처가 여러 개일 때 Linked List로 관리하는 진짜 이유는?

**Ch14에서는 텍스처가 2개뿐이고 중간 삭제도 없다.** 시작에 만들고 종료 시 지우는 단순 구조라 Linked List가 사실상 과분하다.

Linked List는 **이후 챕터 확장을 위한 설계**로, 다음 두 가지 목적을 가진다:

### 1. 종료 시 누수 감지 (Ch16)
```cpp
void CTextureManager::Cleanup()
{
    if (m_pTexLinkHead)
    {
        // texture resource leak!!!
        __debugbreak();  // 리스트가 비어있지 않으면 누수로 간주
    }
}
```
렌더러 종료 시점에 아직 리스트에 남아있는 핸들이 있으면 누수이므로 즉시 감지 가능하다.

### 2. 런타임 중간 삭제 + RefCount (Ch16 TextureManager)

같은 파일을 여러 오브젝트가 공유할 때 RefCount로 실제 해제 시점을 결정한다:

```
CreateTextureFromFile("stone.dds")  오브젝트 A → RefCount = 1
CreateTextureFromFile("stone.dds")  오브젝트 B → RefCount = 2  (공유, GPU 리소스 재사용)

오브젝트 A 소멸 → DeleteTexture() → RefCount = 1  (GPU 리소스 유지)
오브젝트 B 소멸 → DeleteTexture() → RefCount = 0  → 실제 GPU 리소스 해제 + 리스트에서 제거
```

**"종료 시에만 지우면 되는 거 아님?"** — 정적인 씬이라면 맞다. 하지만 레벨 전환, 동적 오브젝트 생성/소멸이 있으면 아무도 안 쓰는 텍스처가 메모리에 계속 쌓이므로 런타임 중 해제가 필요하다.

Linked List는 **"지금 살아있는 텍스처가 무엇인지"** 를 항상 추적하기 위한 렌더러의 원장(ledger) 역할을 한다.

---

## Q. pUploadBuffer는 GPU 메모리인가 CPU 메모리인가?

**물리 위치는 시스템 RAM(CPU 쪽)** 이지만, GPU 주소 공간에 매핑되어 있어서 GPU도 읽을 수 있다.

| Heap 타입 | 물리 위치 | CPU 접근 | GPU 접근 속도 |
|---|---|---|---|
| `HEAP_TYPE_DEFAULT` | VRAM (GPU 전용) | 불가 | 최고속 |
| `HEAP_TYPE_UPLOAD` | 시스템 RAM | `Map()`으로 직접 읽기/쓰기 | 느림 (PCIe 버스 경유) |

```
시스템 RAM
┌─────────────────────────────┐
│  pUploadBuffer              │
│  - CPU: Map()으로 직접 쓰기  │
│  - GPU: PCIe 통해 읽기 가능  │
└─────────────────────────────┘

VRAM
┌─────────────────────────────┐
│  pTexResource               │
│  - CPU: 접근 불가            │
│  - GPU: 고속 직접 접근       │
└─────────────────────────────┘
```

> **Upload Buffer = 시스템 RAM에 있지만 GPU 주소 공간에 매핑된 메모리**

- `Map()`이 가능한 이유: 물리 위치가 시스템 RAM이라 CPU가 포인터로 직접 씀
- GPU가 읽을 수 있는 이유: GPU 주소 공간에 매핑되어 있어 PCIe를 통해 접근 가능
- `CopyTextureRegion()`은 GPU가 PCIe 경유로 Upload Buffer를 읽어 VRAM(pTexResource)으로 복사하는 작업
