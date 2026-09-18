# Architecture

## 계획 구조

아래는 구현 예정 폴더입니다. 기존 vcxproj 안에 배치합니다.

```text
D3D12/
  D3D12.vcxproj
  Engine/
    Core/       Application, Window, Timer, 오류 처리
    RHI/        D3D12Device, SwapChain, FrameContext, GPU 리소스
    Renderer/   RenderManager, RenderContext, RenderPass, Passes/
    Scene/      SceneManager, Scene, GameObject, Components/
    Asset/      AssetManager, Mesh, Material, Texture
    Shaders/    Triangle.hlsl, 이후 렌더 경로별 셰이더
  App/
    main.cpp
Base_MD/
```

RHI는 처음에는 Direct3D 12 전용 래퍼입니다. 범용 API 추상화는 이후 필요 시 검토합니다.

## 책임과 소유권

| 객체 | 책임 | 소유권 |
| --- | --- | --- |
| Application | 창 이벤트, update / render, 종료 조율 | Window와 Manager 소유 |
| RenderManager | 프레임 시작 / 제출, 패스 순서와 렌더 경로 | RHI와 패스 소유 |
| SceneManager | 활성 장면과 업데이트 | Scene 소유, M2 도입 |
| AssetManager | 메시 / 재질 / 텍스처 캐시 | 에셋 소유, M3 도입 |
| Scene / GameObject | 장면과 오브젝트 구성 | Scene이 오브젝트, 오브젝트가 Component 소유 |
| Component | Transform, Camera, MeshRenderer 등 장면 상태 | GPU 큐나 SwapChain 직접 접근 금지 |
| RenderPass | PSO 바인딩, draw / dispatch 기록 | 패스 전용 PSO와 리소스 소유 |
| FrameContext | allocator, Fence 값, 임시 프레임 데이터 | 프레임 슬롯별 소유 |

Manager를 자동으로 전역 Singleton으로 만들지 않습니다.
Application에서 생성하고 필요한 참조를 전달합니다.
Component는 객체 합성 방식이며 범용 ECS를 의미하지 않습니다.
COM은 ComPtr, 독점 소유는 unique_ptr를 사용하고 비소유 참조의 수명을 명시합니다.

## 의존성과 제출

App -> Core / Scene / Renderer, Renderer -> RHI / 렌더 데이터, Asset -> RHI.
RHI는 Scene과 App을 참조하지 않습니다.
M2부터 Component 데이터를 RenderItem / CameraData로 추출해 렌더러에 전달합니다.
MeshRenderer는 메시 / 재질 핸들과 Transform을 연결하며 직접 Draw를 호출하지 않습니다.
패스는 GameObject 내부 구조나 SceneManager를 직접 조회하지 않습니다.

## 패스 계약과 확장

RenderPass의 초기 계약은 초기화, 크기 의존 리소스 갱신, 명령 기록입니다.
RenderContext는 command list, 프레임 슬롯, viewport와 출력 타깃을 제공합니다.
각 패스의 입력, 출력, 기대 리소스 상태를 명시합니다.
RenderManager가 패스 경계의 상태 전환, 큐 제출과 Present를 조율합니다.
패스는 독립적으로 큐를 제출하거나 Present하지 않습니다.
Pass는 엔진 논리 단위이며 D3D12 BeginRenderPass API를 강제하지 않습니다.

| 경로 | 패스 순서 |
| --- | --- |
| M1 | ForwardPass -> 백버퍼 -> Present |
| 확장 Forward | Shadow(선택) -> ForwardOpaque -> Transparent -> ToneMap -> Present |
| 확장 Deferred | Shadow(선택) -> GBuffer -> DeferredLighting -> TransparentForward -> ToneMap -> Present |

M1은 백버퍼에 직접 그립니다. HDR과 ToneMap은 M4에 도입합니다.
M5에서 불투명 경로를 교체하고 투명 오브젝트는 우선 Forward로 유지합니다.
두 경로는 같은 장면 / 재질 데이터를 사용하지만 출력 셰이더와 PSO는 다릅니다.
G-buffer 포맷과 렌더 경로 선택 방식은 M5에서 확정합니다.

## GPU 수명 계약

- 단일 Direct 큐, 프레임 슬롯별 allocator와 제출 Fence 값을 사용합니다.
- 슬롯의 GPU 완료를 확인한 후 allocator를 Reset하고 데이터를 재사용합니다.
- 백버퍼는 PRESENT -> RENDER_TARGET -> PRESENT로 전환합니다.
- Barrier는 리소스 접근 / 상태를 조율하며 CPU의 GPU 완료 대기를 대체하지 않습니다.
- 제출 명령에서 사용하는 리소스는 Fence 완료 전에 해제하거나 덮어쓰지 않습니다.
- Resize는 GPU 완료 -> 백버퍼 참조 해제 -> ResizeBuffers -> RTV 재생성 순서입니다.
- 최소화나 0 크기에서는 렌더링과 ResizeBuffers를 건너뜁니다.
- 종료는 새 제출 중지 -> GPU 대기 -> 패스 / GPU 리소스 -> RHI -> 창 순서입니다.
- 초기화 실패도 RAII로 정리하고 실패한 동작과 HRESULT를 기록합니다.
- Device Lost 자동 복구는 후속 범위입니다.

## 기술 근거

- [기본 DX12 컴포넌트](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-basic-direct3d-12-component)
- [PSO 관리](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12)
- [리소스 상태 전환](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)
