# D3D12 Engine Guide

C++ / Direct3D 12 기반 3D 그래픽스 엔진의 설계와 구현 기준입니다.
Forward로 시작하고 불투명 렌더링을 패스 단위로 Deferred로 확장합니다.
Manager는 시스템 수명과 실행 순서, Component는 장면 데이터와 기능을 담당합니다.

## 현재 상태

엔진 코드는 아직 없습니다. 기존 D3D12.slnx와 D3D12/D3D12.vcxproj를 사용합니다.
프로젝트 설정은 C++20, MSVC v145, Windows SDK 10.0입니다.
도구 설치와 빌드는 미검증이며 우선 대상은 Debug / x64입니다.
현재 루트는 Git 저장소가 아닙니다.

## 문서 안내

| 문서 | 책임 |
| --- | --- |
| [Engine Design](GDD/Game_GDD.md) | 엔진 목표와 에디터 범위 |
| [Implementation Plan](GDD/Implemention_Plan.md) | 마일스톤과 완료 기준 |
| [Architecture](docs/ARCHITECTURE.md) | 모듈, 소유권, 렌더 패스 |
| [Plans](docs/PLANS.md) | 현재 상태와 다음 작업 |
| [Verification](docs/Verify.md) | 빌드와 실행 검증 |
| [Decisions](docs/DECISIONS.md) | 확정 결정 |
| [Work Request](docs/PLAY_REQUEST.md) | 작업 범위 |
| [AI Usage](docs/AI_USAGE.md) | 최소 참조 규칙 |
| [Workflow](docs/WORKFLOW.md) | 진행과 갱신 절차 |

최신 사용자 요청이 문서보다 우선합니다. 요청서 작성은 선택 사항입니다.
확정 방향, 제안 기본값, 구현 상태, 검증 결과를 구분합니다.
Implemention_Plan.md 파일명은 기존 링크 호환성을 위해 유지합니다.

## C++ 코드 컨벤션

- 클래스명은 대문자로 작성합니다. 예: `RENDERMANAGER`, `FORWARDPASS`.
- 일반 변수명은 소문자로 작성합니다. 여러 단어는 언더스코어로 구분합니다.
- `bool` 변수명은 `is`를 접두사로 사용합니다. 예: `isinitialized`, `isvisible`.
- 멤버 변수명은 이름 앞에 `_`를 붙입니다. 예: `_device`, `_frame_index`.
- 함수명은 소문자로 작성합니다. 합성어는 언더스코어로 구분합니다. 예: `create_device`, `record_commands`.
- 기존 코드와 충돌하는 경우 새 코드부터 이 규칙을 적용하고, 무관한 코드는 함께 개명하지 않습니다.

## 빌드 확인

빌드와 실행 확인은 사용자가 직접 수행합니다. AI는 코드와 프로젝트 설정을 작성한 뒤,
실행하지 않은 빌드·화면 출력·GPU 검증을 성공으로 기록하지 않습니다.
