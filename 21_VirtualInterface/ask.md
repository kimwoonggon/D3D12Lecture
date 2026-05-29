# Chapter 21 - Virtual Interface Q&A 기록

## Q1. Chapter 21의 Virtual Interface 핵심 정리 + Chapter 20과의 차이

- 요청: 핵심 정리 + 20과의 비교 + 함수/인자 설명 + 전체 흐름 mermaid flowchart
- 결과 산출물: [compare_20_21.md](compare_20_21.md)
- 핵심 요약:
  - 20장은 `CD3D12Renderer*` 구체 타입 + `void*` 핸들 기반 API를 직접 호출하는 구조.
  - 21장은 COM-스타일 `IRenderer`, `IMeshObject`, `ISprite` 순수 가상 인터페이스 + `IUnknown`(AddRef/Release/QueryInterface) 도입으로 게임 측 코드와 렌더러 구현을 완전히 분리.
  - 생성은 팩토리 함수 `CreateRendererInstance(IRenderer**)`로 일원화 → DLL 경계로 분리하기 위한 사전 정지작업.
  - `void*` 핸들이었던 메시 객체가 타입 안전한 `IMeshObject*`로 승격되어 `BeginCreateMesh / InsertTriGroup / EndCreateMesh`가 객체 자신의 메서드가 됨(렌더러 메서드에서 제거).
  - `__stdcall` 호출 규약 명시로 ABI 안정성 확보(다른 컴파일러/언어 바인딩 대비).

## Q16. 워커가 `ProcessByThread()` 실행 중일 때 메인이 `SetEvent(DESTROY)`를 보내면?

- 핵심: `SetEvent`는 실행 중인 함수를 끊는 인터럽트가 아니라, 커널 이벤트 객체의 상태를 `signaled`로 바꾸는 함수다.
- 워커 스레드는 `RenderThread()` 루프 안에서 `WaitForMultipleObjects()`에 도달했을 때만 `PROCESS` 또는 `DESTROY` 이벤트를 관측한다.
- 워커가 이미 `ProcessByThread()` 안에서 렌더 큐를 처리 중이면, 그 함수는 계속 실행된다. 이 동안 `DESTROY` 이벤트는 워커에게 직접 전달되는 것이 아니라 이벤트 객체 안에 `signaled` 상태로 저장된다.
- `ProcessByThread()`가 끝난 뒤 워커가 다시 `WaitForMultipleObjects()`를 호출하면, 이미 `DESTROY`가 signaled 상태이므로 wait는 즉시 반환되고 `RENDER_THREAD_EVENT_TYPE_DESTROY` case로 들어가 스레드가 종료된다.
- 이것이 cooperative shutdown이다. 종료 요청자가 강제로 함수를 중단시키지 않고, 작업자가 스스로 안전한 중단 지점(wait 루프)에 돌아왔을 때 종료 요청을 확인하고 빠져나간다.
- 이 코드에서 `CleanupRenderThreadPool()`은 `SetEvent(DESTROY)` 후 `WaitForSingleObject(hThread, INFINITE)`로 해당 워커가 실제 종료될 때까지 기다린다. 따라서 워커가 `ProcessByThread()` 중이면 메인 스레드는 그 처리가 끝날 때까지 블록된다.
