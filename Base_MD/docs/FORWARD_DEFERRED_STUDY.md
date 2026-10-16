# Forward와 Deferred 렌더링 학습 노트

작성 기준: 2026-10-08, 이 저장소의 현재 C++ / Direct3D 12 구현.

이 문서는 코드 리뷰나 면접용 요약이 아니라 **렌더링 원리를 차근차근 공부하기 위한 교재**입니다.
코드는 개념을 구체화하는 예시입니다. 처음에는 코드 연결 부분을 건너뛰고 흐름·그림·수식·실습에 집중해도 됩니다.
일반적인 기법, 현재 프로젝트의 구현, 아직 구현하지 않은 확장을 구분합니다.
불투명 래스터라이제이션의 기본 흐름부터 투명도·AA·광원 분류·성능 분석까지 다룹니다.
모든 상용 엔진의 특수 구현을 망라하는 문서는 아니지만, 핵심 개념이 서로 어떻게 연결되는지 빠뜨리지 않는 것을 목표로 합니다.

> 가장 중요한 문장: **Forward는 표면을 그리는 중에 조명을 계산하고, Deferred는 표면 정보를 먼저 기록한 다음 그 정보로 조명을 계산한다.**

## 읽는 순서

| 단계 | 읽을 내용 | 스스로 답할 질문 |
| --- | --- | --- |
| 1회차 | 1~5장 | 무엇을 나중으로 미루는가? |
| 2회차 | 6~10장 | 우리 코드에서 데이터가 어디에 저장되고 어떻게 읽히는가? |
| 3회차 | 11~15장 | 위치 복원, PBR, 메모리와 성능을 설명할 수 있는가? |
| 4회차 | 16~20장 | 투명도·MSAA·많은 광원은 어떻게 처리하는가? |
| 실습 | 21~25장 | 화면을 보고 원인을 찾고 구현 범위를 정직하게 설명할 수 있는가? |

### 목차

