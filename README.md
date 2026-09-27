# D3D12

C++20과 Direct3D 12로 작성한 Windows용 렌더러 프로젝트입니다. 현재는 Win32 창 생성, GPU 초기화, 인덱스 기반 큐브 그리기와 자유 이동 카메라를 구현한 초기 단계입니다. 장기적으로 장면/컴포넌트, 에셋 로딩, Forward 조명, PBR, Deferred 렌더링과 에디터로 확장하는 설계 문서를 포함합니다.

이 문서는 현재 소스와 프로젝트 설정을 분석한 결과입니다. 빌드 성공이나 실제 화면 출력을 검증한 기록은 아닙니다. `Base_MD`의 일부 상태 설명(코드 없음, Git 저장소 없음 등)은 현재 저장소와 다르므로, 구현 현황은 아래 내용과 소스를 기준으로 확인합니다.

## 현재 구현

| 영역 | 구현 내용 |
| --- | --- |
| 애플리케이션 | 콘솔 `main()`에서 렌더러 생성, Win32 메시지 루프와 렌더 루프 실행 |
| 창 | 초기 클라이언트 크기 1280 × 720, 제목 `D3D12 Renderer` |
| 장치 | 하드웨어 어댑터 순회, Feature Level 11_0으로 D3D12 장치 생성 시도 |
| 출력 | 백버퍼 2개, `R8G8B8A8_UNORM`, Flip Discard 스왑 체인, `Present(1, 0)` |
| 렌더링 | 정점 8개/인덱스 36개 큐브, 흰색 PSO와 검은색 PSO로 각각 한 번 그리기 |
| 셰이더 | `D3DCompileFromFile`로 HLSL 런타임 컴파일, `vs_5_0` / `ps_5_0` |
| 카메라 | WASD/QE 이동, 우클릭 마우스 회전, 좌수 좌표계 View/Projection |
| 리소스 | Upload Heap의 정점·인덱스·상수 버퍼, Root CBV `b0` |
| 동기화 | 단일 Direct 큐, 단일 allocator/list, 매 프레임 Fence 완료 대기 |
| 디버깅 | Debug 빌드에서 사용 가능하면 D3D12 Debug Layer 활성화 |

현재 픽셀 셰이더는 정점 색을 출력하지 않고 흰색 또는 검은색 상수를 반환합니다. 텍스처, 조명, 깊이 버퍼, 모델 파일 로딩은 아직 구현되어 있지 않습니다.

## 폴더 구조와 책임

```text
D3D12.slnx
D3D12/
  D3D12.vcxproj
  Core/
    main.cpp                  # 진입점
  Common/
    stdafx.h / stdafx.cpp      # 공통 헤더와 PCH
    Math.h                    # 벡터, 행렬, 좌수 View/Projection
    D3DX12.h                  # 프로젝트에서 사용하는 D3D12 보조 구조체
  Renderer/
    Renderer.h / Renderer.cpp # 창, 장치, 버퍼, 프레임 제출과 종료
    Pipeline.h / Pipeline.cpp # PIPELINE / CUBE_PSO, Root Signature와 PSO
    Manager/
      CameraManager.*         # 카메라 상태와 이동
      InputManager.*          # 키 상태, 우클릭 캡처와 마우스 변화량
    Shader/
      Shader.h / Shader.cpp   # 셰이더 경로 결정과 컴파일
      Triangle.hlsl           # 큐브에도 사용하는 VS와 흰색/검은색 PS
Base_MD/                      # 엔진 확장 설계와 개발 계획
```

`RENDERER`가 창과 대부분의 GPU 리소스를 소유하고 `PIPELINE`을 통해 `CUBE_PSO`에 접근합니다. 입력, 카메라, 셰이더 관리자는 현재 싱글턴입니다. `Base_MD/docs/ARCHITECTURE.md`의 Application/RHI/Scene/RenderPass 분리는 향후 계획이며 현재 코드 구조와는 차이가 있습니다.

## 실행 흐름

1. `main()` → `RENDERER::initialize()`에서 창, 장치, 명령 객체, 스왑 체인, RTV, PSO, 버퍼와 Fence를 생성합니다.
2. `run()`에서 Windows 메시지를 처리하고 `render_frame()`을 반복 호출합니다.
3. 입력과 카메라를 갱신하고 View × Projection 행렬을 매핑된 상수 버퍼에 복사합니다. 프레임 시간은 최대 0.1초로 제한합니다.
4. 백버퍼를 `PRESENT`에서 `RENDER_TARGET`으로 전환하고 배경을 지운 뒤 큐브를 두 번 그립니다.
5. 백버퍼를 `PRESENT`로 되돌리고 명령 제출, 화면 표시, Fence 대기를 수행합니다.
6. 창 종료 후 소멸자에서 GPU 작업 완료를 기다리고 리소스와 창을 정리합니다.

행렬은 CPU의 행 벡터 규약과 HLSL의 `row_major`, `mul(position, transform)`을 사용합니다. 별도 월드 변환은 없으며 큐브 정점에 View × Projection을 적용합니다.

## 빌드와 실행

### 프로젝트 설정 기준 요구 환경

- Windows 및 D3D12 장치 생성이 가능한 그래픽 환경
- C++ 데스크톱 빌드 도구와 MSVC `v145`를 사용할 수 있는 Visual Studio/MSBuild
- Windows SDK 10.0 계열
- C++20 지원

