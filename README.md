# D3D12

C++20과 Direct3D 12로 만드는 Windows용 PBR 메시 뷰어입니다.
현재 단일 glTF 헬멧의 재질, 직접광과 IBL을 Deferred 경로로 출력하고,
Dear ImGui UI로 표시 모드, 변환, 재질과 광원을 조절합니다.

루트의 구현 안내는 이 README에서 관리합니다. 기존 PBR_IMPLEMENTATION.md와
DEFERRED_RENDERING.md의 구현 기록, 수식, 참고 링크와 검증 항목을 아래에 통합했습니다.
Base_MD는 장기 설계 자료이며 현재 구현과 다른 계획이 포함될 수 있습니다.
이 문서 정리에서는 빌드와 GPU 실행 검증을 수행하지 않았습니다.

## 목차

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

## 현재 구현

| 영역 | 구현 내용 |
| --- | --- |
| 애플리케이션 | Win32 창, 메시지 루프, 창 크기 변경, 창 모드/전체화면 토글 |
| GPU | D3D12 장치, Direct Queue, Command Allocator/List, Swap Chain, Fence |
| 메시와 재질 | 단일 glTF 헬멧, 인덱스 드로우, Base Color/Normal/Metallic-Roughness/AO/Emissive |
| PBR | GGX, Schlick Fresnel, Smith Geometry, Directional Light |
| 환경 | DDS Cubemap Skybox, SH Diffuse IBL, Prefiltered Specular IBL, BRDF LUT |
| 렌더 패스 | G-Buffer 5개 MRT와 Depth, Skybox, Deferred Lighting, UI |
| 표시 모드 | Lit, Base Color, Normal, Metallic, Roughness, AO, Emissive, Depth, Wireframe |
| 카메라 | 자유 이동, 우클릭 회전, 메시 중심 공전 토글과 거리/고도 조절 |
| UI | 패널 토글, 메시 표시, 변환, 재질 배율, 광원, 환경 강도, 노출, 텍스처 미리보기 |

현재 모델은 초기화 시 지정된 에셋을 읽습니다. 실행 중 파일 선택, 모델 교체,
여러 객체와 일반적인 다중 재질 처리는 아직 구현하지 않았습니다.
OPEN MODEL, ADD MODEL, DELETE 버튼은 해당 기능 연결 전까지 비활성 상태입니다.

## 폴더 구조와 책임

```text
README.md
UI_Reference.png               # 승인한 기본 UI 시안
D3D12.slnx
Asset/                        # glTF 메시, 재질 텍스처, Skybox/IBL DDS
D3D12/
  D3D12.vcxproj
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
    Texture.*                 # 재질 텍스처와 GPU 리소스
  UI/
    ViewerUi.*                # ImGui 수명, Win32/DX12 연결과 UI 렌더
    ViewerPanels.*            # 패널 배치와 조작 위젯
    UiTheme.*                 # 색상과 스타일
  ThirdParty/ImGui/            # Dear ImGui v1.91.9b 및 백엔드
Base_MD/                      # 장기 설계와 개발 계획
```

UI 위젯은 UI 폴더에서 관리하고 Renderer에 섞지 않습니다.
Renderer는 UI가 수정한 설정을 렌더링에 연결합니다.
입력, 카메라, 광원, 셰이더 관리에는 기존 싱글턴 구조를 사용합니다.
향후 모델 Import, Scene 객체, 공유 GPU 리소스는 각각의 책임으로 분리합니다.

## 실행 흐름

1. main에서 Renderer를 생성하고 창, 장치, 명령 객체, Swap Chain, Depth, 패스와 에셋을 초기화합니다.
2. UI 프레임을 시작하고 UI의 입력 점유 상태를 InputManager에 전달합니다.
3. 카메라와 광원을 갱신하고 객체 변환, 재질, 조명 설정을 상수 버퍼에 반영합니다.
4. G-Buffer에 불투명 메시의 표면 정보를 기록합니다.
5. Back Buffer에 Skybox를 출력하고 Deferred Light Pass에서 표면 조명을 합성합니다.
6. UI를 마지막에 그린 뒤 Back Buffer를 PRESENT로 전환하고 제출/표시/Fence 대기를 수행합니다.
7. 종료 시 GPU 작업 완료를 기다린 후 UI와 GPU 리소스를 정리합니다.

CPU 행 벡터와 HLSL row_major 규약을 사용합니다. 월드 변환과 View/Projection을 적용하고,
비균일 스케일의 법선은 역전치에 해당하는 별도 행렬로 변환합니다.

## 빌드와 실행

