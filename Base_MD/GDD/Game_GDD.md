# Engine Design

## 목표

Windows에서 실행되는 C++20 / Direct3D 12 3D 엔진을 구축합니다.
첫 결과물은 Win32 창에 정점 색상 삼각형을 지속적으로 표시하는 실행 파일입니다.
최종 결과물은 glTF 리소스를 Import하고, PBR 재질과 Light를 설정하며,
Forward와 Deferred 렌더링 결과를 확인할 수 있는 소형 3D 렌더링 에디터입니다.

## 확정 방향

- Forward부터 시작하고 패스 단위로 Deferred를 확장합니다.
- 시스템 관리는 Manager, 장면 오브젝트 구성은 Component 패턴을 사용합니다.
- 삼각형 코드 전에 Base_MD 설계를 고도화합니다.
- Mesh의 직접 Vertex / Edge / Polygon 편집은 구현하지 않고 Import와 Transform 편집에 집중합니다.
- 외부 모델 입력 포맷은 glTF를 기본으로 사용하고 Scene 저장은 엔진 전용 포맷으로 관리합니다.
- PBR Texture와 G-buffer를 개별 출력하는 Debug View를 제공합니다.

## 단계별 시각 목표

1. 색상 삼각형: 창, GPU 명령 제출, 화면 출력.
2. 깊이 테스트가 있는 3D 메시: Transform과 Camera.
3. glTF Import와 Forward 조명: Mesh, Texture, Material, Light.
4. PBR / HDR: BaseColor, Normal, ORM, Emissive, 톤 매핑.
5. Deferred: G-buffer와 Deferred Lighting.
6. Rendering Editor: Scene, Inspector, Light 설정, Debug View, Scene 저장.

PBR은 셰이딩 모델이고 Forward / Deferred는 조명 계산을 배치하는 방식입니다.
삼각형 단계는 조명을 사용하지 않습니다.

## 초기 제외 범위

초기 단계에 Vertex / Edge / Polygon Mesh 편집, 물리, 애니메이션, 레이트레이싱,
Render Graph, 멀티스레드 렌더링, 다중 큐, 범용 ECS를 포함하지 않습니다.
에디터는 Resource Import, Scene 배치, Material / Light 설정과 Debug View부터 구현합니다.