1. [비교 전에 알아야 할 용어](#s01)
2. [메시에서 화면 픽셀까지](#s02)
3. [Forward의 처리 흐름](#s03)
4. [Deferred의 처리 흐름](#s04)
5. [한 픽셀에서 일어나는 일](#s05)
6. [G-buffer에 저장하는 것](#s06)
7. [MRT와 Geometry Pass](#s07)
8. [Lighting Pass와 전체 화면 삼각형](#s08)
9. [D3D12 리소스와 바인딩](#s09)
10. [Barrier, Fence와 프레임 수명](#s10)
11. [좌표계와 깊이 기반 위치 복원](#s11)
12. [PBR과 IBL은 어디에서 계산하는가](#s12)
13. [포맷과 메모리 계산](#s13)
14. [성능 비용을 비교하는 법](#s14)
15. [Draw Call, 인스턴싱, 컬링의 관계](#s15)
16. [투명도와 특수 재질](#s16)
17. [MSAA, TAA와 화질](#s17)
18. [그림자, 화면 공간 효과와 후처리](#s18)
19. [많은 광원: Tiled, Clustered, Forward+](#s19)
20. [기법 선택과 변형](#s20)
21. [우리 프로젝트 코드 읽기](#s21)
22. [디버깅과 검증](#s22)
23. [뷰어 실습](#s23)
24. [자주 하는 질문과 확인 문제](#s24)
25. [포트폴리오 설명과 참고 자료](#s25)

<a id="s01"></a>
## 1. 비교 전에 알아야 할 용어

| 용어 | 의미 | 이 프로젝트의 예 |
| --- | --- | --- |
| Geometry | 공간상의 형태 | Helmet 정점·인덱스·삼각형 |
| Material | 표면의 반사 특성 | Base Color, Metallic, Roughness |
| Lighting | 광원과 표면·시선의 관계로 밝기와 색을 계산 | Directional Light와 PBR |
| Shading | 재질·조명 등을 평가해 표면 출력을 구하는 과정 | Normal Map, BRDF, IBL 평가 |
| Fragment | 래스터라이저가 만든 표면 후보 | 같은 화면 위치에 앞·뒤 삼각형 후보가 존재 가능 |
| Pixel | 렌더 타깃의 한 위치 | G-buffer의 (x, y) |
| Sample | MSAA 등의 개별 커버리지·깊이 샘플 | 현재 프로젝트는 1 sample |
| Pass | 목적과 입출력이 정해진 GPU 작업 묶음 | Geometry, Lighting, UI |
| Render Target | 픽셀 출력을 기록하는 이미지 리소스 | Base Color 타깃 |
| G-buffer | 조명 계산에 필요한 표면 정보를 담은 화면 공간 버퍼 묶음 | 5개 MRT |
| Depth Buffer | 깊이 테스트를 위한 저장소 | D32_FLOAT |
| Overdraw | 같은 화면 위치에 여러 표면 후보가 처리되는 현상 | 겹친 메시 |
| Bandwidth | 메모리와 처리 장치 사이에서 데이터를 옮기는 양·속도 | G-buffer 쓰기·읽기 |

Pass는 별도의 스레드나 Command Queue와 동의어가 아닙니다.
여러 Pass를 하나의 Command List에 기록할 수도 있고, 필요에 따라 나눌 수도 있습니다.
클래스를 파일로 나누었다고 Deferred가 되는 것도 아닙니다. **저장되는 중간 데이터와 조명 계산의 위치가 바뀌어야 합니다.**

Forward/Deferred는 DirectX 버전이나 PBR 사용 여부가 아닙니다.
DirectX 11·12 모두 두 방식을 구현할 수 있고, PBR 역시 양쪽에 적용할 수 있습니다.

<a id="s02"></a>
## 2. 메시에서 화면 픽셀까지

공통적인 불투명 메시 렌더링을 먼저 봅니다.

```text
CPU: 모델 읽기 → GPU 버퍼 준비 → 상수와 리소스 바인딩 → Draw 기록
GPU: 정점 입력 → Vertex Shader → 클리핑·투영·래스터라이제이션
     → Pixel Shader / 깊이·스텐실 테스트 → 렌더 타깃 기록
```

Vertex Buffer에는 위치, 법선, UV, Tangent 같은 정점 속성이 있습니다.
Index Buffer는 어떤 정점들을 삼각형으로 연결하는지 나타냅니다.
Vertex Shader는 정점을 변환하고 픽셀 단계에 필요한 값을 출력합니다.
래스터라이저는 삼각형이 화면의 어느 위치를 덮는지 계산하며 UV·법선 등의 값을 보간합니다.
Pixel Shader는 그 값과 텍스처를 읽어 출력을 만듭니다.

**정점 색을 계산하는 것과 화면 픽셀을 계산하는 것은 다릅니다.**
한 삼각형이 화면을 크게 덮으면 정점은 세 개여도 매우 많은 픽셀 작업이 발생합니다.
반대로 작은 삼각형이 매우 많으면 화면 크기에 비해 정점·삼각형 처리 비용이 커질 수 있습니다.

위 그림은 개념 순서입니다. 깊이 테스트가 무조건 Pixel Shader 뒤에서만 실행된다는 뜻은 아닙니다.
GPU는 조건이 허용하면 Early-Z로 보이지 않는 후보를 셰이딩 전에 제거할 수 있습니다.
깊이 출력, discard, UAV 부작용 등은 실행·최적화 조건에 영향을 주므로 단순히 모든 픽셀이 같은 순서로 실행된다고 가정하지 않습니다.

HLSL에서 VS 출력의 `SV_POSITION`은 클립 공간 위치이고 PS 입력의 `SV_POSITION.xy`는 화면 위치입니다.
같은 이름이라고 좌표계가 같지는 않습니다. [Microsoft HLSL Semantics](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics)

<a id="s03"></a>
## 3. Forward의 처리 흐름

### 3.1 표면을 그리면서 최종 조명 계산

```text
Mesh + Material + Transform + Camera + Lights
                         ↓
                메시를 래스터라이즈
                         ↓
          PS: 텍스처 평가 → Normal → PBR·IBL
                         ↓
               조명이 적용된 색 기록
```

예를 들어 Helmet 표면의 한 픽셀은 Base Color, Normal, Metallic, Roughness를 읽습니다.
같은 Pixel Shader 안에서 광원 방향, 시선 방향과 BRDF를 계산하고 결과 색상을 출력합니다.
출력 대상은 바로 Back Buffer일 수도 있고, 후처리를 위한 HDR 타깃일 수도 있습니다.

설명용 의사 코드:

```cpp
for (const auto& draw : draws)
{
    bind_mesh(draw);
    bind_material(draw);
    bind_camera_and_lights();
    draw_mesh(); // PS에서 재질과 조명을 함께 평가
}
```

### 3.2 광원이 여러 개면 어떻게 하나

일반적인 단일 패스 Forward는 한 PS에서 여러 광원을 순회할 수 있습니다.
광원마다 메시를 다시 그리는 additive multipass Forward도 있지만, 이것이 Forward의 필수 조건은 아닙니다.
따라서 **“Forward는 광원 수만큼 Draw Call이 반드시 증가한다”는 설명은 틀립니다.**

현재 비교용 `MatchedForward.hlsl`은 표면 재질을 읽은 뒤 `bench_shade`를 호출합니다.
이 함수 내부에서 Point Light 배열을 순회합니다. 광원 하나당 별도의 메시 Draw를 하는 구조가 아닙니다.

### 3.3 무엇이 좋은가

표면 정보 전체를 별도의 화면 버퍼에 저장하지 않고 바로 조명에 사용할 수 있습니다.
재질마다 서로 다른 셰이딩을 적용하기 쉽고, 투명 표면의 조명과 블렌딩을 연결하기도 상대적으로 자연스럽습니다.
광원 목록을 잘 제한하면 많은 광원을 다루는 Forward도 충분히 효율적일 수 있습니다.

<a id="s04"></a>
## 4. Deferred의 처리 흐름

### 4.1 무엇을 미루는가

Deferred는 **조명 평가를 표면 기록 이후로 미룹니다.** 다음 프레임까지 미루거나 CPU 작업을 비동기로 실행한다는 의미가 아닙니다.

```text
1. Geometry Pass
   메시 → 재질 텍스처 평가 → G-buffer + Depth

2. Lighting Pass
   G-buffer + Camera + Lights + IBL → 조명 결과
```

Geometry Pass에서도 UV 보간, 텍스처 샘플링, Normal Map 변환 등이 실행됩니다.
“아무 계산 없이 데이터를 복사하는 가벼운 패스”라고 생각하면 안 됩니다.

Lighting Pass는 원래 메시의 정점과 UV를 다시 읽는 대신 화면 위치별로 저장된 표면 특성을 읽습니다.
이 프로젝트에서는 원본 메시를 다시 그리지 않고 전체 화면 삼각형을 그립니다.

### 4.2 장점과 추가 비용은 함께 생긴다

최종적으로 남은 불투명 표면의 조명을 화면 공간에서 평가할 수 있고,
같은 표면 정보를 여러 조명·디버그 작업에서 사용할 수 있습니다.
반면 G-buffer 기록·읽기와 저장 공간이 필요하고, 조명에 필요한 재질 특성을 버퍼 구조에 담아야 합니다.

이는 처리 비용이 사라지는 것이 아니라 **어떤 계산을 언제 수행하고 어떤 중간 정보를 보존할지 바꾸는 설계**입니다.
역사적인 적용 사례와 제약은 [NVIDIA GPU Gems 3, Deferred Shading in Tabula Rasa](https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-19-deferred-shading-tabula-rasa)를 참고합니다.
해당 자료의 옛 API·하드웨어 제한을 현재 D3D12 제한으로 그대로 받아들이지는 않습니다.

<a id="s05"></a>
## 5. 한 픽셀에서 일어나는 일

화면의 한 위치에 A, B, C 표면이 겹쳤고 카메라에 가장 가까운 표면이 A라고 가정합니다.
표준 깊이, 불투명, 깊이 쓰기 활성, 서로 다른 깊이, MSAA 없음이라는 조건입니다.

| 과정 | Forward | Deferred |
| --- | --- | --- |
| C가 먼저 처리됨 | C 재질·조명을 계산할 수 있음 | C 재질을 G-buffer에 기록할 수 있음 |
| B가 더 가까움 | B가 통과해 조명 색을 덮어씀 | B가 통과해 G-buffer를 덮어씀 |
| A가 가장 가까움 | A의 조명 색이 최종적으로 남음 | A의 표면 정보가 최종적으로 남음 |
| 이후 Lighting | 별도 공통 조명 단계가 필수는 아님 | A의 표면 정보를 이용해 조명 계산 |

뒤에서 앞으로 그리면 나중에 가려질 표면에도 작업이 발생할 수 있습니다.
앞에서 앞으로 가까운 순서로 그리거나 Depth Pre-pass를 사용하면 Forward도 불필요한 셰이딩을 줄일 수 있습니다.

**Deferred에서도 A/B/C의 정점 처리와 Geometry Pass 비용은 발생할 수 있습니다.**
줄일 수 있는 것은 주로 뒤에서 덮어써질 표면에 대한 비싼 조명 평가이지, 모든 오버드로가 아닙니다.

Depth Pre-pass는 재질 조명 전에 깊이만 기록하는 별도 패스입니다.
정점·래스터 작업을 추가하는 대신 후속 비싼 픽셀 작업을 줄이는 교환이므로 항상 이득은 아닙니다.
동일 깊이 비교를 사용한다면 두 패스의 위치 계산과 커버리지도 일치해야 합니다.

<a id="s06"></a>
## 6. G-buffer에 저장하는 것

### 6.1 텍스처 원본과 G-buffer의 차이

재질 텍스처는 모델의 UV 공간에 있습니다. 카메라가 바뀌어도 원본 이미지 내용은 그대로입니다.
G-buffer는 화면 공간에 있습니다. 카메라·객체가 바뀌면 각 화면 위치의 표면 정보도 달라집니다.

```text
Material Texture: UV → 재질 값
G-buffer:         화면 (x, y) → 현재 보이는 표면의 재질·법선·위치 정보
```

Geometry Pass는 재질 텍스처와 factor를 평가한 결과를 기록합니다.
따라서 조명 패스는 각 표면의 UV를 몰라도 이미 계산된 Base Color나 Normal을 사용할 수 있습니다.
대신 기록하지 않은 재질 특성을 나중에 필요로 하면 저장 구조 또는 셰이딩 방식을 확장해야 합니다.

### 6.2 우리 프로젝트의 데이터 계약

| 타깃 | 실제 의미 | 조명에서 필요한 이유 |
| --- | --- | --- |
| Albedo | 선형 Base Color와 Alpha | 확산색과 금속 F0 계산 |
| Normal | Normal Map 적용 후 월드 법선, Alpha는 선택 마스크 | N·L, N·V, 반사 방향 |
| Material | R: Metallic, G: Roughness, B: AO, A: 유효 표면 마스크 | 반사 모델과 배경 판정 |
| Emissive | 선형 발광색 | 조명 결과에 발광 추가 |
| Position/Depth | Full: 월드 위치와 유효 W / Compact: 비선형 깊이 | 시선·광원 벡터 및 위치 복원 |
| 별도 Depth | D32_FLOAT | 가장 가까운 표면의 깊이 판정 |

Material Alpha의 유효 마스크와 Albedo Alpha는 서로 다른 용도입니다.
현재 불투명 경로의 마스크는 0으로 Clear하고 표면을 기록하면 1입니다.
Metallic이 0인 유효한 비금속 표면도 있으므로 Metallic 값으로 배경을 판정하면 안 됩니다.

### 6.3 법선을 왜 0~1로 저장하나

현재 Normal은 다음처럼 인코딩합니다.

```hlsl
encoded = normal * 0.5 + 0.5;        // [-1, 1] → [0, 1]
normal  = normalize(encoded * 2 - 1);
```

Compact의 UNORM 포맷은 음수를 직접 저장할 수 없기 때문입니다.
Full은 부동소수점이지만 두 레이아웃이 같은 해석 방식을 쓰도록 동일한 인코딩을 사용합니다.
양자화 때문에 읽은 벡터가 정확히 단위 길이가 아닐 수 있어 다시 normalize합니다.
법선 텍스처와 G-buffer Normal에는 색상용 sRGB 변환을 적용하지 않습니다.

<a id="s07"></a>
## 7. MRT와 Geometry Pass

MRT는 Multiple Render Targets, 즉 한 픽셀 셰이더에서 여러 색상 타깃으로 출력하는 방식입니다.
우리 `GBuffer.hlsl`은 다음 출력 구조를 사용합니다.

```hlsl
struct GBUFFER_OUTPUT
{
    float4 albedo         : SV_TARGET0;
    float4 normal         : SV_TARGET1;
    float4 material       : SV_TARGET2;
    float4 emissive       : SV_TARGET3;
    float4 world_position : SV_TARGET4;
};
```

**색상 타깃이 5개라는 이유만으로 메시를 5번 그리지는 않습니다.**
메시 Draw 한 번에서 5개 출력을 생성합니다. 단, 쓰는 데이터량과 출력 비용은 늘어납니다.

실제 `GBufferRenderPass::render` 흐름:

1. 5개 타깃을 SRV 읽기 상태에서 Render Target 쓰기 상태로 바꿉니다.
2. 5개 RTV와 DSV를 `OMSetRenderTargets`로 연결합니다.
3. 타깃과 Depth를 한 번 Clear합니다.
4. Full/Compact와 Solid/Wireframe에 맞는 PSO를 선택합니다.
5. 각 Draw의 상수·재질 SRV·정점·인덱스·인스턴스 데이터를 바인딩합니다.
6. `DrawIndexedInstanced`로 표면을 기록합니다.
7. 출력 연결을 해제하고 G-buffer를 SRV 읽기 상태로 전환합니다.

객체마다 Clear하면 앞 객체의 데이터가 사라집니다. 한 장면의 Geometry Pass 전체에 대해 초기화해야 합니다.
PSO의 `NumRenderTargets`, 각 `RTVFormats`, 실제 리소스 포맷과 Shader 출력 계약도 맞아야 합니다.
Compact 전환은 UI 문자열만 바꾸는 것이 아니라 타깃·SRV·PSO 조합까지 함께 바꾸는 일입니다.

<a id="s08"></a>
## 8. Lighting Pass와 전체 화면 삼각형

### 8.1 왜 Vertex Buffer 없이 그릴 수 있나

`VS_Deferred`는 `SV_VertexID` 0, 1, 2를 이용해 클립 공간의 큰 삼각형을 만듭니다.
현재 식에서 정점은 (-1, 1), (3, 1), (-1, -3)이고 화면 사각형을 덮습니다.

```cpp
list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
list->DrawInstanced(3, 1, 0, 0);
```

삼각형은 모델이 아니라 **화면의 각 픽셀에서 Lighting PS를 실행하기 위한 수단**입니다.
세 정점만 생성해도 화면 전체의 픽셀 작업이 실행되므로 세 정점이라는 사실만으로 비용이 작다고 판단할 수 없습니다.

### 8.2 Lighting PS의 순서

```text
화면 좌표 → G-buffer Load → 유효 표면 판정
         → 위치 복원 또는 직접 읽기
         → Normal Decode → Material 읽기
         → 직접광 + IBL + Emissive
         → Exposure + Tone Mapping + 출력 변환
```

현재 패스는 깊이 테스트·깊이 쓰기를 끕니다.
Geometry Pass가 이미 불투명 가시성을 정했고, 표면이 없는 픽셀은 유효 마스크로 제외하기 때문입니다.
배경에는 먼저 그린 Skybox가 남습니다. 이 방식이 모든 Deferred 엔진의 유일한 방법은 아닙니다.

### 8.3 Load와 Sample의 차이

G-buffer는 같은 화면 위치의 데이터를 정확하게 읽어야 하므로 정수 좌표와 mip 0을 전달합니다.

```hlsl
int3 pixel = int3(int2(position.xy), 0);
float3 material = gbuffer_material.Load(pixel).xyz;
```

Sample로 경계 양쪽 표면을 선형 보간하면 서로 다른 물체의 법선·재질이 섞일 수 있습니다.
환경맵처럼 방향·LOD에 따라 필터링해야 하는 리소스에는 Sample/SampleLevel을 사용합니다.
관련 API 의미는 [Microsoft Texture Load](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-load)를 참고합니다.

<a id="s09"></a>
## 9. D3D12 리소스와 바인딩

### 9.1 실제 데이터와 View는 다르다

| 개념 | 역할 |
| --- | --- |
| Resource | 실제 버퍼·텍스처와 그 메모리 |
| RTV | 리소스를 색상 출력 대상으로 보는 설명 |
| DSV | 리소스를 깊이·스텐실 대상으로 보는 설명 |
| SRV | 셰이더에서 읽을 리소스의 포맷·범위·차원 설명 |
| CBV | 상수 버퍼를 셰이더에 제공하는 방식 |
| UAV | 셰이더의 임의 접근 쓰기 등에 사용하는 View, 일반적인 현재 G-buffer 출력은 RTV |
| VBV / IBV | 정점·인덱스 버퍼 주소·크기·stride 또는 포맷 |

하나의 G-buffer 리소스에 RTV와 SRV를 만들 수 있습니다.
이것은 픽셀 데이터를 두 번 복사한다는 의미가 아니라, **같은 저장소를 다른 용도로 해석하는 View를 만든다**는 의미입니다.
동일 서브리소스를 일반 RTV 쓰기와 SRV 읽기로 동시에 사용해서는 안 됩니다.

### 9.2 Heap이라는 이름도 구분한다

- Resource Heap: 실제 리소스가 사용할 메모리와 배치에 관한 개념.
- Descriptor Heap: 리소스를 어떻게 볼지에 대한 설명들을 저장하는 배열.
- DEFAULT: G-buffer·정적 메시처럼 GPU 중심으로 사용하는 리소스에 사용.
- UPLOAD: CPU가 기록한 상수·업로드 데이터를 GPU에 전달하는 용도.
- READBACK: GPU 결과를 CPU가 읽을 때 사용하는 용도.

메모리의 물리적 위치는 GPU와 메모리 아키텍처에 따라 달라질 수 있으므로 DEFAULT를 무조건 전용 VRAM이라고 외우지 않습니다.
UPLOAD Heap에 G-buffer를 만들어 CPU가 픽셀을 채우는 것이 아닙니다. Geometry Pass의 GPU 출력이 DEFAULT 리소스를 채웁니다.

Shader-visible CBV/SRV/UAV Heap은 셰이더 리소스 조회에 사용합니다.
RTV/DSV Heap은 Output Merger에 연결하는 descriptor를 보관하며 같은 방식의 Shader-visible Heap이 아닙니다.
[Microsoft Descriptor Heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview)

모든 버퍼 바인딩에 반드시 Heap의 descriptor를 만들어야 하는 것은 아닙니다.
현재의 Root CBV나 Root SRV처럼 지원되는 버퍼를 GPU 가상 주소로 직접 연결하는 방식도 있습니다.
텍스처를 읽는 SRV Descriptor Table과 이 방식을 구분하면 “CBV를 생성하는 코드가 어디 있는가”라는 혼동을 줄일 수 있습니다.

### 9.3 Root Signature와 PSO

Root Signature는 셰이더에 전달할 리소스 바인딩의 계약입니다.
PSO는 Shader, Rasterizer, Depth, Blend, 출력 포맷 등의 조합입니다.
Root Signature를 Serialize하는 이유는 API가 사용할 바이너리 표현으로 만들기 위해서이며,
`ID3DBlob`은 이런 직렬화 결과나 셰이더 바이트코드를 보관하는 메모리 객체입니다.
[Microsoft Root Signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures-overview)

현재 Lighting Pass의 바인딩:

| HLSL 슬롯 | 내용 |
| --- | --- |
| b0 | 카메라·ViewProjection·조명·표시 설정 등 기존 상수 |
| b6 | 비교 광원 정보, Compact 여부, 화면 크기, 역 ViewProjection |
| t0~t4 | G-buffer 5개 |
| t5 | 환경 Cubemap |
| t6 | BRDF LUT |
| t7 | Prefiltered Specular Cubemap |
| s0 | 환경 리소스 샘플러 |

`b6`은 이름과 파일 위치가 Benchmark에 있어도 **일반 Compact 뷰어의 위치 복원에도 사용**합니다.
Benchmark 폴더를 실험 전용이라고 통째로 삭제하면 기본 렌더링까지 깨질 수 있습니다.
`t5`가 바인딩되어 있어도 현재 Diffuse IBL은 고정 SH 계수로 계산한다는 점도 구분합니다.

<a id="s10"></a>
## 10. Barrier, Fence와 프레임 수명

### 10.1 Barrier: GPU의 리소스 접근 용도 전환

```text
G-buffer: PIXEL_SHADER_RESOURCE
       → RENDER_TARGET
       → Geometry 쓰기
       → PIXEL_SHADER_RESOURCE
       → Lighting 읽기

Back Buffer: PRESENT → RENDER_TARGET → 화면 합성 → PRESENT
```

Transition Barrier는 이전 접근과 이후 접근의 상태를 알립니다.
바인딩만 바꿨다고 필요한 상태 전환이 자동 완료되지는 않습니다.
Barrier의 BeforeState도 실제 상태와 맞아야 합니다.
UAV 접근 순서나 겹치는 메모리 재사용에는 별도의 UAV/Aliasing Barrier 개념이 있습니다.
[Microsoft Resource Barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)

### 10.2 Fence: 작업 완료 시점 확인

Fence는 CPU가 GPU 완료를 기다리거나 Queue 사이의 작업 순서를 연결할 때 사용합니다.
Barrier를 넣었다고 CPU가 즉시 리소스를 해제해도 되는 것은 아닙니다.
반대로 CPU에서 Fence를 기다렸다고 잘못된 리소스 상태가 자동 수정되는 것도 아닙니다.
[Microsoft Fence-Based Resource Management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management)

Command List는 명령 기록, Allocator는 기록용 메모리, Queue는 실행 제출을 담당합니다.
GPU가 아직 사용하는 Allocator나 상수 버퍼·descriptor 내용을 CPU가 덮어쓰면 안 됩니다.
여러 프레임을 동시에 진행하려면 프레임별 데이터와 Fence에 따른 재사용 시점을 관리해야 합니다.
같은 Direct Queue의 순차 GPU 사용과 CPU가 쓰는 프레임 데이터 수명은 서로 구별합니다.

### 10.3 현재 구현에서 확인할 부분

일반 뷰어의 `render_frame`은 프레임 시작에서 `wait_for_gpu()`를 호출합니다.
따라서 벤치마크 경로와 일반 뷰어의 CPU/GPU 겹침 조건을 같다고 가정할 수 없습니다.
창 크기 변경·Full/Compact 전환·Import 반영에서도 사용 중인 리소스의 수명을 보호해야 합니다.
GPU가 사용하는 이전 타깃을 기다린 뒤 해제하고, 새 리소스의 RTV/SRV와 크기 상수를 함께 갱신합니다.

<a id="s11"></a>
## 11. 좌표계와 깊이 기반 위치 복원

이 장의 목표는 코드를 외우는 것이 아니라 **화면 좌표 두 개와 깊이 한 개로 왜 3D 위치를 되찾을 수 있는지** 이해하는 것입니다.

### 11.1 좌표 변환을 순서대로 생각하기

| 공간 | 무엇을 기준으로 표현하나 | 예 |
| --- | --- | --- |
| Local / Model | 모델 자체의 원점 | 헬멧 정점의 원래 위치 |
| World | 장면 전체의 공통 원점 | 장면에서 이동·회전한 헬멧 |
| View | 카메라 위치와 방향 | 카메라 앞쪽으로 얼마나 떨어져 있는가 |
| Clip | 투영 후 동차 좌표 | x, y, z, w 네 성분 |
| NDC | Clip의 xyz를 w로 나눈 좌표 | D3D의 통상적인 z 범위는 0~1 |
| Screen | 화면의 픽셀 위치 | 좌상단 원점의 (x, y) |

행 벡터 규약에서:

```text
p_world = p_local × World
p_view  = p_world × View
p_clip  = p_view  × Projection
p_ndc   = p_clip.xyz / p_clip.w
```

`row_major`는 메모리 배치에 관한 지정입니다.
행 벡터 계산을 하는지는 `mul(vector, matrix)` 같은 실제 연산 순서까지 확인해야 합니다.
반대 규약을 쓰는 자료의 행렬을 복사하면 전치나 곱셈 순서가 달라질 수 있습니다.

### 11.2 화면 위치만으로는 부족하다

화면 중앙의 픽셀을 생각해 봅시다.
카메라 바로 앞의 물체도, 멀리 있는 물체도 같은 중앙 픽셀에 투영될 수 있습니다.
화면 위치만으로 알 수 있는 것은 대략 **카메라에서 뻗는 한 광선**이지 그 위의 정확한 위치가 아닙니다.
깊이가 추가되면 그 광선 위에서 어느 지점인지 정할 수 있습니다.

이때 카메라와 투영 설정도 알고 있어야 합니다. 같은 화면 픽셀이라도 FOV나 카메라 위치가 다르면 광선이 달라집니다.
이를 행렬로 묶어 되돌리는 것이 역 View-Projection입니다.

### 11.3 픽셀 중심에서 NDC로

가정: 화면 크기 W×H, 전체 화면 Viewport, 깊이 범위 0~1, 좌상단 기준 정수 픽셀 좌표 (x, y).

```text
u = (x + 0.5) / W
v = (y + 0.5) / H

x_ndc = 2u - 1
y_ndc = 1 - 2v
z_ndc = 저장한 깊이 d
```

0.5는 픽셀의 모서리가 아니라 중심을 사용하기 위한 값입니다.
화면 y는 아래로 증가하지만 NDC y는 위로 증가하므로 방향이 반대입니다.
PS의 `SV_POSITION.xy`는 이미 픽셀 중심 좌표이므로 거기에 다시 0.5를 더하지 않습니다.
현재 코드는 정수 `pixel.xy`에서 복원하므로 0.5를 더합니다.

### 11.4 역행렬과 마지막 W 나눗셈

```text
h = (x_ndc, y_ndc, d, 1) × inverse(View × Projection)
p_world = h.xyz / h.w
```

설명용 HLSL:

```hlsl
float2 uv = (float2(pixel) + 0.5) / screen_size;
float2 ndc = uv * float2(2, -2) + float2(-1, 1);
float4 h = mul(float4(ndc, depth, 1), inverse_view_projection);
float3 world_position = h.xyz / h.w;
```

왜 또 나눌까요? 투영에서 w로 나눈 좌표를 출발점으로 사용했기 때문에 역변환 결과도 동차 좌표입니다.
xyz만 꺼내면 일반적인 3D 위치가 되지 않습니다. 마지막 나눗셈까지 해야 실제 위치를 얻습니다.

역행렬은 **깊이를 만들 때 사용한 바로 그 View·Projection**의 역이어야 합니다.
Viewport offset, 다른 깊이 범위, 비대칭 투영, TAA jitter가 있다면 복원도 그 설정을 반영해야 합니다.
깊이 이미지의 해상도와 화면 크기가 달라도 좌표 대응을 수정해야 합니다.

### 11.5 Depth는 카메라 거리와 다르다

일반적인 원근 투영의 depth는 선형 거리가 아닙니다.
프로젝트의 LH 원근 투영, near=n, far=f, 시점 깊이=z에서:

```text
d = f / (f - n) - (f × n) / ((f - n) × z)
z = (f × n) / (f - d × (f - n))
```

n=0.1, f=100인 예:

| 시점 깊이 z | 저장되는 d, 근삿값 |
| --- | --- |
| 0.1 | 0 |
| 1 | 0.900901 |
| 10 | 0.990991 |
| 50 | 0.998999 |
| 100 | 1 |

`depth * far`로 거리를 구할 수 없는 이유입니다.
또한 시점 z는 카메라의 전방 축 방향 거리이지 카메라에서 표면까지의 유클리드 거리와 항상 같지는 않습니다.

Overview의 Linear View Depth는 깊이로 복원한 위치에서 시점 깊이를 얻고 Range로 나눈 표시입니다.
현재 투영은 clip.w가 view.z와 같아 이를 사용합니다. 모든 투영에서 무조건 clip.w가 거리인 것은 아닙니다.

### 11.6 Reversed-Z와 정밀도

표준 깊이는 near=0, far=1이고 현재 프로젝트도 Depth Clear 1, LESS 비교를 사용합니다.
Reversed-Z는 보통 near=1, far=0으로 매핑하고 Clear·비교·투영을 함께 바꿉니다.
부동소수점 깊이와 결합하면 정밀도 분포를 유리하게 만들 수 있지만, 깊이가 선형이 되는 것은 아닙니다.
복원 행렬·배경 판정·그림자 관련 깊이 규약도 같이 검토해야 합니다.
현재 프로젝트에 구현된 기능으로 취급하지 않습니다.
[NVIDIA: Visualizing Depth Precision](https://developer.nvidia.com/blog/visualizing-depth-precision/)

<a id="s12"></a>
## 12. PBR과 IBL은 어디에서 계산하는가

### 12.1 렌더링 구조와 반사 모델을 분리해서 생각하기

Forward/Deferred는 **계산 배치 방식**, PBR은 **표면 반사를 표현하는 모델**입니다.
동일한 PBR 함수를 Forward PS에서도, Deferred Lighting PS에서도 호출할 수 있습니다.
Deferred로 바꾸었다고 자동으로 재질이 더 사실적으로 보이는 것은 아닙니다.

PBR 계산에 필요한 대표 값:

| 기호 | 의미 |
| --- | --- |
| P | 표면 위치 |
| N | 표면 법선, 단위 벡터 |
| V | 표면에서 카메라로 향하는 단위 벡터 |
| L | 표면에서 광원으로 향하는 단위 벡터 |
| H | normalize(V + L), 반벡터 |
| Base Color | 비금속 확산색 또는 금속의 반사색에 관여 |
| Metallic | 금속/비금속 워크플로 제어 |
| Roughness | 미세 표면 분포의 거칠기 제어 |

Forward는 이 값들을 재질 평가와 함께 사용합니다.
Deferred는 표면 특성을 G-buffer에 기록해 두고, V·L·H처럼 카메라·광원에 의존하는 값을 조명 패스에서 구할 수 있습니다.

### 12.2 직접광의 기본 식

```text
f_specular = D × F × G / (4 × max(N·V, 0) × max(N·L, 0))
f_diffuse  ≈ (1 - F) × (1 - metallic) × base_color / pi

L_direct = (f_diffuse + f_specular) × L_in × max(N·L, 0)
```

실제 구현은 0으로 나누지 않도록 작은 값으로 분모를 제한합니다.
D는 미세 표면 법선 분포, F는 Fresnel 반사 비율, G는 미세 표면의 가림을 근사합니다.
G의 미세 표면 가림은 다른 메시가 만드는 Shadow Map의 그림자와 다릅니다.

자주 사용하는 Schlick 근사는:

```text
F = F0 + (1 - F0) × (1 - V·H)^5
F0 = lerp(0.04, base_color, metallic)
```

0.04는 흔한 비금속 근삿값이지 모든 물질에 정확한 상수는 아닙니다.
GGX에 넣는 alpha와 UI의 perceptual roughness도 구분해야 합니다.
흔히 alpha=r²를 사용하지만 함수가 r을 받는지 alpha를 받는지에 따라 제곱 위치가 달라집니다.
숫자만 보고 “제곱이 두 번이면 무조건 오류”라고 외우기보다 변수의 의미를 추적해야 합니다.
[PBRT: Roughness Using Microfacet Theory](https://www.pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory)

### 12.3 Normal Map과 좌표 공간

Normal Map은 보통 Tangent Space의 방향을 담습니다.
Tangent T, Bitangent B, 기하 법선 N을 이용해 월드 공간 방향으로 옮깁니다.

```text
n_tangent = texture_normal × 2 - 1
n_world   = normalize(T × n_tangent.x + B × n_tangent.y + N × n_tangent.z)
```

법선은 조명과 같은 공간에서 계산해야 합니다. 월드 법선에 View Space 광원 벡터를 내적하면 결과가 틀립니다.
비균일 스케일에서 법선은 일반 방향 벡터처럼 World의 3×3만 곱하면 안 되고 역전치 변환이 필요합니다.
이는 변환된 접선과 법선의 수직 관계를 보존하기 위한 것입니다.
미러 변환과 Tangent의 handedness도 Bitangent 방향에 영향을 줍니다.

Normal Map은 셰이딩 방향을 바꾸지만 모델의 실루엣·삼각형 위치를 직접 바꾸지는 않습니다.

### 12.4 glTF 텍스처와 G-buffer의 채널을 혼동하지 않기

glTF Metallic-Roughness 입력은 G가 Roughness, B가 Metallic입니다.
AO는 별도 `occlusionTexture` 참조의 R을 읽으며 같은 이미지인지 여부는 모델마다 다릅니다.
Base Color·Emissive는 색상 데이터이고 Metallic·Roughness·Normal·AO는 데이터 텍스처입니다.
G-buffer의 R=Metallic, G=Roughness, B=AO는 우리가 선택한 출력 패킹입니다.
[Khronos glTF 2.0 Materials](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#materials)

### 12.5 IBL은 배경을 그리는 것과 다르다

Skybox는 환경을 화면 배경으로 표시합니다.
IBL은 환경에서 표면으로 들어오는 빛의 효과를 계산합니다.
따라서 Skybox가 보여도 IBL 바인딩·방향·재질 처리가 잘못되면 금속 표면은 어두울 수 있습니다.
반대로 Skybox를 숨겨도 IBL 계산을 유지하면 모델의 환경 반사는 남습니다.

대표적인 실시간 구성:

- Diffuse IBL: 법선 방향의 저주파 환경 조명, Irradiance Map 또는 SH 등으로 근사.
- Specular IBL: 반사 방향과 Roughness에 따라 prefilter된 환경맵을 조회.
- BRDF LUT: 시선각·거칠기에 따른 적분 보정항을 미리 계산해 조회.

Prefiltered mip는 단순히 원본 이미지를 작게 만든 것과 목적이 다릅니다.
거칠기에 따른 반사 분포를 근사하도록 생성해야 하며 LUT·roughness 규약도 서로 맞아야 합니다.
현재 뷰어는 고정 SH 계수와 제공된 Specular DDS·BRDF LUT를 사용합니다.
임의 환경맵으로 교체할 때 이 값들도 일치해야 하는 이유를 이해하는 것이 중요합니다.

### 12.6 색상 공간, AO와 Emissive

조명 에너지는 선형 공간에서 더한 뒤 노출·Tone Mapping·출력 변환을 적용합니다.
Base Color를 감마 인코딩된 값 그대로 BRDF에 넣거나, 선형 결과에 감마 변환을 두 번 적용하면 색이 달라집니다.
단순 pow(1/2.2)는 흔한 근사이며 정확한 sRGB 전달 함수와 동일하지 않습니다.

AO는 주변 간접광의 가림을 근사하는 값이지 모든 직접광을 어둡게 하는 보편적인 그림자 값이 아닙니다.
어느 조명 성분에 AO를 적용할지는 셰이딩 모델에 따라 구분합니다.
Emissive를 화면 색에 더한다고 주변 물체가 실제로 밝아지는 것은 아닙니다. 그 효과에는 GI나 별도의 광원 표현이 필요합니다.

<a id="s13"></a>
## 13. 포맷과 메모리 계산

### 13.1 이름에서 저장량 읽기

`R8G8B8A8`은 8비트 성분 4개이므로 32비트, 즉 4바이트입니다.
UNORM은 정수를 0~1 범위로 해석합니다. FLOAT는 부동소수점 표현이며 저장 가능한 범위·정밀도가 다릅니다.

| 타깃 | Full | B/pixel | Compact | B/pixel |
| --- | --- | ---: | --- | ---: |
| Base Color | R8G8B8A8_UNORM | 4 | 동일 | 4 |
| Normal | R16G16B16A16_FLOAT | 8 | R10G10B10A2_UNORM | 4 |
| Material | R8G8B8A8_UNORM | 4 | 동일 | 4 |
| Emissive | R16G16B16A16_FLOAT | 8 | R11G11B10_FLOAT | 4 |
| Position/Depth | R32G32B32A32_FLOAT | 16 | R32_FLOAT | 4 |
| **합계** | | **40** | | **20** |

Normal RGB는 Compact에서도 각 10비트를 쓰며 Alpha는 2비트입니다.
현재 Alpha의 선택 여부처럼 작은 상태값에는 사용할 수 있지만 정밀한 연속값을 저장하려면 부족합니다.
R11G11B10_FLOAT는 음수를 저장하지 못하지만 일반적인 비음수 Emissive 저장에 사용할 수 있습니다.
포맷을 줄이면 밴딩·법선 오차·하이라이트 차이가 생길 수 있으므로 값의 범위와 시각적 민감도를 함께 봅니다.

### 13.2 메모리는 해상도에 비례한다

```text
색상 타깃 데이터 용량 = Width × Height × bytes_per_pixel
MiB = bytes / 1,048,576
```

정렬·압축·할당 오버헤드와 다른 리소스를 제외한 계산:

| 해상도 | Full 40B | Compact 20B | 별도 D32 4B |
| --- | ---: | ---: | ---: |
| 1920×1080 | 79.102 MiB | 39.551 MiB | 7.910 MiB |
| 3840×2160 | 316.406 MiB | 158.203 MiB | 31.641 MiB |

가로와 세로가 두 배면 픽셀 수는 네 배입니다.
1920×1080에서 색상 타깃과 D32만 합치면 Full은 87.012 MiB, Compact는 47.461 MiB입니다.
**40→20은 색상 타깃 50% 감소이고, 44→24는 Depth까지 포함해 약 45.45% 감소**입니다.
전체 GPU 메모리는 메시·재질 텍스처·환경맵·Back Buffer·업로드·추가 타깃까지 포함하므로 다른 값입니다.

프레임별로 같은 타깃을 여러 세트 유지하면 그 수만큼 용량이 증가할 수 있습니다.
Swap Chain 버퍼가 두 개라고 G-buffer도 반드시 두 세트라는 뜻은 아닙니다. 실제 소유 리소스 수를 확인해야 합니다.

### 13.3 저장량과 대역폭과 실행 시간은 다르다

79 MiB를 할당했다는 사실과 프레임마다 실제 DRAM에서 이동한 양은 다릅니다.
메모리 트래픽은 오버드로, 읽는 채널·횟수, 캐시, 압축, Clear 처리, 해상도 등에 영향을 받습니다.

단순히 모든 색상 데이터를 프레임당 한 번 쓰고 한 번 읽는 모델은 `2 × P × B`입니다.
이것은 비교를 이해하기 위한 단순 계산일 뿐 실제 하한·상한이나 GPU 계측값이 아닙니다.
캐시·압축으로 외부 메모리 이동은 작아질 수도 있고, 반복 작업 때문에 커질 수도 있습니다.

포맷을 절반으로 줄여도 전체 프레임 시간이 절반이 되지는 않습니다.
전체 시간의 20%만 해당 최적화의 영향을 받고 그 부분을 절반으로 줄이면:

```text
새 시간 = 기존 시간 × (0.8 + 0.2 × 0.5) = 기존 시간 × 0.9
```

이 예에서는 전체 감소는 10%입니다. 먼저 무엇이 병목인지 알아야 하는 이유입니다.

<a id="s14"></a>
## 14. 성능 비용을 비교하는 법

### 14.1 객체 수만으로는 설명할 수 없다

다음은 모두 비용을 바꿉니다.

- 정점·삼각형 수와 화면상 삼각형 크기
- 렌더 해상도와 실제 표면이 덮는 픽셀 수
- 겹침·그리기 순서·Early-Z·Depth Pre-pass
- 재질 샘플 수·BRDF 복잡도·그림자 샘플 수
- 픽셀 또는 타일에 실제로 영향을 주는 광원 수
- G-buffer 포맷·읽기·쓰기와 캐시 동작
- CPU Draw 준비·기록·리소스 업데이트와 GPU 대기

작은 Helmet을 10,000개 그리면 메시 처리량은 커지지만 각 메시의 조명 계산 면적은 작을 수 있습니다.
반대로 화면 전체를 덮는 단순 평면은 삼각형이 적어도 픽셀·조명 비용이 클 수 있습니다.

### 14.2 가정이 드러나는 비용 모델

기호를 다음처럼 정의합니다.

```text
G       : 정점·삼각형·래스터 공통 비용
P       : 화면 전체 픽셀 수
Pvis    : 보이는 불투명 표면 픽셀 수
Ff      : Forward에서 실제 재질·조명을 평가하는 fragment 수
Fg      : Geometry Pass에서 실제 재질을 평가하는 fragment 수
Lavg    : 평가 대상 fragment/pixel당 평균 광원 수
Cm      : 재질 평가 비용
Cl      : 광원 하나의 조명 평가 비용
Wg/Rg   : G-buffer 기록/조회 비용
```

단순화한 설명 모델:

```text
Forward ≈ G + Ff × (Cm + Lavg × Cl) + 색상·깊이 비용
Deferred ≈ G + Fg × Cm + Wg + 화면 순회 비용
           + Pvis × (Rg + Lavg × Cl) + 합성 비용
```

엄밀한 하드웨어 예측식이 아닙니다. 병렬 실행, 캐시, GPU 점유율 등을 생략했습니다.
Forward가 per-object 광원 목록을 쓰고 Deferred가 전체 광원을 순회하면 Lavg 자체가 다릅니다.
따라서 구조 이름만 놓고 동일한 계수를 대입하면 안 됩니다.

### 14.3 Deferred가 유리할 수 있는 경우

비싼 조명이 많은데 같은 화면 위치에서 Forward 셰이딩이 반복되면,
최종 표면을 기록한 뒤 조명을 평가하는 구조가 비용을 줄일 수 있습니다.
화면 공간에 이미 있는 표면 정보를 여러 효과에서 재사용하는 것도 설계상 이점입니다.

단, Geometry Pass의 재질 평가와 기록 비용까지 없어지는 것은 아닙니다.
Forward가 잘 구성된 Depth Pre-pass나 광원 분류를 사용하면 차이가 줄거나 반대가 될 수 있습니다.

### 14.4 Forward가 유리할 수 있는 경우

조명과 재질이 단순하고 불필요한 셰이딩이 적으면, 큰 G-buffer를 기록·읽는 추가 비용을 피하는 것이 유리할 수 있습니다.
많은 투명 물체나 다양한 재질 모델이 주요 콘텐츠라면 구조적 편의도 중요합니다.
MSAA 중심의 구현에서도 Forward가 자연스러운 선택이 될 수 있습니다.

이는 조건부 설명입니다. “광원 4개 이하는 Forward, 5개부터 Deferred” 같은 보편적인 경계는 없습니다.

### 14.5 CPU 시간과 GPU 시간을 구분하기

CPU 준비 시간은 장면·Draw 목록·상수 데이터를 만드는 시간입니다.
CPU 명령 기록 시간은 API로 Command List를 만드는 시간입니다.
GPU 시간은 제출된 GPU 구간의 실제 실행을 타임스탬프로 측정한 값입니다.
CPU의 Draw 호출이 빨리 반환되었다고 GPU가 이미 그리기를 마친 것은 아닙니다.

CPU와 GPU가 잘 겹치면 대략 더 느린 쪽이 처리량을 제한합니다.
강제 대기, Present/VSync, 프레임 제한이 있으면 단순한 max 모델과 달라질 수 있습니다.

```text
FPS = 1000 / frame_time_ms
60 FPS ≈ 16.67 ms
120 FPS ≈ 8.33 ms
```

GPU 병목 상황에서 CPU 작업만 줄이면 FPS 변화가 작을 수 있습니다.
이것은 CPU 최적화가 없었다는 뜻이 아니라 최종 프레임을 제한하는 구간이 바뀌지 않았다는 뜻일 수 있습니다.
추론과 확정은 구분하고 실제 구간 측정으로 확인해야 합니다.

<a id="s15"></a>
## 15. Draw Call, 인스턴싱, 컬링의 관계

### 15.1 서로 다른 문제를 해결한다

| 기법 | 주로 바꾸는 것 |
| --- | --- |
| Forward / Deferred | 재질·조명의 계산 순서와 중간 데이터 |
| Instancing | 유사 객체를 적은 Draw로 제출 |
| 리소스 공유 | 같은 메시·텍스처의 중복 저장·생성 감소 |
| Frustum / Occlusion Culling | 불필요한 객체·Draw의 제출 또는 처리 감소 |
| Light Culling | 표면에 관계없는 광원 평가 감소 |
| LOD | 거리 등에 맞게 geometry·재질 복잡도 조절 |

Forward와 Deferred 모두 인스턴싱과 컬링을 사용할 수 있습니다.
인스턴싱으로 Draw 10,000회를 1회로 줄여도 인스턴스 10,000개의 정점 변환과 픽셀 처리가 모두 없어지지는 않습니다.
같은 물체라도 서로 다른 PSO·재질 바인딩·투명 정렬 조건 때문에 항상 한 번으로 묶을 수 있는 것도 아닙니다.

### 15.2 컬링을 끈다는 말의 범위

Frustum Culling, Occlusion Culling, Backface Culling, Light Culling, Depth Test는 서로 다른 단계의 기능입니다.
객체 컬링을 꺼도 깊이 테스트는 남아 있을 수 있습니다.
광원의 반경 밖에서 attenuation이 0이 되는 것과, 광원 목록에서 미리 제외해 BRDF 계산 자체를 생략하는 것도 다릅니다.

학습용 비교에서 변수를 고정하려고 컬링을 제외할 수는 있지만,
실제 엔진 설계에서 컬링을 제거하는 것이 일반적으로 좋은 최적화라는 뜻은 아닙니다.

<a id="s16"></a>
## 16. 투명도와 특수 재질

### 16.1 왜 불투명과 같은 방법으로 처리하기 어려운가

기본 G-buffer는 한 화면 위치에 가장 가까운 불투명 표면 하나의 특성을 남깁니다.
그런데 유리 뒤에 헬멧이 보이는 장면은 유리와 헬멧의 기여를 모두 보존해야 합니다.
유리 정보로 G-buffer를 덮어쓰면 뒤의 표면 정보를 잃고, 안 쓰면 유리 자체를 표현할 수 없습니다.

일반적인 straight alpha 합성은 다음 형태입니다.

```text
C_out = alpha × C_front + (1 - alpha) × C_back
```

법선·Metallic·Roughness를 이 식으로 섞은 후 조명하는 것과,
각 표면을 조명한 결과를 섞는 것은 같지 않습니다. BRDF가 이 값들에 대해 선형이 아니기 때문입니다.

### 16.2 흔한 해결: Hybrid

```text
불투명 Geometry → Deferred Lighting → HDR 색상
                                      ↓
                     투명 표면 Forward 조명·블렌딩
                                      ↓
                         Tone Mapping / 최종 출력
```

투명은 보통 불투명 Depth를 테스트하되 깊이 쓰기를 끄고 뒤에서 앞으로 정렬합니다.
복잡한 교차 표면에서는 객체 단위 정렬만으로 완벽하지 않을 수 있습니다.
OIT, Depth Peeling 등의 대안도 있지만 추가 메모리·패스·정밀도 문제가 생깁니다.
유리의 굴절은 단순 alpha 합성보다 더 많은 표면·배경 정보가 필요합니다.

### 16.3 Alpha Mask는 별개

Alpha Mask는 임계값 미만의 fragment를 버리고 나머지는 불투명처럼 처리하는 방식입니다.
머리카락 카드나 나뭇잎에 쓰일 수 있지만 부드러운 반투명과 동일하지 않습니다.
Depth Pre-pass가 있다면 alpha 판정도 일치해야 하며 셰이더 discard와 깊이 최적화의 관계를 고려해야 합니다.
현재 프로젝트의 glTF 경로는 OPAQUE를 대상으로 하고 MASK/BLEND는 지원 범위 밖입니다.

### 16.4 재질 표현력의 문제

Clearcoat, Anisotropy, Subsurface, Hair처럼 서로 다른 모델이 필요한 경우를 생각해 봅시다.
Forward는 재질별 PS에서 필요한 계산을 직접 실행하기 쉽습니다.
Deferred는 조명 패스가 그 재질을 구분할 ID와 추가 특성을 알아야 합니다.
해결 방법에는 G-buffer 채널 추가, 재질별 패스, ID 기반 분기 등이 있지만 저장량·분기·패스 비용이 생깁니다.
따라서 “Deferred는 재질을 지원할 수 없다”가 아니라 **어떤 재질을 어떤 데이터 계약으로 표현할지 설계해야 한다**가 정확합니다.

<a id="s17"></a>
## 17. MSAA, TAA와 화질

### 17.1 MSAA가 다루는 문제

픽셀 경계에 삼각형이 일부만 걸치면 계단 현상이 생깁니다.
MSAA는 픽셀 안의 여러 sample 위치에서 coverage와 깊이를 구분합니다.
기본 픽셀 주파수 셰이딩에서는 sample 수와 같은 횟수로 PS가 항상 실행되는 것은 아닙니다.
재질의 고주파 Normal·Specular 깜빡임까지 모두 해결하는 방법도 아닙니다.
[Microsoft Rasterization Rules](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-rasterizer-stage-rules)

### 17.2 Deferred에서 어려워지는 이유

경계 픽셀의 sample A는 금속, sample B는 배경 또는 다른 재질일 수 있습니다.
G-buffer 값을 먼저 평균 내면 존재하지 않는 중간 법선·중간 재질이 만들어질 수 있습니다.
그래서 multisampled G-buffer, sample별 표면 처리, edge 판정과 적절한 resolve 전략이 필요합니다.
논리적 저장량도 sample 수에 따라 커질 수 있으며 실제 대역폭·시간은 하드웨어와 구현에 따라 다릅니다.

**Deferred에서 MSAA가 원리적으로 불가능한 것은 아닙니다.**
다만 Forward 색상 결과를 resolve하는 것보다 표면 데이터 처리에 더 주의가 필요합니다.
엔진별 지원 여부를 일반적인 원리의 불가능성으로 바꾸어 설명하면 안 됩니다.
[Epic의 Forward Shading 설명](https://dev.epicgames.com/documentation/en-us/unreal-engine/forward-shading-renderer-in-unreal-engine)은 실제 엔진의 AA 선택 사례로 읽으면 좋습니다.

### 17.3 TAA는 둘 다 가능하다

TAA는 시간에 걸친 샘플을 이용합니다. 일반적으로 이전 화면, Motion Vector, Depth,
재투영·가림 해제 판정 등이 필요하며 Ghosting과 흐려짐을 제어해야 합니다.
G-buffer가 있으면 유용한 보조 정보가 있지만 TAA가 Deferred 전용은 아닙니다.

투영에 jitter를 적용했다면 위치 복원과 이전 프레임 재투영도 해당 행렬 규약을 따라야 합니다.
현재 프로젝트는 sample count 1이며 TAA/MSAA를 구현한 것으로 설명하지 않습니다.

### 17.4 색상과 법선 정밀도도 화질이다

같은 해상도라도 G-buffer 포맷, mip, 샘플러, Normal 인코딩, 출력 변환 때문에 결과가 달라질 수 있습니다.
Full/Compact 비교는 단순히 전체 이미지가 비슷한지뿐 아니라 매끈한 금속 하이라이트,
어두운 Base Color, 작은 Normal 변화에서 정밀도 차이가 보이는지도 확인해야 합니다.

<a id="s18"></a>
## 18. 그림자, 화면 공간 효과와 후처리

### 18.1 Shadow는 Deferred가 자동으로 만들지 않는다

표면이 광원을 향하고 있어도 중간에 다른 물체가 있으면 빛이 도달하지 않습니다.
직접광 식의 N·L은 표면 방향을 검사하지만 그 사이의 다른 물체를 검사하지는 않습니다.

Shadow Mapping의 개념 흐름:

```text
광원 시점에서 깊이 기록
          ↓
카메라에 보이는 표면 P를 광원 공간으로 변환
          ↓
Shadow Map 깊이와 비교 → visibility
          ↓
직접광에 visibility 적용
```

Forward와 Deferred 모두 가능합니다.
Deferred는 복원한 P를 사용하고 Forward는 메시 PS에 전달된 P를 사용할 수 있습니다.
광원 종류에 따라 Directional Cascade, Spot 투영, Point Cubemap 등 추가 비용이 달라집니다.
Bias, Shadow Acne, Peter Panning, 필터링도 별도로 공부해야 할 주제입니다.
IBL 역시 별도 가림 정보 없이 모든 주변 물체의 그림자를 자동 재현하지 않습니다.

### 18.2 G-buffer 재사용의 예

| 효과 | 활용할 수 있는 정보 | 중요한 한계 |
| --- | --- | --- |
| SSAO | Depth, Normal | 화면 밖·가려진 기하 정보 부족 |
| SSR | Depth, Normal, Roughness, 화면 색 | 화면에 없는 반사 대상은 찾기 어려움 |
| Decal | 위치 복원, Normal, 재질 타깃 | 읽기·쓰기 충돌을 피하는 설계 필요 |
| Selection Outline | 선택 마스크, 주변 픽셀 | 외곽선 판정 규칙과 해상도에 영향 |
| Motion Blur / TAA | Motion Vector와 Depth 등 | 별도의 이전 프레임 정보 필요 |

이들은 Deferred와 잘 결합할 수 있지만 자동으로 생기는 기능은 아닙니다.
Forward도 필요한 Depth·Normal 등의 보조 패스를 만들어 같은 종류의 효과를 구현할 수 있습니다.
현재 실제로 연결된 예는 선택 마스크를 이용한 Outline과 G-buffer 디버그 표시입니다.

### 18.3 HDR와 Tone Mapping 위치

여러 조명 결과는 선형 HDR 공간에서 더하는 것이 기본입니다.
Tone Mapping은 비선형이므로 `ToneMap(A + B)`와 `ToneMap(A) + ToneMap(B)`는 일반적으로 같지 않습니다.
광원마다 Tone Mapping한 뒤 더하면 올바른 조명 합성이 되지 않습니다.

확장 가능한 일반 구조는 다음과 같습니다.

```text
Opaque Lighting → HDR 타깃 → Transparent / Emissive 합성
                → Bloom 등 HDR 효과 → Exposure / Tone Mapping
                → 출력 색상 변환 → UI / Present
```

HDR 타깃에 float를 사용한다고 모니터의 HDR 출력까지 자동 지원하는 것은 아닙니다.
디스플레이 HDR은 출력 색 공간·Swap Chain·운영체제·표시장치 설정까지 별개의 문제입니다.
우리 구현은 조명 PS에서 Tone Mapping 후 UNORM Back Buffer에 쓰며 별도 HDR 후처리 체인은 아직 없습니다.

<a id="s19"></a>
## 19. 많은 광원: Tiled, Clustered, Forward+

### 19.1 Deferred여도 전부 순회하면 비싸다

화면의 모든 표면 픽셀에서 1,000개 광원을 순회하면 Deferred도 큰 비용이 발생합니다.
중요한 질문은 “광원이 총 몇 개인가”뿐 아니라 **“이 픽셀에 필요한 광원만 골랐는가”**입니다.

전통적인 Light Volume 방식은 Point Light의 영향 구 같은 볼륨을 화면에 투영하고 그 영역만 조명합니다.
광원별 additive 합성, 깊이·스텐실·카메라 내부 판정 등의 설계가 필요합니다.
Light Volume은 원본 메시를 광원마다 다시 그리는 것과 다릅니다.

### 19.2 Tiled Deferred

화면을 예를 들어 16×16 픽셀의 타일로 나눕니다. 이 크기는 예시이며 고정된 규칙이 아닙니다.
타일의 깊이 범위와 광원 영향 범위를 이용해 광원 인덱스 목록을 구성합니다.
Lighting은 그 타일의 목록만 순회합니다.

```text
G-buffer / Depth → 타일별 광원 목록 생성
                            ↓
                 타일의 픽셀 조명 계산
```

목록 생성에도 비용이 있으며 목록 메모리, 광원 수 초과 처리, Compute 동기화가 필요합니다.
타일 안에 가까운 물체와 아주 먼 배경이 섞이면 깊이 범위가 넓어져 광원 배제가 덜 효과적일 수 있습니다.

### 19.3 Forward+

광원 분류 결과를 Forward PS에서 사용합니다.
많은 구현은 Depth Pre-pass와 Compute 기반 타일 광원 분류 후 Forward 재질·조명을 계산합니다.
**최종 재질 조명을 Geometry와 분리해 G-buffer에서 계산하는 것이 아니므로 Deferred와 다릅니다.**
[AMD Forward+ 예제](https://github.com/GPUOpen-LibrariesAndSDKs/ForwardPlus11)

### 19.4 Clustered Shading

화면 x/y뿐 아니라 깊이 방향까지 공간을 나누어 더 작은 3D 영역별 광원 목록을 구성할 수 있습니다.
Forward와 Deferred 양쪽에서 사용할 수 있으며 깊이 불연속이 큰 장면에 대한 타일 분류의 한계를 줄이는 데 도움이 됩니다.
분할 수와 목록 메모리·계산 비용 사이의 선택이 필요합니다.
[Olsson 등, Clustered Deferred and Forward Shading](https://research.chalmers.se/publication/161725)

### 19.5 현재 프로젝트의 구현

비교 경로(`--benchmark`)의 Deferred에는 광원 루프가 세 가지 있습니다.
Forward(`--forward`)는 항상 전체 루프입니다. 일반 뷰어에는 Point Light가 없으므로 이 선택이 적용되지 않습니다.

| 변형 | 실행 인자 | 셀 | 깊이 범위 | 코드 |
| --- | --- | --- | --- | --- |
| 전체 루프 | (없음) | 없음 | - | `bench_shade`의 기본 루프 |
| Tiled | `--tiled` | 16×16 픽셀 타일 | 타일 안 G-buffer 표면의 최소·최대 view z | `LightCulling.hlsl`의 `CS_Tiled` |
| Clustered | `--clustered` | 16×9 화면 셀 × 24 깊이 슬라이스 | 장면 경계 사이를 지수 분할, G-buffer를 읽지 않음 | `CS_Clustered` |

프레임 순서:

```text
G-buffer → Light Culling (Compute) → Skybox → Deferred Lighting → UI
```

**광원 목록 대신 비트마스크.** 셀마다 `1024 / 32 = 32`개의 `uint`를 두고 광원 i가 닿으면 i번째 비트를 켭니다.

- 셀당 목록 크기 상한이 없어 넘침 처리가 필요 없습니다(셀당 128 B, 1080p 타일 8,160개 ≈ 1 MB).
- Lighting은 `firstbitlow`로 켜진 비트만 오름차순으로 순회합니다. 전체 루프와 **더하는 순서가 같습니다.**
- 감쇠 `saturate(1 - d²/r²)`는 반경 밖에서 정확히 0이므로, 반경 밖 광원을 건너뛰어도 결과가 변하지 않습니다.

그래서 세 변형의 Lit 화면은 같아야 합니다. `--capture`로 저장한 1280×720 화면을 비교했을 때 차이 나는 픽셀이 0개였습니다
(400개 헬멧, 256개 광원, 반경 3, Compact G-buffer, 2026-10-16).

**셀 경계 계산.** View 공간에서 `x = ndc.x · z / P00`는 ndc와 z 각각에 대해 선형입니다.
따라서 셀의 화면 사각형을 z 최소·최대에서 펼친 8개 꼭짓점의 AABB가 그 절두체 조각을 감쌉니다.
광원 구와 AABB 사이 거리가 반경 이하이면 포함합니다. 위치 복원 오차를 흡수하도록 작은 여유를 둡니다.

**Tiled와 Clustered가 다른 점.**

- Tiled는 실제 표면 깊이로 범위를 좁혀서, 표면이 한 평면에 모인 장면에서는 셀당 광원 수가 매우 적습니다.
- 한 타일에 가까운 물체 테두리와 먼 배경이 섞이면 깊이 범위가 그 사이 전체로 넓어집니다.
- Clustered는 깊이를 미리 나누므로 이런 불연속에 덜 민감하지만, 화면 셀이 크고(1280×720에서 80×80 px)
  슬라이스가 표면에 맞춰지지 않아 평평한 장면에서는 Tiled보다 셀당 광원이 많을 수 있습니다.
- 비교 UI의 **Light count heatmap**은 셀이 고른 광원 수를 색으로 보여 줍니다(파랑 0, 초록 32, 빨강 64+).

**측정.** `Benchmark/RunLightCullingStudy.ps1`이 Forward, 전체 루프, Tiled, Clustered를
같은 장면(평면 `Flat`과 20층 `Layers`), 광원 수, 반경에서 반복 측정합니다.
`Light culling ms`는 Compute 분류 비용이고 `Lighting ms`는 분류 결과를 쓰는 조명 비용입니다. 둘을 더해서 비교합니다.
결과는 해당 GPU·장면에 한정되며 상용 엔진의 Tiled/Clustered 구현을 대표하지 않습니다.
투명 재질, 그림자, Spot Light와 Clustered Forward는 구현 범위 밖입니다.

<a id="s20"></a>
## 20. 기법 선택과 변형

### 20.1 비교표

| 질문 | Forward | Deferred |
| --- | --- | --- |
| 재질과 조명 계산 위치 | 같은 메시 셰이딩에서 결합 | 표면 기록 후 별도 조명 |
| 기본 중간 데이터 | 조명 색상, Depth 등 | 표면 속성 G-buffer, Depth 등 |
| 많은 광원 | 목록 제한 여부가 중요 | 역시 목록 제한 여부가 중요 |
| 큰 G-buffer 비용 | 기본 구조에서는 피할 수 있음 | 기록·조회·저장 비용 발생 |
| 투명 재질 | 조명·합성이 비교적 자연스러움 | 보통 별도 Forward 경로 결합 |
| 재질별 복잡한 모델 | 개별 Shader로 표현하기 쉬움 | 저장 계약·분기·패스 확장 필요 |
| MSAA | 상대적으로 자연스러운 결합 | 표면 데이터 sample 처리 고려 |
| 화면 공간 효과 | 필요한 보조 정보 생성 | 저장한 정보 재사용에 유리 |
| CPU Draw 수 | 배칭·패스·광원 방식에 따라 다름 | 배칭·패스·광원 방식에 따라 다름 |
| 무조건 빠른가 | 아니오 | 아니오 |

특정 모바일·VR·데스크톱이라는 이유만으로 답을 고정하지 않습니다.
해상도, 메모리 구조, 투명도, AA, 콘텐츠, 실제 GPU 계측을 함께 보고 선택합니다.
특히 tile-based GPU에서는 타깃 유지·저장 방식과 API 활용에 따라 외부 메모리 비용이 달라질 수 있습니다.
[Imagination의 타일 메모리 설명](https://docs.imgtec.com/starter-guides/powervr-architecture/html/topics/rules/do-use-on-chip-memory-efficiently-for-deferred-rendering.html)

하드웨어의 **Tile-Based Deferred Rendering(TBDR)**이라는 이름도 소프트웨어의 G-buffer Deferred Shading과 구분합니다.
전자는 GPU 내부의 타일 기반 처리 구조이고, 후자는 애플리케이션의 표면·조명 계산 배치입니다.
TBDR GPU에서 Forward 셰이딩을 실행할 수도 있습니다.

### 20.2 이름이 비슷한 다른 개념

Deferred Shading은 이 문서의 기본 G-buffer 방식입니다.
Light Pre-pass 또는 Deferred Lighting이라고 부르는 별도 기법은 법선·깊이로 조명 정보를 만들고,
후속 기하 패스에서 재질과 결합하는 구조를 의미하기도 합니다.
우리 클래스 이름 `DeferredLightPass`가 그런 Light Pre-pass 구조 전체를 구현했다는 뜻은 아닙니다.

Visibility Buffer는 표면 속성을 많이 저장하기보다 primitive 등의 가시성 ID를 저장하고
필요한 기하·재질 정보를 나중에 복원·평가하는 계열입니다.
데이터 접근과 미분·텍스처 LOD 등 추가 설계가 필요하며 현재 프로젝트에는 없습니다.
[Burns·Hunt: The Visibility Buffer](https://jcgt.org/published/0002/02/04/)

Render Graph는 패스의 읽기·쓰기 의존성과 수명을 표현하는 설계입니다.
Deferred의 필수 조건도 아니고 Forward에서는 사용할 수 없는 것도 아닙니다.
패스를 클래스 단위로 분리하는 것은 Render Graph로 확장하기 좋은 기반일 수 있지만 둘은 같은 기능이 아닙니다.

<a id="s21"></a>
## 21. 우리 프로젝트 코드 읽기

이 장은 코드 리뷰 목록이 아니라 **앞에서 배운 개념이 어디에 나타나는지 찾는 지도**입니다.
처음 읽을 때 모든 초기화 코드를 이해하려 하지 말고, 출력 데이터와 입력 데이터부터 연결합니다.

| 순서 | 파일 | 찾아볼 개념 |
| --- | --- | --- |
| 1 | [GBuffer.hlsl](../../Deffered/Renderer/Shader/GBuffer.hlsl) | 표면 정보 5개 출력과 Compact 깊이 기록 |
| 2 | [DeferredLight.hlsl](../../Deffered/Renderer/Shader/DeferredLight.hlsl) | G-buffer 조회, 배경 제외, 조명 호출 |
| 3 | [GBufferDebug.hlsli](../../Deffered/Renderer/Shader/GBufferDebug.hlsli) | 역 ViewProjection 위치 복원과 4분할 표시 |
| 4 | [PbrCommon.hlsli](../../Deffered/Renderer/Shader/PbrCommon.hlsli) | 직접광, SH, Specular IBL, 출력 변환 |
| 5 | [InstancedMesh.hlsl](../../Deffered/Renderer/Shader/InstancedMesh.hlsl) | World·법선 변환, SV_InstanceID |
| 6 | [GBufferRenderPass.cpp](../../Deffered/Renderer/Pass/GBufferRenderPass.cpp) | MRT, PSO, Clear, 상태 전환 |
| 7 | [DeferredLightPass.cpp](../../Deffered/Renderer/Pass/DeferredLightPass.cpp) | SRV Heap, Root Signature, 전체 화면 Draw |
| 8 | [Renderer.cpp](../../Deffered/Renderer/Renderer.cpp) | 패스 실행 순서와 GPU 완료 관리 |
| 비교 | [MatchedForward.hlsl](../../Deffered/Renderer/Shader/Pip/MatchedForward.hlsl) | 같은 종류의 표면 정보를 PS 안에서 조명에 바로 사용 |
| 비교 | [MatchedLighting.hlsli](../../Benchmark/Shader/MatchedLighting.hlsli) | 비교 경로 양쪽에서 공유하는 광원 계산 |

공부할 때 아래 한 줄을 손으로 추적해 봅니다.

```text
glTF의 roughness 텍스처 G
 → material factor 곱하기
 → G-buffer Material.g 저장
 → Lighting Pass에서 .y 읽기
 → BRDF / 환경 반사 LOD에 사용
```

그 다음에는 법선과 위치에 대해 같은 추적을 합니다.
이 세 경로를 설명할 수 있으면 API 호출을 모두 외우지 않아도 구조의 중심은 이해한 것입니다.

일반 뷰어와 비교용 셰이더는 완전히 동일한 셰이딩 코드가 아닙니다.
화면 결과를 비교할 때는 경로·파라미터·환경 입력까지 맞춰야 한다는 예로만 기억하면 됩니다.

<a id="s22"></a>
## 22. 디버깅과 검증

### 22.1 확인 순서가 개념 이해를 돕는다

최종 색상이 이상하면 BRDF 수식부터 전부 바꾸지 않습니다.
**기하 → 표면 정보 → 위치 → 조명 → 출력 변환** 순서로 확인하면 어느 단계의 문제인지 분리할 수 있습니다.

| 관찰 | 생각해 볼 개념 |
| --- | --- |
| Base Color에도 모델이 없다 | Draw, Viewport, 좌표 변환, 깊이, RTV 연결 |
| Base Color는 맞는데 Normal이 이상하다 | TBN, 좌표 공간, 인코딩, 미러 변환 |
| Material 색이 예상과 다르다 | glTF 채널과 출력 패킹 구분, factor |
| Full은 맞고 Compact에서만 깨진다 | 깊이 저장, 화면 크기, 역행렬, W 나눗셈 |
| Skybox는 보이지만 금속은 검다 | IBL 리소스, 반사 방향, Metallic, 환경 강도 |
| 전체 색이 지나치게 밝거나 어둡다 | 선형/sRGB, 노출, Tone Mapping 중복 |
| 모델 주위가 검은 사각형이다 | 유효 마스크, 배경 보존, Clear 값 |
| 크기 변경 후에만 이상하다 | 타깃/SRV/Viewport/역행렬 관련 크기 갱신 |

표는 가능한 원인을 좁히는 학습용 분류이며 특정 증상에 원인이 하나뿐이라는 의미가 아닙니다.

### 22.2 디버그 표시를 올바르게 읽기

Base Color는 조명이 없는 재질 색이므로 Lit보다 평평해 보이는 것이 정상입니다.
World Normal의 색은 표면 방향을 표현하며 예쁜 색상을 위한 텍스처가 아닙니다.
Material의 청록·자홍색은 여러 스칼라 값의 패킹 결과이므로 색 자체가 이상해 보여도 오류라고 할 수 없습니다.
Depth가 어둡다면 Range가 큰 것일 수 있으며 데이터가 없는 것과 구분해야 합니다.

### 22.3 도구를 쓰면 무엇을 보는가

D3D12 Debug Layer는 잘못된 상태·바인딩 등의 API 사용 문제를 찾는 데 도움을 줍니다.
GPU 캡처 도구는 Draw, PSO, 바인딩된 리소스, 렌더 타깃의 내용을 추적하는 데 사용합니다.
GPU 타이밍과 CPU 프로파일은 서로 다른 질문에 답합니다.
경고가 없다는 사실만으로 시각적으로 올바르거나 성능이 좋은 구현이라고 결론내리지 않습니다.
이 문서를 작성하면서 새로운 앱 실행·GPU 캡처는 하지 않았습니다.

<a id="s23"></a>
## 23. 뷰어 실습

코드 수정 없이 개념을 확인하는 순서입니다. 속도를 측정하기보다 **변경 전에 결과를 예측하고 화면과 비교**합니다.

### 실습 1. 같은 표면을 다른 정보로 보기

1. 실행 인자 없이 Deffered를 시작하고 Damaged Helmet을 Import합니다.
2. FOCUS 후 카메라·광원 공전을 정지합니다.
3. Lit, Base Color, Normal, Metallic, Roughness, AO, Emissive, Depth를 차례로 봅니다.
4. 각 모드가 조명 전 데이터인지 조명 후 결과인지 적습니다.
5. Overview에서 네 정보가 같은 표면을 가리키는지 확인합니다.

기대 학습: 재질 원본, 화면 공간 표면 정보, 최종 조명 결과를 구분합니다.

### 실습 2. 광원을 바꿔도 재질 정보는 유지되는가

1. Lit에서 Directional Light 방향·강도를 바꿉니다.
2. 같은 카메라에서 Base Color·Normal·Material을 확인합니다.
3. 직접광 설정만 바꿨을 때 재질 특성 자체가 바뀌어야 하는지 생각합니다.

기대 학습: 조명 결과 변화와 표면 정보 변화는 다릅니다.
실제 앱은 매 프레임 Geometry Pass도 다시 실행하지만, 계산 내용의 의존성은 분리되어 있습니다.

### 실습 3. Skybox와 IBL 분리

1. Skybox 표시를 끕니다.
2. 금속 표면의 환경 반사가 남아 있는지 봅니다.
3. Environment 강도를 0으로 바꾼 결과와 비교합니다.

기대 학습: 배경 표시와 환경 조명은 서로 다른 작업입니다.

### 실습 4. Full과 Compact

1. 구도를 고정하고 Full/Compact를 전환합니다.
2. Lit의 하이라이트와 Normal 표시를 관찰합니다.
3. 카메라를 이동해도 Compact 위치 복원이 안정적인지 봅니다.
4. “용량 감소”와 “육안으로 같은 결과”와 “수학적으로 동일”을 구분해 적습니다.

기대 학습: 저장 공간과 정밀도의 교환 관계, 위치를 저장하는 방법의 차이.

### 실습 5. Depth Range

1. Depth 모드에서 카메라를 고정합니다.
2. Range만 바꿉니다.
3. 회색 값은 바뀌지만 물체의 위치·가시성 자체가 바뀌는지 확인합니다.

기대 학습: 디버그 표시용 정규화와 실제 Depth Test는 다릅니다.

### 실습 6. 겹친 두 객체

1. ADD MODEL로 객체를 추가하고 Inspector에서 일부 겹치도록 배치합니다.
2. 한 객체의 위치만 바꾸어 앞뒤 관계를 바꿉니다.
3. Overview에서 겹친 픽셀에 두 표면이 동시에 보관되는지 관찰합니다.

기대 학습: 기본 G-buffer에는 가장 가까운 불투명 표면이 남습니다.

### 실습 7. 이론 계산

1. 1920×1080과 3840×2160의 픽셀 수를 계산합니다.
2. 각 해상도의 Full/Compact 색상 타깃 용량을 구합니다.
3. D32를 더한 비율이 왜 정확히 50% 감소가 아닌지 설명합니다.
4. 저장량 감소만으로 FPS 향상률을 계산할 수 없는 이유를 적습니다.

<a id="s24"></a>
## 24. 자주 하는 질문과 확인 문제

### Q1. Deferred는 GPU가 덜 일하게 하는 기술인가?

무조건 그렇지는 않습니다. 조명 계산 배치를 바꾸는 대신 중간 표면 정보를 저장·조회합니다.
줄어드는 작업과 새로 생기는 작업의 크기가 장면에 따라 다릅니다.

### Q2. G-buffer를 5개 만들면 Draw도 5배인가?

아닙니다. MRT로 한 메시 Draw에서 여러 타깃에 기록합니다. 다만 데이터 쓰기 비용은 증가할 수 있습니다.

### Q3. Lighting Pass는 원본 메시를 다시 그리는가?

우리 구현은 전체 화면 삼각형을 그리고 G-buffer를 읽습니다.
Shadow Pass나 다른 기법의 추가 Geometry Pass와 혼동하지 않습니다.

### Q4. Deferred는 픽셀마다 딱 한 번만 계산하는가?

기본 불투명 표면 정보는 최종 한 표면으로 정리되지만 Geometry overdraw, MSAA,
광원별 패스, 여러 효과 등으로 작업 횟수는 달라집니다. “모든 GPU 작업이 픽셀당 한 번”은 아닙니다.

### Q5. 광원이 많으면 항상 Deferred가 이기는가?

아닙니다. 영향 면적, Shadow 비용, light list, 재질, 대역폭과 Forward+ 같은 기법에 따라 다릅니다.

### Q6. 위치를 복원하면 Vertex Buffer도 필요 없나?

아닙니다. Geometry Pass에서 표면을 화면에 투영하는 데 메시가 필요합니다.
Lighting Pass가 그 정점 버퍼를 다시 읽을 필요가 없는 것입니다.

### Q7. G-buffer에 UV만 넣으면 되지 않나?

가능한 대안이지만 어느 재질·텍스처인지와 샘플링 정보도 필요합니다.
재질 조회·미분·LOD·분기 등을 후속 단계에서 처리해야 하므로 단순히 채널 두 개로 모든 문제가 해결되지는 않습니다.

### Q8. Depth Buffer와 깊이를 저장한 색상 타깃은 같은가?

현재는 별도 리소스입니다. DSV는 깊이 판정, Compact의 R32_FLOAT 타깃은 조명에서 위치 복원에 사용합니다.
적절한 typeless 리소스·DSV/SRV 포맷과 상태 설계로 깊이 리소스를 직접 읽는 대안도 있습니다.

### Q9. Compact는 무손실 압축인가?

아닙니다. 저장 포맷 변경과 위치 복원을 조합한 레이아웃입니다.
법선·발광 등의 정밀도 차이가 생길 수 있으며 타깃 메모리의 실제 하드웨어 압축과도 다른 이야기입니다.

### Q10. Barrier는 GPU 전체를 멈추는가?

필요한 의존성과 상태를 표현하는 동기화입니다. CPU가 GPU 전체 완료를 기다리는 Fence 대기와 같지 않습니다.
과도하거나 부정확한 Barrier는 비용과 오류의 원인이 될 수 있습니다.

### Q11. RTV와 SRV는 픽셀 복사본 두 개인가?

아닙니다. 같은 리소스에 서로 다른 View를 만들 수 있습니다.
descriptor를 만드는 것과 실제 텍스처 데이터를 복사하는 것은 다른 작업입니다.

### Q12. CBV에 정점과 조명을 한 번에 넣으면 더 간단하지 않나?

정점은 메시의 구조, 상수는 프레임·객체·재질별 설정처럼 사용 방식과 변경 주기가 다릅니다.
IA의 정점 입력, 상수 버퍼 접근, 인스턴스 버퍼를 구분하면 재사용과 업데이트를 설계하기 좋습니다.
특수한 통합 설계도 가능하지만 바인딩 호출 하나를 줄이는 것만으로 전체 비용이 줄었다고 판단하지 않습니다.

### Q13. 화면에 Skybox가 보이면 IBL도 적용된 것인가?

아닙니다. 배경 Draw와 표면 환경 조명은 별도입니다.

### Q14. G-buffer가 있으면 SSAO·SSR·그림자도 구현된 것인가?

아닙니다. 필요한 입력을 일부 갖춘 것이며 각 알고리즘과 추가 리소스·패스를 구현해야 합니다.

### Q15. PBR은 Deferred에 종속되는가?

아닙니다. PBR 반사 모델과 렌더링 구조는 별개의 선택입니다.

### 스스로 설명해 보기

아래 질문에 문서를 보지 않고 답한 뒤 해당 장과 비교합니다.

1. 한 픽셀에 표면 세 개가 겹쳤을 때 Forward와 Deferred의 작업을 그림으로 그릴 수 있는가?
2. Geometry Pass에서 조명 없이 Normal Map을 계산하는 이유는 무엇인가?
3. Metallic-Roughness 입력 채널과 Material G-buffer 채널은 어떻게 다른가?
4. 역 ViewProjection 뒤에 W로 나누는 이유는 무엇인가?
5. 컬링 OFF인데도 뒤 물체가 보이지 않는 이유는 무엇인가?
6. 유리와 불투명 표면의 속성을 평균 내서 조명하면 왜 올바른 합성이 아닌가?
7. G-buffer를 절반으로 줄여도 GPU 시간이 거의 같을 수 있는 이유 세 가지는 무엇인가?
8. Tiled Deferred와 Forward+는 무엇을 공유하고 무엇이 다른가?

<details>
<summary>답을 먼저 생각한 뒤 확인하는 해설</summary>

1. 뒤→앞 순서라면 Forward는 여러 후보의 조명을 계산할 수 있고 Deferred도 여러 후보의 재질을 평가할 수 있습니다. 최종 G-buffer에는 가장 가까운 표면이 남고 그 표면을 조명합니다. Early-Z와 순서에 따라 실제 실행은 달라집니다.
2. Normal Map은 표면 방향을 만드는 재질 평가입니다. 광원이 없어도 정할 수 있고, 이후 조명에서 그 방향을 사용할 수 있습니다.
3. glTF 입력은 G=Roughness, B=Metallic이며 AO는 별도 참조의 R입니다. 우리 G-buffer 출력은 R=Metallic, G=Roughness, B=AO입니다.
4. 역변환 결과는 동차 좌표이므로 xyz/w까지 해야 실제 월드 위치가 됩니다.
5. 객체 컬링과 깊이 테스트는 별개입니다. 객체를 제출했더라도 뒤쪽 fragment는 깊이 테스트에서 제외될 수 있습니다.
6. BRDF가 법선·재질 값에 대해 선형이 아니기 때문입니다. 속성의 평균을 조명한 결과와 각 표면을 조명해 합성한 결과는 다릅니다.
7. geometry 비용이 지배적이거나, 다른 패스가 오래 걸리거나, 외부 대역폭보다 연산이 병목일 수 있습니다. 캐시·압축 때문에 포맷 크기 비율이 실제 트래픽 비율과 다를 수도 있습니다.
8. 둘 다 타일 광원 목록을 활용할 수 있습니다. Tiled Deferred는 저장된 표면 정보를 조명하고 Forward+는 메시의 Forward PS에서 재질과 조명을 함께 평가합니다.

</details>

<a id="s25"></a>
## 25. 포트폴리오 설명과 참고 자료

### 25.1 원리를 이해한 뒤 압축할 문장

> Forward 경험을 바탕으로 표면 정보 생성과 조명 평가를 분리한 Deferred 파이프라인을 구현했습니다.
> Geometry Pass에서 MRT로 재질·법선·깊이 정보를 기록하고, Lighting Pass에서 이를 읽어 PBR과 IBL을 계산합니다.
> 이후 깊이 기반 위치 복원과 저장 포맷 변경으로 색상 G-buffer의 픽셀당 데이터 용량을 40B에서 20B로 줄였습니다.

이어 설명할 학습 내용은 **가시성, 표면 정보의 데이터 계약, 명시적 리소스 상태 전환, 정밀도와 저장 비용의 교환**입니다.
인스턴싱은 기존 기능, G-buffer 시각화는 결과 확인 도구입니다.
원본 S.T.L 전체의 성능이 향상되었다거나 모든 Forward보다 빠르다고 주장하지 않습니다.

### 25.2 자료를 읽는 목적

| 자료 | 읽을 때 집중할 내용 |
| --- | --- |
| [GPU Gems 3: Deferred Shading](https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-19-deferred-shading-tabula-rasa) | 기하·조명 분리의 동기와 저장 비용 |
| [Microsoft HLSL Semantics](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics) | SV_POSITION, SV_TARGET, 정점 ID |
| [Microsoft Texture Load](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-load) | 정수 좌표 기반 G-buffer 접근 |
| [Microsoft Descriptor Heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview) | 데이터 리소스와 View 저장소의 구분 |
| [Microsoft Root Signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures-overview) | 셰이더 바인딩 계약 |
| [Microsoft Resource Barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12) | 리소스 사용 상태와 전환 |
| [Microsoft Fence-Based Resource Management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management) | GPU 작업 완료와 메모리 재사용 |
| [NVIDIA Depth Precision](https://developer.nvidia.com/blog/visualizing-depth-precision/) | 원근 깊이의 비선형성과 Reversed-Z |
| [PBRT Microfacet Theory](https://www.pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory) | D·G·거칠기와 반사 모델 |
| [Khronos glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) | 입력 재질·텍스처의 의미 |
| [AMD Forward+](https://github.com/GPUOpen-LibrariesAndSDKs/ForwardPlus11) | 타일 광원 목록을 Forward에서 사용 |
| [Clustered Shading 연구](https://research.chalmers.se/publication/161725) | 공간 분할을 양쪽 렌더링에 적용 |
| [Epic Forward Shading](https://dev.epicgames.com/documentation/en-us/unreal-engine/forward-shading-renderer-in-unreal-engine) | 엔진별 AA·기능 선택의 실제 사례 |
| [Microsoft Rasterization Rules](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-rasterizer-stage-rules) | 픽셀·sample·coverage와 MSAA |
| [Imagination 타일 메모리](https://docs.imgtec.com/starter-guides/powervr-architecture/html/topics/rules/do-use-on-chip-memory-efficiently-for-deferred-rendering.html) | GPU 구조에 따른 중간 타깃 저장 비용 |
| [Visibility Buffer 논문](https://jcgt.org/published/0002/02/04/) | 표면 속성 대신 가시성 ID를 저장하는 다른 설계 |

외부 자료의 개념을 우리 코드와 연결해 설명한 것이며 문장이나 구현 전체를 그대로 옮긴 문서가 아닙니다.
PBRT는 PBR 이론 참고서이지 이 프로젝트의 Deferred 패스 설계 명세가 아닙니다.
GPU Gems·AMD 예제의 옛 API 조건과 특정 엔진의 지원 제한은 일반 이론과 구분해서 읽습니다.

### 마지막 정리

```text
Forward  = 표면이 처리될 때 재질과 조명을 함께 평가
Deferred = 표면 정보를 기록한 뒤 조명 평가

G-buffer = 조명에 필요한 화면 공간의 표면 데이터
Compact  = 위치 복원 + 저장 포맷 선택으로 데이터량 감소

판단 기준 = 장면, 광원, 재질, 해상도, 대역폭, CPU/GPU 병목, 필요한 기능
```

이 문서의 목표는 어떤 방식이 승자인지 외우는 것이 아닙니다.
**어떤 정보를 언제 만들고, 어디에 저장하고, 다음 단계에서 왜 읽는지 설명할 수 있으면 원리를 이해한 것입니다.**


