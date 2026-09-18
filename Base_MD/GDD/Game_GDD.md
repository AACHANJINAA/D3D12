# Engine Design

## 목표

Windows에서 실행되는 C++20 / Direct3D 12 3D 엔진을 구축합니다.
첫 결과물은 Win32 창에 정점 색상 삼각형을 지속적으로 표시하는 실행 파일입니다.
이후 카메라, 메시, 재질, 조명으로 확장하고 동일 장면을 Forward / Deferred로 그립니다.

## 확정 방향

- Forward부터 시작하고 패스 단위로 Deferred를 확장합니다.
- 시스템 관리는 Manager, 장면 오브젝트 구성은 Component 패턴을 사용합니다.
- 삼각형 코드 전에 Base_MD 설계를 고도화합니다.

## 단계별 시각 목표

1. 색상 삼각형: 창, GPU 명령 제출, 화면 출력.
2. 깊이 테스트가 있는 3D 메시: Transform과 Camera.
3. 텍스처와 Forward 조명: 재질과 업로드.
4. PBR / HDR: 재질 모델과 톤 매핑.
5. Deferred: G-buffer와 조명 패스, Forward 결과 비교.

PBR은 셰이딩 모델이고 Forward / Deferred는 조명 계산을 배치하는 방식입니다.
삼각형 단계는 조명을 사용하지 않습니다.

## 제외 범위

초기 단계에 에디터, 물리, 애니메이션, 레이트레이싱, Render Graph,
멀티스레드 렌더링, 다중 큐, 범용 ECS를 포함하지 않습니다.
게임 장르와 콘텐츠는 미정입니다.
