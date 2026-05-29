# Q&A

## Q. TriGroup이 왜 8개로 제한되어 있지?

`BasicMeshObject.h`의 상수 체인이 핵심입니다.

```cpp
static const UINT DESCRIPTOR_COUNT_PER_OBJ       = 1;   // CBV 1개
static const UINT DESCRIPTOR_COUNT_PER_TRI_GROUP  = 1;   // SRV(tex) 1개
static const UINT MAX_TRI_GROUP_COUNT_PER_OBJ     = 8;   // ← 여기
static const UINT MAX_DESCRIPTOR_COUNT_FOR_DRAW   = 1 + (8 × 1) = 9;
```

그리고 `D3D12Renderer.cpp`에서:

```cpp
// MAX_DRAW_COUNT_PER_FRAME = 1024
m_ppDescriptorPool[i]->Initialize(
    m_pD3DDevice,
    MAX_DRAW_COUNT_PER_FRAME * CBasicMeshObject::MAX_DESCRIPTOR_COUNT_FOR_DRAW
    // = 1024 × 9 = 9216 디스크립터
);
```

**즉 8은 기능적 한계가 아니라 DescriptorPool 크기를 미리 계산하기 위한 "최대 가정값"입니다.**

DescriptorPool은 프레임당 고정 크기로 미리 할당됩니다.
- 오브젝트 1개당 디스크립터 최대 9개 (CBV 1 + SRV 8)
- 프레임당 오브젝트 최대 1024개
- → 총 9216 슬롯을 미리 예약

만약 TriGroup을 16개로 올리면 오브젝트당 17개 디스크립터가 되어 DescriptorPool 총량도 함께 커집니다.
결국 "메모리 사전 계산을 단순하게 유지하기 위한 강사의 교육용 상수"입니다.

---

## Q. BasicMeshObject와 SpriteObject의 차이가 뭐지?

### 핵심 차이 한 줄 요약

| | `CBasicMeshObject` | `CSpriteObject` |
|---|---|---|
| **좌표계** | 3D 월드 공간 (XMMATRIX matWorld) | 2D 화면 픽셀 공간 (iPosX, iPosY) |
| **용도** | 3D 메시 오브젝트 (큐브, 캐릭터 등) | 2D UI / 스프라이트 (HUD, 텍스트 오버레이 등) |

---

### 세부 비교

#### 버텍스 / 인덱스 버퍼

- `CBasicMeshObject`  
  - 버텍스 버퍼를 **인스턴스마다 개별** 소유 (`m_pVertexBuffer` — non-static)  
  - 여러 개의 `INDEXED_TRI_GROUP` 을 가질 수 있음 (최대 8개) → 하나의 메시에 **여러 재질/텍스처** 적용 가능  
  - `BeginCreateMesh / InsertIndexedTriList / EndCreateMesh` 로 런타임에 메시 데이터 구성

- `CSpriteObject`  
  - 버텍스/인덱스 버퍼가 **static** (모든 인스턴스 공유) → 항상 동일한 사각형(quad) 2장짜리 폴리곤  
  - 메시 형태가 고정(단순 quad)이고 위치/크기는 상수 버퍼로 매 프레임 전달

#### 렌더링 파라미터

- `CBasicMeshObject::Draw(pCommandList, pMatWorld)`  
  - `XMMATRIX` 월드 행렬을 받아 3D 공간에 배치  
  - View / Projection 행렬과 곱해져 원근투영 적용

- `CSpriteObject::DrawWithTex(pCommandList, pPos, pScale, pRect, Z, pTexHandle)`  
  - 픽셀 좌표(`XMFLOAT2 pPos`)와 스케일, UV 클리핑 Rect를 받아 화면에 직접 배치  
  - 직교 투영(Orthographic) 사용 → 원근 없음, 화면 크기 기준

#### 디스크립터 구성

| | 디스크립터 수 | 구성 |
|---|---|---|
| `CBasicMeshObject` | 오브젝트 1개(CBV) + 트리그룹당 1개(SRV) | CBV \| SRV per TriGroup |
| `CSpriteObject` | 고정 2개 | CBV \| SRV(tex) |

#### 텍스처 처리

- `CBasicMeshObject` : 텍스처는 TriGroup 단위로 바인딩, 초기화 시 파일에서 로드
- `CSpriteObject` : `DrawWithTex`에서 `TEXTURE_HANDLE*` 를 매 호출마다 동적으로 교체 가능 → **동적 텍스처(DynamicTexture)** 지원 (텍스트 렌더링 등)

---

## Q. RenderQueue를 왜 쓰는 거야?

즉시 렌더링(ch17)에서는 `RenderMeshObject()` 호출 순서 = GPU Draw 순서로 고정됩니다.
RenderQueue(ch18)는 아이템을 다 모은 뒤 `Process()` 전에 개입할 수 있는 자리를 만듭니다.

