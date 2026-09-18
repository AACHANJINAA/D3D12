# Work Request

최신 직접 요청의 보조 문서이며 요청서 작성은 필수가 아닙니다.

## 현재 요청: M0

- 대상: Base_MD 기존 문서.
- 요청: Forward 시작, 패스 단위 Deferred 확장, Manager / Component 구체화.
- 결과: 책임 / 수명 계약, 단계별 계획과 삼각형 완료 기준.
- 범위: 문서 고도화. C++ / HLSL 구현은 다음 작업.
- 완료 기준: 문서와 링크의 일관성, 확정 / 제안 / 구현 상태 구분.

## 다음 요청 예시: M1

- 요청: 기존 VS 프로젝트에서 DX12 정점 색상 삼각형 구현.
- 대상: App, Engine/Core, RHI, Renderer, Shaders, 프로젝트 등록.
- 참조: ARCHITECTURE, Implementation Plan의 M1, Verify.
- 결과: 빌드 가능한 코드와 실제 실행 검증.
- 제한: Scene / Component, 조명은 이후 단계.
- 완료 기준: Verify의 M1 항목.

새 요청에는 대상, 원하는 동작, 수정 제한, 완료 기준을 필요한 만큼 적습니다.
빈 항목은 기존 결정과 보수적인 기본값으로 처리하며 중요한 모호성만 확인합니다.