외부 패키지 복원 설정은 없으며, 프로젝트에서 `d3d12.lib`, `dxgi.lib`, `d3dcompiler.lib`를 링크합니다. 솔루션 파일은 `.slnx` 형식입니다.

### 빌드 절차

1. `D3D12.slnx`를 열고 `D3D12` 프로젝트를 선택합니다. 솔루션 형식을 지원하지 않는 환경에서는 `D3D12/D3D12.vcxproj`를 직접 엽니다.
2. 우선 `Debug | x64` 구성을 선택해 빌드합니다.
3. 실행 파일 옆의 `Renderer/Shader/Triangle.hlsl` 존재 여부를 확인한 뒤 실행합니다.

Developer PowerShell에서 프로젝트를 직접 빌드하는 명령 예시:

```powershell
msbuild .\D3D12\D3D12.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

셰이더 로더는 현재 작업 폴더가 아닌 **실행 파일 위치**를 기준으로 다음 경로를 읽습니다.

```text
<실행 파일 폴더>/
  D3D12.exe
  Renderer/Shader/Triangle.hlsl
```

프로젝트에는 HLSL의 `CopyToOutputDirectory=PreserveNewest` 메타데이터가 있습니다. 실제 출력 폴더에 파일이 없으면 위 구조로 복사해야 합니다. 셰이더 컴파일 오류 메시지는 `OutputDebugStringA`로 전달합니다.

### 조작

| 입력 | 동작 |
| --- | --- |
| W / S | 카메라 방향 기준 수평 전진 / 후진 |
| A / D | 수평 좌 / 우 이동 |
| Q / E | 아래 / 위 이동 |
| 마우스 오른쪽 버튼 + 이동 | Yaw / Pitch 회전 |
| 창 닫기 | 프로그램 종료 |

이동 키는 우클릭을 누르지 않아도 적용됩니다. 초기 카메라 위치는 `(0, 0, -6)`, 수직 시야각은 60도, Near/Far는 0.1/100입니다. 이동 속도는 초당 4 단위이며 대각선 이동 벡터를 정규화합니다.

## 코드 분석에서 확인한 한계

| 항목 | 현재 코드와 영향 |
| --- | --- |
| 와이어프레임 설정 | `CUBE_PSO::initialize()`에서 `_wireframe_pipeline`도 `D3D12_FILL_MODE_SOLID`로 생성합니다. 검은색 두 번째 그리기가 면을 다시 채우므로, 코드상 흰 면 위의 검은 선 표현이 되지 않습니다. |
| 깊이 처리 | Depth/Stencil이 꺼져 있고 DSV가 없습니다. 가려진 면을 깊이로 제거하지 않으며 Cull Mode도 `NONE`입니다. |
| 창 크기 변경 | `WM_SIZE`와 `ResizeBuffers` 처리가 없습니다. Viewport, Scissor, 카메라 종횡비가 1280 × 720 기준으로 고정됩니다. |
| 프레임 병렬성 | 백버퍼는 2개지만 매 제출 후 GPU 완료를 기다립니다. 프레임별 allocator/상수 버퍼를 사용하는 구조는 아직 없습니다. |
| 입력 포커스 | `GetAsyncKeyState`를 사용하며 활성 창 여부를 확인하지 않습니다. 포커스 상실과 마우스 캡처 상실에 대한 별도 처리도 없습니다. |
| 오류 처리 | 초기화 단계는 주로 `bool`로 실패를 반환합니다. 프레임 중 `Reset`, `Close`, `Present`, `Signal` 등의 실패 처리와 Device Lost 복구가 없습니다. |
| 빌드 구성 | Debug/Release와 Win32/x64 구성이 선언되어 있으나 `stdafx.cpp`에는 Debug 런타임, `_DEBUG`, `x64/Debug` 출력/PCH 경로가 구성 조건 없이 지정되어 있습니다. 다른 구성 사용 전 정리가 필요합니다. |

위 내용은 정적 분석 결과이며, 이번 문서 작성에서는 코드를 수정하거나 빌드·화면·GPU 검증을 수행하지 않았습니다. 저장소에 별도 자동 테스트 프로젝트나 CI 설정은 확인되지 않았습니다.

## 확장 계획과 참고 문서

기존 계획은 Component 기반 장면 → glTF Import/Forward 조명 → PBR/HDR → Deferred → Rendering Editor 순서입니다. 이는 구현 완료 기능 목록이 아닙니다.

현재 코드 기준으로는 와이어프레임/깊이 처리, Resize, 입력 포커스, 빌드 구성 정리를 먼저 진행하고 프레임별 리소스와 장면 구조로 확장하는 순서를 제안합니다.

- [엔진 목표와 범위](Base_MD/GDD/Game_GDD.md)
- [구현 마일스톤](Base_MD/GDD/Implemention_Plan.md)
- [계획 아키텍처](Base_MD/docs/ARCHITECTURE.md)
- [기존 진행 계획](Base_MD/docs/PLANS.md)
- [설계 결정](Base_MD/docs/DECISIONS.md)

기존 문서의 완료 상태와 구조는 실제 코드와 대조해 읽어야 합니다. 이 README는 현재 구현을 안내하고, `Base_MD`는 장기 설계의 맥락을 제공합니다.
