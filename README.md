# D3D12

C++20과 Direct3D 12로 만드는 Windows용 PBR 메시 뷰어입니다.
빈 장면으로 시작하여 OPEN MODEL/ADD MODEL로 선택한 glTF 모델들의 재질, 직접광과 IBL을 Deferred 경로로 출력하고,
Dear ImGui UI로 표시 모드, 변환, 재질과 광원을 조절합니다.

루트의 구현 안내는 이 README에서 관리합니다. 기존 PBR_IMPLEMENTATION.md와
DEFERRED_RENDERING.md의 구현 기록, 수식, 참고 링크와 검증 항목을 아래에 통합했습니다.
Base_MD는 장기 설계 자료이며 현재 구현과 다른 계획이 포함될 수 있습니다.
두 실행 프로젝트로 분리했습니다. 아래 기존 기능 설명은 별도 표시가 없으면 Deffered 기준입니다.

- `Deffered`: 현재 개발 중인 Deferred PBR 뷰어. 요청한 폴더 철자를 유지합니다.
- `Forward`: PIP S.T.L의 원본 glTF 셰이더를 사용하는 독립적인 정적 메시 Forward 포팅본.
- `Asset`, `Benchmark`: 공유 자산과 동일 장면 입력. 게임 로직은 비교 대상에서 제외합니다.

Forward는 S.T.L 전체 렌더러의 무수정 복사본이 아닙니다. 인스턴싱, 컬링, 프레임 Fence와
DEFAULT 메시 버퍼를 연결했지만 로더와 프레임 자원 관리 구현 등은 다릅니다.
[포팅 범위와 차이](Forward/PipReference/PORTING_NOTES.md)에 원본 커밋, 보존 항목과 비교 제한을 기록했습니다.
화면 동등성과 성능 우위는 아직 검증하지 않았습니다.

## 목차

