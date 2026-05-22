# DirectX 12: Texture Resource & Pipeline Binding Flow

DirectX 12에서 CPU의 이미지 데이터를 GPU 전용 메모리에 올리고, 이를 셰이더(HLSL) 레지스터에 최종 바인딩하기까지의 전체 파이프라인 정리입니다.

---

## 1. 텍스처 리소스 생성 및 메모리 정렬 (Upload Buffer & RowPitch)

### 1-1. GPU 메모리(Heap)의 성격
* `D3D12_HEAP_TYPE_DEFAULT`: GPU 전용 고속 메모리(VRAM). CPU가 직접 접근(`memcpy`)할 수 없습니다.
* `D3D12_HEAP_TYPE_UPLOAD`: CPU가 접근 가능한 중간 임시 창고 버퍼.

### 1-2. 메모리 정렬 규칙과 RowPitch
* **CPU 원본 데이터**: 데이터가 빈틈없이 다닥다닥 붙어 있음 (`Width * 4` 바이트 간격).
* **GPU 버퍼 규격**: DX12 하드웨어 규칙상 텍스처의 한 줄(Row)은 반드시 **256바이트의 배수**로 정렬되어야 함.
* **`Footprint.Footprint.RowPitch`**: GPU 규격에 맞춰 패딩(여백)을 포함한 한 줄의 실제 크기.
* **복사 메커니즘**: `pUploadBuffer->Map()`을 통해 얻은 가상 주소 포인터(`pMappedPtr`)에 한 줄씩 복사할 때, CPU 포인터는 `Width * 4`만큼 이동하지만, GPU 포인터는 `RowPitch`만큼 건너뛰며 복사해야 데이터 오염(사선 찢어짐 등)이 발생하지 않습니다.

---

## 2. 루트 시그니처 (Root Signature) 인터페이스 설계

### 2-1. Root Parameter와 Descriptor Range의 관계
* **`D3D12_DESCRIPTOR_RANGE` (내부 목록)**: 레지스터의 연속된 범위를 정의 (예: SRV 종류를 t0 레지스터부터 시작).
* **`D3D12_ROOT_PARAMETER` (범용 인자 슬롯)**: 셰이더 파이프라인이 받는 최상위 매개변수 상자. `Descriptor Table`, `Root Constant`, `Root Descriptor` 중 어떤 방식으로 데이터를 넘길지 결정하는 슬롯 창구 역할을 합니다.
* **관계성**: 둘의 배열 크기는 같을 필요가 전혀 없습니다. `Root Parameter Size`는 큰 통로의 개수이며, `Range Size`는 하나의 `Descriptor Table` 통로를 통과할 세부 자원 목록의 개수입니다.

### 2-2. Range의 연속성 활용 (묶음 배송)
* 개별 선언: `ranges[0].Init(..., 1, 0);` (t0) + `ranges[1].Init(..., 1, 1);` (t1)
* **통합 선언**: `ranges[0].Init(..., 2, 0);` -> t0부터 연속으로 2개의 Descriptor를 묶어서 처리하겠다는 뜻. (코드 간결화 및 최적화)

### 2-3. 왜 루트 시그니처를 직렬화(Serialize) 하는가?
* C++ 구조체의 복잡한 포인터 관계를 끊어내고, 하드웨어(NVIDIA, AMD 등 드라이버)가 즉시 해독할 수 있는 표준 **연속 바이너리 블록(`ID3DBlob`)**으로 만들기 위함입니다.
* 이를 통해 디스크에 파일로 캐싱하여 게임 로딩 속도를 극대화할 수 있습니다.

---

## 3. Descriptor Heap과 최종 바인딩 (그리기 직전 단계)

### 3-1. SRV (Shader Resource View)의 정체
* 실제 이미지 픽셀 데이터는 `m_pTexResource`에 있고, **SRV는 이 데이터를 셰이더가 어떻게 해석해야 하는지 적어둔 '설명서 카드'**입니다. 이 카드는 `D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV` 종류의 Descriptor Heap에 저장됩니다.
* C++ 측 인덱스(`BASIC_MESH_DESCRIPTOR_INDEX_TEX = 0`)를 사용하여 힙의 정확한 칸 주소를 계산하고 카드를 기록합니다.

### 3-2. 그리기 직전 바인딩 3신기
1. `SetGraphicsRootSignature(m_pRootSignature)`: "0번 통로는 2칸짜리 텍스처 통로다" (통로 개설)
2. `SetDescriptorHeaps(1, &m_pDescritorHeap)`: "이 카드 보관함(서랍장)을 열어두고 대기해라" (보관함 활성화 - 하드웨어적으로 무거운 작업이므로 프레임당 최소화)
3. `SetGraphicsRootDescriptorTable(0, gpuDescriptorTable)`: 열려 있는 보관함의 0번 칸(시작 주소)을 0번 통로에 던짐. 설계도에 개수가 `2`로 묶여 있으므로 자동으로 디퓨즈(`t0`)와 노멀맵(`t1`)이 세트로 셰이더에 공급됨.

### 3-3. Descriptor Heap을 안 거치는 다이렉트 방식들
* **Root Constant / Root Descriptor**: 덩치가 작거나 주소만 알면 되는 자원(상수 버퍼 등)은 Descriptor Heap을 전혀 거치지 않고 GPU 가상 주소나 값을 파이프라인에 직접 꽂아 넣을 수 있습니다. (초고속 접근 가능)
* **단, 이 경우에도 하드웨어 레지스터 방 배정 설계도 역할인 `Root Signature`는 무조건 필요합니다.**