| 목적 | 설명 |
|------|------|
| **Z-sort** | 반투명 오브젝트를 뒤→앞 순서로 정렬 후 Draw → 블렌딩 정확 |
| **재질별 정렬** | 같은 PSO/텍스처끼리 묶어 Draw → State Change 횟수 감소 → GPU 성능 향상 |
| **Frustum Culling** | 카메라 밖 오브젝트는 `Add()` 자체를 안 함 → DrawCall 감소 |
| **멀티스레드 분리** | 게임 스레드는 `Add()`만, 렌더 스레드가 `Process()` → 병렬 실행 가능 |

현재 ch18에서는 위 기능이 **아직 구현되지 않았습니다.** ch18은 구조만 전환한 단계이고 실제 기능은 이후 챕터(19, 20)에서 추가됩니다.
지금은 ch17 대비 오히려 약간 느립니다 (`RENDER_ITEM` 생성 + `memcpy` 오버헤드 추가).

---

## Q. `if (pTexureHandle)` 분기가 텍스트 전용인가?

아닙니다. **텍스처가 있느냐 없느냐**의 분기입니다.

| 조건 | 의미 |
|------|------|
| `pTexHandle == nullptr` | 텍스처 자체 없음 → `Draw()` (단색/버텍스 컬러만) |
| `pTexHandle != nullptr` | 텍스처 있음 → `DrawWithTex()` (파일텍스처/타일텍스처/동적텍스처 전부 포함) |
| `pTexHandle->pUploadBuffer != nullptr` | 그 중 동적 텍스처 → CPU 갱신 업로드 처리 |

텍스트는 동적 텍스처(`pUploadBuffer` 존재)이지만, `pTexHandle != nullptr` 분기는 모든 텍스처가 공유합니다.

---

## Q. ch17 vs ch18 차이가 commandlist 기록 타이밍의 차이인가?

맞습니다. 두 챕터 모두 `ExecuteCommandLists()`는 `EndRender()`에서 딱 1번만 호출됩니다.

```
ch17: RenderMeshObject() 호출 시점 → 즉시 CommandList에 DrawIndexedInstanced 기록
ch18: RenderMeshObject() 호출 시점 → CPU 버퍼(RenderQueue)에 파라미터 복사만
      EndRender() → Process() → 그때 CommandList에 기록
```

차이는 **"CommandList에 기록하는 타이밍"** 입니다.
ch18은 `Process()` 직전에 정렬/컬링 등을 삽입할 수 있는 구조입니다.

---

## Q. `m_pCommandQueue`가 뭔가?

D3D12에는 3개의 오브젝트가 협력합니다:

| 객체 | 비유 | 역할 |
|------|------|------|
| `CommandAllocator` | 종이 묶음 | CommandList가 기록할 메모리 공간 |
| `CommandList` | 주문서 | `Draw*`, `Set*` 명령을 여기에 기록 |
| `CommandQueue` | 주문 접수 창구 | 완성된 CommandList를 GPU에 제출 |

```cpp
// CommandList에 기록 (GPU 실행 안 함)
pCommandList->DrawIndexedInstanced(...);

// CommandQueue로 GPU에 제출 (여기서 실제 GPU 실행)
m_pCommandQueue->ExecuteCommandLists(...);
```

`pCommandList->Draw*()`는 "나중에 이거 그려"를 적어두는 것이고,
`ExecuteCommandLists()`에서 GPU가 실제로 실행합니다.

---

## Q. SRV 바인딩 → 셰이더에서 샘플링이 무슨 뜻인가?

**SRV 바인딩**: CPU에서 텍스처를 셰이더가 읽을 수 있는 슬롯에 연결하는 작업

```cpp
// DrawWithTex() 내부
CopyDescriptorsSimple(1, srvDest, pTexHandle->srv, ...); // SRV를 DescriptorTable에 등록
pCommandList->SetGraphicsRootDescriptorTable(0, gpuTable); // 셰이더에 바인딩
```

**샘플링**: 셰이더가 UV 좌표(0.0~1.0)로 텍스처에서 해당 위치의 픽셀 색상을 읽는 것

```hlsl
// shSprite.hlsl
Texture2D texDiffuse : register(t0);  // t0 슬롯 = SRV가 연결된 자리
float4 PSMain(PSInput input) : SV_TARGET
{
    return texDiffuse.Sample(samplerDiffuse, input.TexCoord);
    // UV 좌표로 텍스처 픽셀 색상 읽기
}
```

연결 흐름: `pTexHandle->srv` → `DescriptorTable` → `register(t0)` → `.Sample(uv)` → 픽셀 색상

---

## Q. RenderQueue 안 데이터의 수명주기는?

