# PBR Implementation

이 문서는 DirectX 12 렌더러의 Physically Based Rendering 구현 기능과
`Physically Based Rendering: From Theory to Implementation`의 관련 내용을 연결해 기록합니다.

참고 도서: [Physically Based Rendering, 4th Edition](https://www.pbr-book.org/4ed/contents)

## 구현 원칙

- PBR은 조명 모델과 재질 모델을 물리 기반으로 계산합니다.
- 재질 값은 가능한 한 선형 색상 공간에서 계산합니다.
- 직접광과 환경광을 분리해 구현합니다.
- 수식의 각 항을 독립적인 함수로 구현해 디버깅과 검증이 가능하도록 합니다.
- glTF의 Metallic-Roughness 재질 규약을 엔진 Material 구조에 연결합니다.

## 기능과 참고 내용

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

## 현재 Shader 구조

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

## glTF Material 연결 계획

현재 `DamagedHelmet` 에셋에는 다음 Texture가 있습니다.

```text
BaseColor          -> Albedo
Default_normal     -> Tangent Space Normal
Default_metalRoughness
    R              -> Ambient Occlusion
    G              -> Roughness
    B              -> Metallic
Default_AO         -> Ambient Occlusion
Default_emissive   -> Emissive
```

실제 glTF 규약을 우선하며, ORM Texture가 존재하면 채널을 분리해 사용합니다.
AO Texture가 별도로 존재하는 경우에는 glTF Material의 Occlusion Strength를 적용합니다.

## 구현 단계

### PBR-01. 직접광 기반 BRDF

- Albedo Texture
- Vertex Normal
- Directional Light
- GGX D
- Schlick F
- Smith G
- glTF Material의 Metallic / Roughness factor

완료 기준: Helmet Mesh에 금속성 하이라이트와 Roughness 변화가 나타납니다.

### PBR-02. glTF Material Texture

- Metallic-Roughness Texture Import 및 glTF factor 연결
- AO Texture Import
- Emissive Texture Import
- SRV Descriptor Table 확장
- Material Constant Buffer 정리

완료 기준: 셰이더 상수가 아닌 glTF Texture 채널이 Material 결과를 결정합니다.

### PBR-03. Tangent Space Normal

- glTF Tangent Attribute 지원
- Tangent / Bitangent / Normal TBN 구성
- Normal Map Decode
- Shading Normal 적용

완료 기준: Normal Texture의 표면 디테일이 조명에 반영됩니다.

### PBR-04. Skybox와 IBL

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

### PBR-05. HDR 출력

- HDR Render Target
- Linear / sRGB 변환 확인
- Tone Mapping
- Exposure 조절

완료 기준: 밝은 반사 영역이 클리핑되지 않고 안정적으로 화면에 출력됩니다.

## 검증 항목

- Metallic을 `0.0`으로 설정했을 때 비금속 재질의 Diffuse가 유지되는가
- Metallic을 `1.0`으로 설정했을 때 Diffuse가 사라지고 Specular 색상이 Albedo에 가까워지는가
- Roughness가 낮을수록 하이라이트가 좁고 강해지는가
- Roughness가 높을수록 하이라이트가 넓고 약해지는가
- NdotL이 0 이하일 때 직접광이 출력되지 않는가
- Fresnel에 의해 grazing angle에서 반사율이 증가하는가
- Normal Map을 적용해도 표면 실루엣은 변하지 않는가
- HDR 환경맵을 적용했을 때 금속 재질에 환경이 반사되는가
- Linear 색상 계산 후 최종 출력에서만 sRGB 변환이 적용되는가

## 참고

- [PBR Book Contents](https://www.pbr-book.org/4ed/contents)
- [Reflection Models](https://www.pbr-book.org/4ed/Reflection_Models)
- [Textures and Materials](https://www.pbr-book.org/4ed/Textures_and_Materials)
- [Light Sources](https://www.pbr-book.org/4ed/Light_Sources)
- [Light Transport](https://www.pbr-book.org/4ed/Light_Transport_I_Surface_Reflection)