- [주 개발 목표](#주-개발-목표)
- [현재 구현](#현재-구현)
- [폴더 구조와 책임](#폴더-구조와-책임)
- [실행 흐름](#실행-흐름)
- [빌드와 실행](#빌드와-실행)
- [조작과 UI](#조작과-ui)
- [코드 컨벤션](#코드-컨벤션)
- [다음 작업과 남은 과제](#다음-작업과-남은-과제)
- [PBR 구현 기록](#pbr-구현-기록)
- [Deferred 구현 기록](#deferred-구현-기록)
- [장기 설계 문서](#장기-설계-문서)

## 주 개발 목표

**PIP S.T.L 렌더러의 병목을 분석하고 동일 조건에서 CPU/GPU 시간과 메모리 개선을 수치로 입증합니다.**
향후 개발 우선순위와 완료 기준은 [S.T.L 대비 성능 검증 계획](Base_MD/docs/RENDERER_BENCHMARK_PLAN.md)을 따릅니다.
현재 성능 우위는 미검증이며, Deferred 구현 자체를 성능 향상으로 취급하지 않습니다.

진행 순서는 계측과 기준선 구축 -> 프레임 동기화 -> 인스턴싱/컬링 -> 다중 광원 비교 ->
G-buffer 최적화 -> Tiled Deferred -> 포트폴리오 결과 정리입니다.
빌드는 요청에 따라 확인하며, 미측정 값과 이론적 감소량을 실측 결과와 구별합니다.

## 현재 구현

| 영역 | 구현 내용 |
| --- | --- |
| 애플리케이션 | Win32 창, 메시지 루프, 창 크기 변경, 창 모드/전체화면 토글 |
| GPU | D3D12 장치, Direct Queue, Command Allocator/List, Swap Chain, Fence |
| 메시와 재질 | 런타임 glTF 선택/교체, 인덱스 드로우, Base Color/Normal/Metallic-Roughness/AO/Emissive |
| PBR | GGX, Schlick Fresnel, Smith Geometry, Directional Light |
| 환경 | DDS Cubemap Skybox, SH Diffuse IBL, Prefiltered Specular IBL, BRDF LUT |
| 렌더 패스 | G-Buffer 5개 MRT와 Depth, Skybox, Deferred Lighting, UI |
| 표시 모드 | Lit, Base Color, Normal, Metallic, Roughness, AO, Emissive, Depth, Wireframe |
| 카메라 | 자유 이동, 우클릭 회전, 메시 중심 공전 토글과 거리/고도 조절 |
| UI | 패널 토글, 메시 표시, 변환, 재질 배율, 광원, 환경 강도, 노출, 텍스처 미리보기 |

시작 시 고정 메시를 읽지 않으며 객체 수는 0입니다. OPEN MODEL은 파일 선택과 모델 교체에 연결했습니다.
취소/Import 실패 시 기존 모델을 유지하고, 성공 시 이름/통계/미리보기와 카메라를 갱신합니다.
다중 노드/primitive/불투명 재질과 여러 Scene 객체를 지원합니다.
OPEN MODEL은 선택한 객체를 교체하고, ADD MODEL은 기존 객체를 유지하며 추가합니다.
선택 객체가 없으면 OPEN MODEL도 새 객체를 만듭니다. DELETE는 선택한 객체만 삭제합니다.

## 폴더 구조와 책임

```text
README.md
UI_Reference.png               # 승인한 기본 UI 시안
D3D12.slnx
Asset/                        # glTF 메시, 재질 텍스처, Skybox/IBL DDS
Benchmark/                    # 두 프로젝트가 읽는 공통 장면 JSON과 설정 로더
Forward/
  Forward.vcxproj
  Renderer/Pass/PipForwardPass.* # 원본 셰이더 바인딩, 인스턴싱과 컬링
  Renderer/Shader/Pip/         # PIP 원본 HLSL 4개
  PipReference/               # 원본 C++ 참조본, 해시와 포팅 차이 기록
  Core/, Common/, UI/, Scene/, Resource/ # 독립 실행용 뷰어 코드
Deffered/
  Deffered.vcxproj
  Core/main.cpp               # 진입점
  Common/
    stdafx.h / stdafx.cpp     # 공통 헤더, namespace 정리와 PCH
    Math.h                    # 자체 벡터/행렬 연산
    D3DX12.h                  # 프로젝트 D3D12 보조 구조체
  Renderer/
    Renderer.*                # 초기화, 패스 조립, 프레임 제출과 종료
    ViewerSettings.h          # UI와 렌더러가 공유하는 설정
    Manager/
      CameraManager.*         # 카메라와 공전
      InputManager.*          # 입력, 마우스 캡처, UI 입력 차단
      LightManager.*          # 광원 상태와 회전
      ResourceManager.h       # 모델/이미지 weak 캐시, 장치 수명에 맞춰 Renderer가 소유
    SceneRenderData.*         # 객체·primitive별 상수 버퍼와 Draw 목록
    Pass/
      GBufferRenderPass.*     # 재질 MRT, 깊이와 메시 드로우
      SkyboxRenderPass.*      # 환경 배경과 환경 리소스
      DeferredLightPass.*     # G-Buffer와 IBL을 이용한 조명
      MeshRenderPass.*        # 기존 Forward 경로
    Shader/
      Shader.*                # 셰이더 관리와 런타임 컴파일
      Mesh.hlsl
      GBuffer.hlsl
      DeferredLight.hlsl
      Skybox.hlsl
      PbrCommon.hlsli         # 공통 재질/BRDF/IBL 계산
  Resource/
    GltfMesh.*                # glTF 메시 데이터
    ModelResource.*           # 선택한 모델의 메시/재질 텍스처/정점·인덱스 GPU 리소스
    MeshResource.*            # 공유 정점/인덱스 버퍼
    MaterialResource.h        # 공유 재질 파라미터와 SRV 집합
    ResourceCache.h           # 정규화 경로 기반 weak 캐시
    Texture.*                 # 재질 텍스처와 GPU 리소스
  UI/
    ViewerUi.*                # ImGui 수명, Win32/DX12 연결과 UI 렌더
    ViewerPanels.*            # 패널 배치와 조작 위젯
    UiTheme.*                 # 색상과 스타일
    ModelFileDialog.*         # Windows 파일 선택과 Import 오류 표시
  Scene/Scene.*               # 안정적인 객체 ID, 선택, 변환, 표시, 객체별 재질 오버라이드
  ThirdParty/ImGui/            # Dear ImGui v1.91.9b 및 백엔드
Base_MD/                      # 장기 설계와 개발 계획
Tests/GltfImportTests.cpp      # Import CPU 회귀 테스트 소스
```

UI 위젯은 UI 폴더에서 관리하고 Renderer에 섞지 않습니다.
Renderer는 UI가 수정한 설정을 렌더링에 연결합니다.
입력, 카메라, 광원, 셰이더 관리에는 기존 싱글턴 구조를 사용합니다.
모델 Import, Scene 객체, 공유 GPU 리소스를 각각 분리했습니다.
ResourceManager는 전역 싱글턴이 아닌 Renderer 소유 객체로 두어 GPU 장치보다 오래 남지 않도록 합니다.

## 실행 흐름

1. main에서 창, 장치, 명령 객체, Swap Chain, Depth, 패스와 환경 리소스를 초기화합니다. 모델은 비워 둡니다.
2. UI 프레임을 시작하고 UI의 입력 점유 상태를 InputManager에 전달합니다.
3. 카메라와 광원을 갱신하고 객체 변환, 재질, 조명 설정을 상수 버퍼에 반영합니다.
4. G-Buffer/Depth를 한 번 지우고, 표시 중인 모든 객체의 primitive를 재질별로 기록합니다.
5. Back Buffer에 Skybox를 출력하고 Deferred Light Pass에서 표면 조명을 합성합니다.
6. UI를 마지막에 그린 뒤 Back Buffer를 PRESENT로 전환하고 제출/표시/Fence 대기를 수행합니다.
7. 종료 시 GPU 작업 완료를 기다린 후 UI와 GPU 리소스를 정리합니다.

CPU 행 벡터와 HLSL row_major 규약을 사용합니다. 월드 변환과 View/Projection을 적용하고,
비균일 스케일의 법선은 역전치에 해당하는 별도 행렬로 변환합니다.

## 빌드와 실행

- Windows, D3D12 지원 그래픽 환경, Windows SDK 10.0 계열
- C++20 및 MSVC v145 빌드 도구
- Rider에서 Visual Studio/MSBuild 도구 체인을 사용하거나 Visual Studio에서 프로젝트 열기
- 정확성 점검은 Debug | x64, 성능 측정은 Release | x64 구성을 사용합니다.

D3D12.slnx를 열고 Rider 실행 프로젝트를 Deffered 또는 Forward로 선택합니다.
Developer PowerShell의 빌드 명령 예시:

```powershell
msbuild .\D3D12.slnx /m /p:Configuration=Debug /p:Platform=x64
msbuild .\D3D12.slnx /m /p:Configuration=Release /p:Platform=x64
```

실행 파일과 중간 산출물은 각 프로젝트의 `bin/<Platform>/<Configuration>`과
`obj/<Platform>/<Configuration>`에 분리합니다. 셰이더와 환경 DDS는 빌드 후 자동 복사합니다.
이전 루트 `x64` 산출물은 보존했지만 더 이상 새 빌드의 실행 경로가 아닙니다.

공통 장면 테스트는 저장소 루트에서 아래 명령을 **각각 따로** 실행합니다.

```powershell
& .\Forward\bin\x64\Release\Forward.exe --benchmark .\Benchmark\static-grid.json
& .\Deffered\bin\x64\Release\Deffered.exe --benchmark .\Benchmark\static-grid.json
```

동일 모델 6개, 변환, 카메라, 광원, 해상도와 VSync를 JSON으로 고정하고 UI/선택 효과를 끕니다.
일반 실행은 두 프로젝트 모두 빈 장면에서 모델을 선택합니다.
아직 두 경로의 BRDF/샘플러/출력 품질이 같지 않으므로 이 장면만으로 성능 우열을 주장하지 않습니다.
GPU 계측과 CSV 수집은 다음 단계입니다.

2026-10-02 검증: 두 프로젝트 Debug/Release x64 빌드와 HLSL 진입점 12개 컴파일 성공.
PIP 원본 HLSL 경고는 포팅 기록에 남겼습니다. 앱 실행/GPU 검증/성능 측정은 미실행이며 화면 캡처는 하지 않았습니다.

주요 링크 라이브러리는 d3d12, dxgi, d3dcompiler, windowscodecs, dwmapi, imm32, comdlg32, windowsapp입니다.
Import JSON 파서는 Windows SDK의 C++/WinRT Windows.Data.Json을 사용합니다. 별도 외부 파서 다운로드는 필요 없습니다.
ImGui 소스는 저장소에 포함되어 있으며 별도 패키지 복원 없이 프로젝트에서 컴파일합니다.
자체 C++ 파일의 첫 PCH include와 프로젝트의 PrecompiledHeaderFile 경로를 맞춥니다.
ImGui 외부 소스는 PCH를 사용하지 않습니다.

런타임 셰이더는 D3DCompileFromFile로 컴파일합니다.
실행 파일 기준 Renderer/Shader의 HLSL/HLSLI와 Asset의 환경 DDS가 필요합니다.
모델 glTF/BIN/이미지는 OPEN MODEL로 선택한 파일 위치에서 상대 URI로 읽습니다.
헬멧 파일이 없어도 뷰어가 시작되며, 헬멧 파일을 출력 폴더로 복사하는 프로젝트 항목도 제거했습니다.
프로젝트에는 에셋/셰이더의 PreserveNewest 복사 메타데이터가 있으므로
실제 출력 폴더에도 상대 경로와 파일이 유지되는지 확인합니다.
셰이더 오류는 디버그 출력으로 확인합니다.

## 조작과 UI

| 입력/패널 | 동작 |
| --- | --- |
| OPEN MODEL | .gltf 파일 선택, 선택 객체 교체(다른 객체는 유지), 선택이 없으면 추가 |
| ADD MODEL | 기존 객체를 유지한 채 추가, 선택 객체 옆에 배치하고 새 객체 선택 |
| W/S, A/D, Q/E | 자유 카메라 전후, 좌우, 상하 이동 |
| 마우스 오른쪽 버튼 + 이동 | 마우스 중앙 고정 상태에서 카메라 회전 |
| R | 메시 중심 자동 공전 켜기/끄기 |
| 공전 중 W/S | 메시와 거리 좁히기/넓히기 |
| 공전 중 Q/E | 메시를 바라보며 고도 조절, 최대 ±45도 |
| L | Directional Light 회전 켜기/끄기 |
| 숫자 1 | 창 모드/전체화면 전환 |
| SCENE | 객체 목록 선택, 행별 표시 체크박스, FOCUS, DELETE |
| INSPECTOR | 선택 객체 위치/회전/스케일, 재질 선택과 해당 재질의 배율 |
| DISPLAY | 9가지 표시 모드, Skybox 토글, Depth 범위 |
| LIGHT | 방향, 색상, 강도, 환경 강도, 노출과 광원 회전 |
| ASSETS | 모델 정보와 재질 텍스처 미리보기 |
| STATUS | FPS, 표시 객체 수와 삼각형 수 |

상단 패널 버튼으로 각각 켜고 끄며 X로 닫은 패널도 다시 열 수 있습니다.
HIDE ALL/RESTORE로 패널 표시 상태를 유지한 채 전체를 숨기고 복원합니다.
UI 입력 중에는 카메라 입력이 충돌하지 않도록 차단합니다.
LIGHT의 숫자는 슬라이더 바 왼쪽에 따로 표시합니다.

## 코드 컨벤션

- 클래스 이름은 모두 대문자: RENDERER, CAMERA_MANAGER.
- 일반 변수는 소문자이며 복합어는 snake_case를 사용합니다.
- bool 변수는 is 접두사를 사용하고 is 바로 뒤에 밑줄을 붙이지 않습니다.
- 멤버 변수에는 _ 접두사를 붙입니다.
- 함수는 소문자이며 복합어를 밑줄로 구분합니다: create_pipeline, render_frame.
- Manager 파일 이름에는 Manager 접미사를 사용합니다: CameraManager.h.
- 반복 namespace와 공통 include는 stdafx.h에서 정리합니다.
- 기본 수학 연산은 Common/Math.h의 자체 구현을 사용합니다.
- 외부 라이브러리 소스는 원래 컨벤션을 유지합니다.
- 위 규칙은 작성 기준이며 기존 코드의 불일치를 이 문서 작업에서 일괄 수정하지 않습니다.

## 다음 작업과 남은 과제

다중 primitive/material, ADD MODEL과 Scene 목록, 공유 리소스와 객체별 설정을 연결했습니다.
다중 객체의 깊이/재질/삭제 정확성을 확인하면서, 다음 주 작업은
[B0: 계측 도구와 비교 기준선 구축](Base_MD/docs/RENDERER_BENCHMARK_PLAN.md)으로 진행합니다.
GLB, 투명 재질 등 Import 확장은 비교 실험에 필요한 경우를 제외하고 후순위로 둡니다.

### 현재 Import 범위

- glTF 2.0 JSON 파일, 기본 scene의 여러 mesh node와 여러 TRIANGLES primitive/불투명 재질
- primitive가 참조하는 재질과 texture/image 인덱스 사용, 파일명이나 accessor 순서에 의존하지 않음
- 외부 로컬 BIN/이미지 URI, 상대 경로와 percent escape, 유니코드 파일 경로
- accessor/bufferView offset과 interleaved stride, unsigned 8/16/32-bit 인덱스, 인덱스 생략
- FLOAT POSITION/NORMAL/TANGENT, FLOAT 또는 normalized unsigned 8/16-bit TEXCOORD_0
- 노드 matrix/TRS와 부모 변환 적용, RH에서 LH로 Z 반전 및 winding/tangent 부호 보정
- 메시 geometry는 한 번 디코딩하고 여러 노드가 같은 정점/인덱스 범위를 공유
- 노드별 변환을 보존하고 Draw에서 객체 Transform과 합성, 음수 스케일의 tangent 부호 및 역전치 법선 처리
- NORMAL이 없으면 flat normal 생성, TANGENT가 없으면 UV 기반 tangent 생성
- Base Color/Normal/Metallic-Roughness/AO/Emissive, normal scale 및 occlusion strength
- 없는 텍스처는 중립 기본값 사용. 지정된 텍스처 파일의 로딩 실패는 Import 실패로 처리
- 모델 bounds에 맞춘 초기 카메라 거리/공전 중심/이동 속도/Near·Far 설정

GLB, data URI, 내장 이미지, sparse accessor, 압축/확장 기능, 스킨/애니메이션,
vertex color, Alpha MASK/BLEND, TEXCOORD_1 및 texture transform은
아직 지원하지 않습니다. 해당 입력을 조용히 무시하지 않고 오류로 거절합니다.
현재 텍스처 샘플러는 repeat + linear, 단일 mip입니다. 다른 wrapping은 거절하며
glTF 필터 설정과 mip 체인을 완전히 재현하지는 않습니다.
이전 헬멧 전용 추가 회전을 제거했으므로 정면 방향은 원본 노드 변환을 따릅니다.

### Import 구조와 검증

UI의 ModelFileDialog는 경로 선택/오류 표시만, GltfMesh는 CPU 파싱/검증만,
ModelResource는 공유 MeshResource와 여러 MaterialResource를 소유합니다.
Renderer는 별도 upload command list로 후보를 준비하고 Fence 완료 후 Scene에 추가/교체합니다.
실패한 후보는 활성 모델을 덮어쓰지 않습니다. 현재 로딩은 동기 방식이므로 큰 파일은 일시적으로 UI를 멈춥니다.

Tests/GltfImportTests.cpp는 offset/stride, 재질 인덱스, 좌표 변환, 유니코드 경로,
8/16/32-bit 인덱스, 잘린 버퍼, 범위 초과 인덱스와 미지원 primitive 거절을 검사합니다.
테스트 소스만 추가했으며 이 작업에서는 빌드/실행하지 않았습니다.
x64 Developer PowerShell에서 선택적으로 실행합니다(메인 프로젝트에는 포함하지 않음).

```powershell
cl /nologo /std:c++20 /EHsc /MDd /Y- Tests\GltfImportTests.cpp Deffered\Resource\GltfMesh.cpp Deffered\Scene\Scene.cpp /Fo:Tests\ /Fe:Tests\GltfImportTests.exe /link windowsapp.lib
.\Tests\GltfImportTests.exe
```

수동 확인: 빈 시작 화면 → 파일 선택 취소 → 정상 모델 열기 → 다른 모델로 교체 →
잘못된 파일/누락된 이미지 열기 후 기존 모델 유지 → R 공전/창 크기 변경/전체화면 전환.

### 다중 객체와 공유 리소스

- SCENE_OBJECT는 공유 MODEL_RESOURCE 참조와 고유 ID, Transform, 표시 상태, 재질별 오버라이드를 보유합니다.
- 같은 모델 경로를 ADD하면 파싱/정점/인덱스/재질을 재생성하지 않습니다.
- 모델 내부의 같은 primitive를 여러 노드가 참조해도 geometry는 한 번만 저장합니다.
- 서로 다른 모델이나 재질에서 같은 이미지 경로를 참조하면 IMAGE_TEXTURE를 공유합니다.
- 이미지 자체는 공유하지만 Base Color/Emissive의 sRGB SRV와 데이터 텍스처의 UNORM SRV는 재질별로 생성합니다.
- 재질 배율은 객체별 오버라이드이므로 한 객체의 편집이 다른 인스턴스에 전파되지 않습니다.
- 캐시는 weak_ptr이고 객체/재질이 실제 소유자입니다. 마지막 참조가 사라지면 해제되며 오래된 캐시 키도 정리합니다.
- 업로드 완료 후 staging 버퍼를 해제합니다. 교체/삭제 전에는 GPU Fence 완료를 기다립니다.
- 매 프레임 GPU 대기 정책을 유지하며, SceneRenderData가 Draw별 512-byte 슬라이스에 상수를 저장합니다.
- GBufferRenderPass는 한 번 Clear한 뒤 전체 Draw 목록을 기록합니다. 객체마다 G-Buffer나 Depth를 다시 지우지 않습니다.
- UI의 Shared models/textures 수치로 재사용을 확인할 수 있습니다. 숨김은 리소스를 해제하지 않습니다.
- 객체 수 128개, 장면 전체 Draw 65,536개를 상한으로 두고 초과 요청은 기존 장면을 유지한 채 거절합니다.

캐시 경로는 Windows의 일반적인 대소문자 비구분 경로를 기준으로 정규화합니다.
디스크 변경 감지/강제 Reload, 비동기 Import, GPU instancing은 아직 없습니다.
OPEN으로 같은 파일을 다시 선택해도 활성 캐시가 있으면 재사용합니다.

추가 회귀 항목: 다중 재질 매핑, 중복 노드 geometry 공유, 부모/미러 변환, 순환 노드 거절,
객체 ID/설정 독립성, 중간 객체 삭제, 마지막 참조 해제와 객체 수 제한.
이 테스트들은 소스만 확장했으며 이번 작업에서 빌드하거나 실행하지 않았습니다.

실행 확인 순서: 같은 glTF를 두 번 ADD → Shared models가 1인지 확인 → 한 객체만 변환/재질 수정 →
한 객체 숨김/삭제 후 나머지 유지 → 마지막 객체 삭제 후 모델 캐시 수 0 →
서로 겹친 여러 객체의 깊이 판정 → 다중 재질 모델의 재질 선택/미리보기 확인.

구현 참고: [Windows JSON Parse](https://learn.microsoft.com/en-us/uwp/api/windows.data.json.jsonobject.parse),
[Windows 파일 선택](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-getopenfilenamew),
[glTF 2.0 규약](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

남은 기술 과제:

- GLB, sparse/내장 이미지 등 추가 glTF 구조와 투명 메시 처리
- 그림자와 Point/Spot Light
- 별도 HDR 조명 타깃과 Tone Mapping 후처리, 색상 공간 검증
- 환경맵 변경에 대응하는 SH/IBL 전처리 또는 일치하는 전처리 데이터
- 매 프레임 GPU 대기를 줄이기 위한 프레임별 allocator/상수 버퍼
- Device Lost 복구와 오류 처리 강화
- Debug x64 외 구성의 PCH/런타임/출력 경로 점검
- 자동 테스트와 Debug Layer 기반 검증

## PBR 구현 기록

이 문서는 DirectX 12 렌더러의 Physically Based Rendering 구현 기능과
`Physically Based Rendering: From Theory to Implementation`의 관련 내용을 연결해 기록합니다.

아래 완료 표기는 기존 구현 기록의 상태이며 일반 모델 지원이나 GPU 검증 완료를 의미하지 않습니다.
PBRT는 논문 모음이 아닌 도서이며, 아래 링크는 이론적 근거입니다.
Schlick Fresnel, SH와 Split-Sum IBL 등 실시간 근사는 PBRT 코드를 그대로 이식한 것이 아닙니다.

참고 도서: [Physically Based Rendering, 4th Edition](https://www.pbr-book.org/4ed/contents)

### 구현 원칙

- PBR은 조명 모델과 재질 모델을 물리 기반으로 계산합니다.
- 재질 값은 가능한 한 선형 색상 공간에서 계산합니다.
- 직접광과 환경광을 분리해 구현합니다.
- 수식의 각 항을 독립적인 함수로 구현해 디버깅과 검증이 가능하도록 합니다.
- glTF의 Metallic-Roughness 재질 규약을 엔진 Material 구조에 연결합니다.

### 기능과 참고 내용

| 엔진 구현 기능 | PBR Book 참고 내용 | 핵심 적용 내용 | 상태 |
| --- | --- | --- | --- |
| Albedo Texture | [10.4 Image Texture](https://www.pbr-book.org/4ed/Textures_and_Materials/Image_Texture) | UV를 이용한 이미지 Texture 샘플링 | 완료 |
| Vertex Normal | [3.5 Normals](https://www.pbr-book.org/4ed/Geometry_and_Transformations/Normals) | 표면 방향과 조명 입사각 계산 | 완료 |
| Directional Light | [12.3 Distant Lights](https://www.pbr-book.org/4ed/Light_Sources/Distant_Lights) | 위치와 무관한 일정 방향의 광원 | 완료 |
| Diffuse Reflection | [9.2 Diffuse Reflection](https://www.pbr-book.org/4ed/Reflection_Models/Diffuse_Reflection) | Lambert diffuse와 NdotL 적용 | 완료 |
| Cook-Torrance BRDF | [9.4 Conductor BRDF](https://www.pbr-book.org/4ed/Reflection_Models/Conductor_BRDF) | D, F, G 항을 이용한 Specular 반사 | 완료 |
| GGX NDF | [9.6 Roughness Using Microfacet Theory](https://www.pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory) | 거칠기에 따른 Microfacet Normal 분포 | 완료 |
| Schlick Fresnel | [9.4 Conductor BRDF](https://www.pbr-book.org/4ed/Reflection_Models/Conductor_BRDF) | 시선각에 따른 반사율 변화 | 완료 |
| Smith Geometry | [9.6.2 The Masking Function](https://www.pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory) | Microfacet Shadowing / Masking 보정 | 완료 |
| Metallic Workflow | [9.4 Conductor BRDF](https://www.pbr-book.org/4ed/Reflection_Models/Conductor_BRDF) | 금속의 F0와 Diffuse 비활성화 | 완료 |
| Roughness Workflow | [9.6 Roughness Using Microfacet Theory](https://www.pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory) | Roughness를 GGX alpha로 변환 | 완료 |
| Material 구조 | [10.5 Material Interface and Implementations](https://www.pbr-book.org/4ed/Textures_and_Materials/Material_Interface_and_Implementations) | Texture와 glTF 수치 파라미터를 Material로 통합 | 완료 |
| Normal Map | [3.5 Normals](https://www.pbr-book.org/4ed/Geometry_and_Transformations/Normals) | Tangent Space Normal을 Shading Normal로 변환 | 완료 |
| ORM Texture | [10.4 Image Texture](https://www.pbr-book.org/4ed/Textures_and_Materials/Image_Texture) | AO, Roughness, Metallic 채널 분리 | 완료 |
| Emissive | [4.4 Light Emission](https://www.pbr-book.org/4ed/Radiometry_Spectra_and_Color/Light_Emission) | 자체 발광 색상과 glTF factor 적용 | 완료 |
| Skybox | [12.5 Infinite Area Lights](https://www.pbr-book.org/4ed/Light_Sources/Infinite_Area_Lights) | 무한 영역 환경광과 환경 Texture | 완료 |
| IBL Diffuse | [12.5 Infinite Area Lights](https://www.pbr-book.org/4ed/Light_Sources/Infinite_Area_Lights) | Irradiance 환경광과 SH 근사 | 완료 |
| IBL Specular | [9.6 Roughness Using Microfacet Theory](https://www.pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory) | Roughness별 Prefiltered Environment | 완료 |
| BRDF LUT | [13.1 The Light Transport Equation](https://www.pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/The_Light_Transport_Equation) | Split-Sum Specular IBL 보정항 | 완료 |
| Linear Color | [4.6 Color](https://www.pbr-book.org/4ed/Radiometry_Spectra_and_Color/Color) | 색상 변환과 에너지 계산 기준 | 구현 중 |
| Tone Mapping | [5.4 Film and Imaging](https://www.pbr-book.org/4ed/Cameras_and_Film/Film_and_Imaging) | HDR 조명 결과를 화면 출력 범위로 변환 | 완료 |

### 현재 Shader 구조

현재 Pixel Shader는 다음 순서로 직접광을 계산합니다.

```text
Albedo Texture Sample
        |
        v
Normal / View / Light Direction 계산
        |
        +-- NdotL, NdotV, NdotH, VdotH
        |
        +-- GGX Distribution D
        +-- Schlick Fresnel F
        +-- Smith Geometry G
        |
        v
Cook-Torrance Specular + Diffuse
        |
        v
Ambient + Direct Lighting
```

현재 구현된 핵심 식은 다음과 같습니다.

```text
f_specular = (D * F * G) / (4 * NdotL * NdotV)

kS = F
kD = (1 - kS) * (1 - metallic)

f_diffuse = kD * albedo / PI

direct_lighting = (f_diffuse + f_specular) * radiance * NdotL
```

### glTF Material 연결

현재 `DamagedHelmet` 에셋에는 다음 Texture가 있습니다.

```text
BaseColor          -> Albedo
Default_normal     -> Tangent Space Normal
Default_metalRoughness
    R              -> Occlusion Texture가 같은 이미지를 참조할 때만 AO로 사용
    G              -> Roughness
    B              -> Metallic
Default_AO         -> Ambient Occlusion
Default_emissive   -> Emissive
```

Metallic-Roughness의 G/B 채널과 Occlusion Texture의 R 채널을 구분합니다.
AO는 glTF의 occlusionTexture 참조를 따라야 하며 Metallic-Roughness의 R을 무조건 AO로 읽지 않습니다.
Occlusion Strength 적용 여부를 포함한 일반 glTF 재질 지원은 Import 확장 시 검증합니다.

### 구현 단계와 완료 기준

#### PBR-01. 직접광 기반 BRDF

- Albedo Texture
- Vertex Normal
- Directional Light
- GGX D
- Schlick F
- Smith G
- glTF Material의 Metallic / Roughness factor

완료 기준: Helmet Mesh에 금속성 하이라이트와 Roughness 변화가 나타납니다.

#### PBR-02. glTF Material Texture

- Metallic-Roughness Texture Import 및 glTF factor 연결
- AO Texture Import
- Emissive Texture Import
- SRV Descriptor Table 확장
- Material Constant Buffer 정리

완료 기준: 셰이더 상수가 아닌 glTF Texture 채널이 Material 결과를 결정합니다.

#### PBR-03. Tangent Space Normal

- glTF Tangent Attribute 지원
- Tangent / Bitangent / Normal TBN 구성
- Normal Map Decode
- Shading Normal 적용

완료 기준: Normal Texture의 표면 디테일이 조명에 반영됩니다.

#### PBR-04. Skybox와 IBL

- Equirectangular 또는 Cubemap Texture Import
- Skybox Pass
- Diffuse Irradiance
- Prefiltered Specular Environment
- BRDF LUT

현재 Asset의 대상 파일:

```text
Asset/Skybox/cloudy/cloudy_skybox.dds
Asset/Skybox/cloudy/cloudy_specular.dds
Asset/Skybox/BRDF.dds
```

완료 기준: Directional Light가 없어도 환경광과 금속 반사가 표현됩니다.

#### PBR-05. HDR 출력 (부분 구현)

- HDR Render Target
- Linear / sRGB 변환 확인
- Tone Mapping
- Exposure 조절

완료 기준: 밝은 반사 영역이 클리핑되지 않고 안정적으로 화면에 출력됩니다.

### 검증 항목

- Metallic을 `0.0`으로 설정했을 때 비금속 재질의 Diffuse가 유지되는가
- Metallic을 `1.0`으로 설정했을 때 Diffuse가 사라지고 Specular 색상이 Albedo에 가까워지는가
- Roughness가 낮을수록 하이라이트가 좁고 강해지는가
- Roughness가 높을수록 하이라이트가 넓고 약해지는가
- NdotL이 0 이하일 때 직접광이 출력되지 않는가
- Fresnel에 의해 grazing angle에서 반사율이 증가하는가
- Normal Map을 적용해도 표면 실루엣은 변하지 않는가
- HDR 환경맵을 적용했을 때 금속 재질에 환경이 반사되는가
- Linear 색상 계산 후 최종 출력에서만 sRGB 변환이 적용되는가

### 참고

- [PBR Book Contents](https://www.pbr-book.org/4ed/contents)
- [Reflection Models](https://www.pbr-book.org/4ed/Reflection_Models)
- [Textures and Materials](https://www.pbr-book.org/4ed/Textures_and_Materials)
- [Light Sources](https://www.pbr-book.org/4ed/Light_Sources)
- [Light Transport](https://www.pbr-book.org/4ed/Light_Transport_I_Surface_Reflection)

## Deferred 구현 기록

### 구현 범위

현재 불투명 glTF 메시는 G-Buffer에 한 번 그린 후 Deferred Light Pass에서
Directional Light, SH Diffuse IBL, Prefiltered Specular IBL, BRDF LUT 및
Emissive를 합성합니다. 기존 카메라 조작과 L 광원 회전 입력을 사용합니다.
사용자가 화면 출력을 확인한 상태입니다. 전체 검증 항목은 아래 체크리스트로 별도 확인합니다.

### 패스 구성

1. GBufferRenderPass: 재질 텍스처와 Normal Map을 읽고 5개의 MRT와 Depth 기록.
2. SkyboxRenderPass: Back Buffer에 환경 배경 출력.
3. DeferredLightPass: 화면 전체 삼각형으로 G-Buffer를 읽고 PBR 조명 출력.
4. ViewerUi: 조명 결과 위에 UI 출력.
5. Present: Back Buffer를 PRESENT 상태로 전환.

Renderer는 패스를 조립합니다. GBufferRenderPass가 타깃, RTV, 상태 전환과
메시 Draw를 소유하며 DeferredLightPass는 G-Buffer/IBL SRV Heap과 조명 PSO를 소유합니다.
현재 불투명 메시에 Forward Draw와 검은색 추가 Draw를 중복 실행하지 않습니다.
기존 MeshRenderPass 소스는 남겨 두었지만 기본 렌더 경로에서 호출하지 않습니다.

### 저장 데이터

| MRT / SRV | 포맷 | 데이터 |
| --- | --- | --- |
| 0 / t0 | R8G8B8A8_UNORM | 선형 Base Color와 Alpha |
| 1 / t1 | R16G16B16A16_FLOAT | Normal Map 적용 후 월드 Normal, 0..1 인코딩 |
| 2 / t2 | R8G8B8A8_UNORM | R: Metallic, G: Roughness, B: AO |
| 3 / t3 | R16G16B16A16_FLOAT | 선형 Emissive |
| 4 / t4 | R32G32B32A32_FLOAT | 월드 위치, W: 메시가 기록되면 1 |
| DSV | D32_FLOAT | 가장 가까운 표면 판정 |

Depth에서 위치를 복원하는 대신 월드 위치를 별도 타깃에 저장합니다.
구조를 확인하기 쉬운 대신 타깃 메모리와 대역폭을 더 사용합니다.
위치 W를 0으로 Clear하고 조명 패스에서 해당 픽셀을 discard하여 배경을 보존합니다.
G-Buffer는 필터 보간 없이 Texture2D.Load로 같은 화면 좌표의 픽셀을 읽습니다.

조명 Heap의 t5는 환경 Cubemap, t6는 BRDF LUT, t7은 Prefiltered Specular Cubemap입니다.
SRV는 원본 리소스로 직접 생성하며 shader-visible Heap에서 descriptor를 복사하지 않습니다.

### 셰이더와 수명

- GBuffer.hlsl: 재질 데이터 기록.
- DeferredLight.hlsl: G-Buffer 샘플링과 배경 판정.
- PbrCommon.hlsli: 기존 Forward와 공유하는 상수 버퍼, BRDF, SH, IBL 계산.
- Mesh.hlsl: 기존 메시 VS와 Forward PS.
- 각 프레임에 G-Buffer 상태를 PIXEL_SHADER_RESOURCE -> RENDER_TARGET ->
  PIXEL_SHADER_RESOURCE로 전환합니다.
- 숫자 1 전체화면 전환과 일반 창 크기 변경 시 GPU 완료를 기다리고
  Back Buffer, Depth, G-Buffer 및 Deferred SRV를 다시 만듭니다.
- 최소화 중에는 렌더링을 쉬고, 리사이즈 실패 시 유효하지 않은 타깃을 사용하지 않습니다.

### 적용 근거와 한계

| 구현 기능 | 참고 자료 | 적용 내용 |
| --- | --- | --- |
| RTV 기록 후 SRV 읽기 | [Microsoft Resource Barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12) | 읽기/쓰기 용도에 맞는 명시적 상태 전환 |
| 표면 조명 계산 분리 | [PBRT Surface Reflection](https://www.pbr-book.org/4ed/Light_Transport_I_Surface_Reflection) | 표면 정보로 반사광과 발광을 계산하는 개념 |

Deferred 패스 분리는 엔진 설계이며 PBRT의 구현을 그대로 이식한 것은 아닙니다.
이번 변경은 기존 PBR 수식과 고정 SH 환경 계수를 재사용합니다.
Tone Mapping과 Gamma는 조명 PS에서 수행하며 별도 HDR 후처리 타깃은 아직 없습니다.
다중 재질/다중 메시 처리는 Scene Draw 목록으로 확장했습니다.
투명 메시, 그림자, Point/Spot Light는 아직 구현하지 않았습니다.

### 실행 확인 항목

- 첫 화면에서 헬멧의 재질, Normal 디테일, 발광, 환경 반사가 보이는지.
- 헬멧 바깥에 검은 사각형 없이 Skybox가 보이는지.
- R 공전과 L 광원 회전 중 조명이 표면에 맞게 변하는지.
- 숫자 1을 여러 번 누르거나 창을 최대화/복원해도 비율과 타깃 크기가 맞는지.
- Debug Layer에 리소스 상태, Descriptor, PSO 포맷 불일치가 없는지.

## 장기 설계 문서

- [주 목표: S.T.L 대비 성능 검증 계획](Base_MD/docs/RENDERER_BENCHMARK_PLAN.md)
- [엔진 목표와 범위](Base_MD/GDD/Game_GDD.md)
- [구현 마일스톤](Base_MD/GDD/Implemention_Plan.md)
- [계획 아키텍처](Base_MD/docs/ARCHITECTURE.md)
- [기존 진행 계획](Base_MD/docs/PLANS.md)
- [설계 결정](Base_MD/docs/DECISIONS.md)

2026-10-02부터 성능 검증 계획을 현재 개발 우선순위의 기준으로 사용합니다.
기존 Application/RHI/Scene/Component 계획과 실제 구현을 구분해 읽습니다.
앞으로 루트에 기능별 MD를 추가하는 대신 이 README의 관련 절을 갱신합니다.