- Windows, D3D12 지원 그래픽 환경, Windows SDK 10.0 계열
- C++20 및 MSVC v145 빌드 도구
- Rider에서 Visual Studio/MSBuild 도구 체인을 사용하거나 Visual Studio에서 프로젝트 열기
- 우선 Debug | x64 구성을 사용하며 빌드/실행 확인은 사용자가 수행합니다.

D3D12.slnx 또는 D3D12/D3D12.vcxproj를 엽니다.
Developer PowerShell의 빌드 명령 예시:

```powershell
msbuild .\D3D12\D3D12.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

주요 링크 라이브러리는 d3d12, dxgi, d3dcompiler, windowscodecs, dwmapi, imm32입니다.
ImGui 소스는 저장소에 포함되어 있으며 별도 패키지 복원 없이 프로젝트에서 컴파일합니다.
자체 C++ 파일의 첫 PCH include와 프로젝트의 PrecompiledHeaderFile 경로를 맞춥니다.
ImGui 외부 소스는 PCH를 사용하지 않습니다.

런타임 셰이더는 D3DCompileFromFile로 컴파일합니다.
실행 파일 기준 Renderer/Shader의 HLSL/HLSLI와 Asset의 glTF, BIN, 이미지, DDS가 필요합니다.
프로젝트에는 에셋/셰이더의 PreserveNewest 복사 메타데이터가 있으므로
실제 출력 폴더에도 상대 경로와 파일이 유지되는지 확인합니다.
셰이더 오류는 디버그 출력으로 확인합니다.

## 조작과 UI

| 입력/패널 | 동작 |
| --- | --- |
| W/S, A/D, Q/E | 자유 카메라 전후, 좌우, 상하 이동 |
| 마우스 오른쪽 버튼 + 이동 | 마우스 중앙 고정 상태에서 카메라 회전 |
| R | 메시 중심 자동 공전 켜기/끄기 |
| 공전 중 W/S | 메시와 거리 좁히기/넓히기 |
| 공전 중 Q/E | 메시를 바라보며 고도 조절, 최대 ±45도 |
| L | Directional Light 회전 켜기/끄기 |
| 숫자 1 | 창 모드/전체화면 전환 |
| SCENE | 현재 메시 선택과 표시 토글 |
| INSPECTOR | 위치/회전/스케일 및 재질 배율 |
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

다음 구현은 OPEN MODEL 버튼을 통한 런타임 모델 불러오기입니다.

1. UI 파일 선택으로 glTF 경로를 받고 Import 요청을 전달합니다.
2. 모델과 재질을 임시 로드하고, 성공하면 GPU 사용 완료를 보장한 뒤 기존 모델을 교체합니다.
3. 취소/실패 시 기존 모델을 유지하고 오류를 알립니다. 성공 시 이름, 통계, 미리보기와 카메라 타깃을 갱신합니다.
4. 이후 ADD MODEL과 Scene 객체 목록으로 여러 객체 렌더링을 구현합니다.
5. Mesh/Material/Texture 공유 리소스와 객체별 Transform을 분리하고 캐시/수명 관리를 정리합니다.

실제 Import 지원 범위는 구현 시 명시합니다. glTF 파일 선택 UI만 붙였다고
다중 primitive, 다중 material, GLB, 스킨/애니메이션까지 지원하는 것은 아닙니다.

남은 기술 과제:

- 일반적인 glTF 구조와 다중 재질, 투명 메시 처리
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
투명 메시, 그림자, Point/Spot Light, 일반적인 다중 재질/다중 메시 처리는 이번 범위에 포함하지 않습니다.

### 실행 확인 항목

- 첫 화면에서 헬멧의 재질, Normal 디테일, 발광, 환경 반사가 보이는지.
- 헬멧 바깥에 검은 사각형 없이 Skybox가 보이는지.
- R 공전과 L 광원 회전 중 조명이 표면에 맞게 변하는지.
- 숫자 1을 여러 번 누르거나 창을 최대화/복원해도 비율과 타깃 크기가 맞는지.
- Debug Layer에 리소스 상태, Descriptor, PSO 포맷 불일치가 없는지.

## 장기 설계 문서

- [엔진 목표와 범위](Base_MD/GDD/Game_GDD.md)
- [구현 마일스톤](Base_MD/GDD/Implemention_Plan.md)
- [계획 아키텍처](Base_MD/docs/ARCHITECTURE.md)
- [기존 진행 계획](Base_MD/docs/PLANS.md)
- [설계 결정](Base_MD/docs/DECISIONS.md)

Base_MD 내부 문서는 이번 통합에서 변경하지 않았습니다.
기존 Application/RHI/Scene/Component 계획과 실제 구현을 구분해 읽습니다.
앞으로 루트에 기능별 MD를 추가하는 대신 이 README의 관련 절을 갱신합니다.
