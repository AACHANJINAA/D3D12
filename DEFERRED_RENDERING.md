# Deferred Rendering

## 구현 범위

현재 불투명 glTF 메시는 G-Buffer에 한 번 그린 후 Deferred Light Pass에서
Directional Light, SH Diffuse IBL, Prefiltered Specular IBL, BRDF LUT 및
Emissive를 합성합니다. 기존 카메라 조작과 L 광원 회전 입력을 사용합니다.
소스 연결을 완료했으며 빌드와 GPU 실행 검증은 아직 수행하지 않았습니다.

## 패스 구성

1. GBufferRenderPass: 재질 텍스처와 Normal Map을 읽고 5개의 MRT와 Depth 기록.
2. SkyboxRenderPass: Back Buffer에 환경 배경 출력.
3. DeferredLightPass: 화면 전체 삼각형으로 G-Buffer를 읽고 PBR 조명 출력.
4. Present: Back Buffer를 PRESENT 상태로 전환.

Renderer는 패스를 조립합니다. GBufferRenderPass가 타깃, RTV, 상태 전환과
메시 Draw를 소유하며 DeferredLightPass는 G-Buffer/IBL SRV Heap과 조명 PSO를 소유합니다.
현재 불투명 메시에 Forward Draw와 검은색 추가 Draw를 중복 실행하지 않습니다.
기존 MeshRenderPass 소스는 남겨 두었지만 기본 렌더 경로에서 호출하지 않습니다.

## 저장 데이터

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

## 셰이더와 수명

- GBuffer.hlsl: 재질 데이터 기록.
- DeferredLight.hlsl: G-Buffer 샘플링과 배경 판정.
- PbrCommon.hlsli: 기존 Forward와 공유하는 상수 버퍼, BRDF, SH, IBL 계산.
- Mesh.hlsl: 기존 메시 VS와 Forward PS.
- 각 프레임에 G-Buffer 상태를 PIXEL_SHADER_RESOURCE -> RENDER_TARGET ->
  PIXEL_SHADER_RESOURCE로 전환합니다.
- 숫자 1 전체화면 전환과 일반 창 크기 변경 시 GPU 완료를 기다리고
  Back Buffer, Depth, G-Buffer 및 Deferred SRV를 다시 만듭니다.
- 최소화 중에는 렌더링을 쉬고, 리사이즈 실패 시 유효하지 않은 타깃을 사용하지 않습니다.

## 적용 근거와 한계

| 구현 기능 | 참고 자료 | 적용 내용 |
| --- | --- | --- |
| RTV 기록 후 SRV 읽기 | [Microsoft Resource Barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12) | 읽기/쓰기 용도에 맞는 명시적 상태 전환 |
| 표면 조명 계산 분리 | [PBRT Surface Reflection](https://www.pbr-book.org/4ed/Light_Transport_I_Surface_Reflection) | 표면 정보로 반사광과 발광을 계산하는 개념 |

Deferred 패스 분리는 엔진 설계이며 PBRT의 구현을 그대로 이식한 것은 아닙니다.
이번 변경은 기존 PBR 수식과 고정 SH 환경 계수를 재사용합니다.
Tone Mapping과 Gamma는 조명 PS에서 수행하며 별도 HDR 후처리 타깃은 아직 없습니다.
투명 메시, 그림자, Point/Spot Light, 일반적인 다중 재질/다중 메시 처리는 이번 범위에 포함하지 않습니다.

## 실행 확인 항목

- 첫 화면에서 헬멧의 재질, Normal 디테일, 발광, 환경 반사가 보이는지.
- 헬멧 바깥에 검은 사각형 없이 Skybox가 보이는지.
- R 공전과 L 광원 회전 중 조명이 표면에 맞게 변하는지.
- 숫자 1을 여러 번 누르거나 창을 최대화/복원해도 비율과 타깃 크기가 맞는지.
- Debug Layer에 리소스 상태, Descriptor, PSO 포맷 불일치가 없는지.