| 객체 | 수명 |
|------|------|
| `CRenderQueue` 버퍼 메모리 자체 | 앱 전체 (Initialize ~ Cleanup) |
| `RENDER_ITEM` 안의 **값** (matWorld, iPosX 등) | 1프레임 (Reset() 후 덮어씌워짐) |
| `RENDER_ITEM` 안의 **포인터** (pObjHandle, pTexHandle) | 원본 오브젝트/텍스처 수명을 따름 |
| 실제 MeshObject / Texture | Delete*() 호출 시까지 |

```
프레임마다: Add() → Process() → Reset()  (버퍼 재사용, free 없음)
```

주의: `Add()` 후 같은 프레임에 `DeleteBasicMeshObject()`를 호출하면 `Process()` 시점에 dangling pointer가 됩니다.
그래서 `Delete*()` 함수 내부에서 `WaitForFenceValue()`로 GPU 완료를 먼저 기다린 후 삭제합니다.

---

## Q. `pUploadBuffer`의 데이터는 CPU에 있는 건가, GPU에 올라간 건가?

**CPU에 있으면서 GPU도 접근할 수 있는 공유 메모리**입니다.

D3D12 힙 타입 3가지:

| 힙 타입 | 위치 | CPU 접근 | GPU 접근 | 속도 |
|---------|------|----------|----------|------|
| `HEAP_TYPE_DEFAULT` | GPU VRAM | X | O | 빠름 |
| `HEAP_TYPE_UPLOAD` | CPU/GPU 공유 RAM | O (Map/Unmap) | O | 느림 |
| `HEAP_TYPE_READBACK` | CPU/GPU 공유 RAM | O | O | 느림 |

```
pTexResource   → HEAP_TYPE_DEFAULT  (GPU VRAM)
                 CPU에서 직접 못 씀
                 셰이더가 실제로 샘플링하는 곳

pUploadBuffer  → HEAP_TYPE_UPLOAD   (공유 메모리)
                 CPU에서 Map()/Unmap()으로 직접 쓸 수 있음
                 GPU→VRAM 복사의 중간 경유지 (staging buffer)
```

실제 데이터 흐름:

```
[CPU malloc 버퍼]    [UPLOAD 힙]         [DEFAULT 힙 = VRAM]
m_pTextImage    →   pUploadBuffer   →   pTexResource
                memcpy/Map/Unmap     CopyTextureRegion
                (CPU가 직접 씀)      (GPU가 CommandList 실행 시 복사)
```

`pUploadBuffer`는 "CPU가 데이터를 쓰는 스테이징 버퍼"이고, 셰이더가 샘플링할 때는 `pTexResource`(VRAM)에서 읽습니다.

---

## Q. TriGroup마다 `DrawIndexedInstanced()`를 따로 호출하는 이유 — 메모리 레이아웃

### DescriptorPool 안 레이아웃

`Draw()` 시 DescriptorPool에서 슬롯을 연속으로 할당받습니다:

```
DescriptorPool (flat 배열)
┌──────┬──────────┬──────────┬──────────┐
│ [0]  │  [1]     │  [2]     │  [3]     │
│ CBV  │ SRV      │ SRV      │ SRV      │
│(행렬)│TexGroup0 │TexGroup1 │TexGroup2 │
└──────┴──────────┴──────────┴──────────┘
 ↑ gpuDescriptorTable 시작점
```

### 셰이더가 읽는 슬롯은 t0 딱 1개

```hlsl
Texture2D texDiffuse : register(t0);  // t0 슬롯 하나만
```

GPU 파이프라인 규칙: `DrawIndexedInstanced()` 1번 실행 중에는 `t0`가 고정됩니다.  
→ "이 삼각형은 TextureA, 저 삼각형은 TextureB"를 Draw 1번으로 동시에 처리 불가능  
→ 텍스처 바꿀 때마다 t0에 새 SRV를 꽂고 Draw를 새로 호출해야 함

### Draw 루프 흐름

```
i=0: SetDescriptorTable(1, [1]번 슬롯)  ← t0 = TextureA
     IASetIndexBuffer(TriGroup[0])
     DrawIndexedInstanced()             ← TextureA로 그림
     포인터 → [2]번으로 이동

i=1: SetDescriptorTable(1, [2]번 슬롯)  ← t0 = TextureB
     IASetIndexBuffer(TriGroup[1])
     DrawIndexedInstanced()             ← TextureB로 그림
     포인터 → [3]번으로 이동
```

CBV(행렬)는 오브젝트당 1개 공유([0]번), SRV(텍스처)만 TriGroup마다 다릅니다.  
버텍스 버퍼는 모든 TriGroup이 공유하며, 인덱스 버퍼만 "어떤 버텍스를 이 텍스처로 그릴지" 지정합니다.
