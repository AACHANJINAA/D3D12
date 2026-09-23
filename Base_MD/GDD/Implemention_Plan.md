# Implementation Plan

실제 진행 상태는 [Plans](../docs/PLANS.md)에서 관리합니다.

| 단계 | 범위 | 선행 조건 | 완료 기준 |
| --- | --- | --- | --- |
| M0 | 문서 설계 | 사용자 방향 | 책임, 패스, 삼각형 기준 명시 |
| M1 | Win32 / DX12 삼각형 | M0, 도구 설치 | x64 빌드, 삼각형, resize / 종료, GPU 오류 없음 |
| M2 | Component 장면 | M1 | Transform, Camera, MeshRenderer로 3D 메시 출력 |
| M3 | glTF Import / Forward 조명 | M2 | Mesh, Texture, Material Import와 Light 출력 |
| M4 | PBR / HDR | M3 | Normal, ORM, Emissive, 선형 셰이딩, 톤 매핑 |
| M5 | Deferred | M4 | G-buffer, Deferred Lighting, 투명 Forward |
| M6 | Rendering Editor | M5 | Scene, Inspector, Light 설정, Debug View, Scene 저장 |

## M1 작업 순서

1. 기존 vcxproj에 C++ / HLSL과 빌드 설정을 등록합니다.
2. Application / Window와 RenderManager 초기화, 종료를 구현합니다.
3. Device, 단일 Direct 큐, SwapChain, RTV, 프레임 컨텍스트를 구현합니다.
4. VS / PS, Root Signature, PSO와 삼각형 정점 버퍼를 생성합니다.
5. ForwardPass에서 바인딩과 DrawInstanced를 기록합니다.
6. 프레임 Fence, 백버퍼 상태 전환, resize / 최소화 / 오류 처리를 구현합니다.
7. Debug / Release x64 빌드와 실제 화면을 검증합니다.

M1에는 RenderManager와 ForwardPass부터 도입합니다.
SceneManager와 Component는 M2에서, AssetManager는 에셋 요구가 생기는 M3에서 도입합니다.
M1 삼각형 버퍼는 패스가 소유하며 M2에서 메시 에셋으로 이동합니다.

## 에디터 구현 범위

에디터는 Mesh 내부 구조를 직접 편집하지 않습니다.
glTF를 Import하고 Scene에 배치한 뒤 Transform, Material, Light와 렌더링 모드를 조작합니다.

```text
Scene View -> Entity 선택 -> Inspector 수정 -> Renderer 반영 -> Scene 저장
```

지원할 Debug View는 Lit, Unlit, Wireframe, Normal, Depth, BaseColor,
Metallic, Roughness, AO, Emissive입니다.

## 제안 기본값

기존 Visual Studio 프로젝트, Win32 API, 백버퍼 2개, 단일 Direct 큐를 사용합니다.
셰이더 컴파일러 / Shader Model과 Feature Level은 도구 및 GPU 지원 확인 후 결정합니다.
하드웨어 사용 실패 시 WARP 옵션을 검토합니다.

## Deferred 전환

ForwardPass 이름 변경만으로 전환할 수 없습니다.
G-buffer 텍스처와 RTV / SRV, 출력 포맷별 PSO, 재질 출력 셰이더,
깊이 공유, 조명 입력과 상태 전환을 함께 추가합니다.
Render Graph와 Forward+는 필요성과 프로파일 결과를 확인한 뒤 별도로 결정합니다.

초기 G-buffer는 BaseColor, World Normal, Metallic / Roughness / AO와 Depth로 구성합니다.
World Position은 별도 타깃에 저장하지 않고 Depth와 Camera 행렬로 복원하는 것을 기본값으로 둡니다.
