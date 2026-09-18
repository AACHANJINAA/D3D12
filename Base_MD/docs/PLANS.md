# Plans

| 작업 | 상태 | 근거 |
| --- | --- | --- |
| M0 문서 고도화 | 완료 | 목표, 소유권, 패스, 검증 기준 작성 |
| 환경 조사 | 일부 완료 | C++20 / v145 / SDK 10.0 설정 확인. 설치 미검증 |
| M1 삼각형 | 대기 | 이번 범위는 문서. C++ / HLSL 미작성 |
| M2 Component 장면 | 대기 | M1 이후 |
| M3 Forward 조명 | 대기 | M2 이후 |
| M4 PBR / HDR | 대기 | M3 이후 |
| M5 Deferred | 대기 | M4 이후 |

## 다음 작업

M1: 도구 확인 -> 창 / Application -> RHI / RenderManager ->
ForwardPass / HLSL -> Fence -> resize / 종료 -> 빌드와 실제 실행.

## 미확정 항목

- Win32 진입점과 기존 Console 설정 처리: M1에서 결정.
- 셰이더 컴파일러 / Shader Model, 어댑터 지원: M1에서 확인.
- 백버퍼 2개와 단일 Direct 큐: 제안 기본값.
- 좌표계 / 행렬 규약: M2 전에 확정.
- G-buffer 포맷: M5에서 재질과 메모리 기준으로 결정.
- Git: 현재 저장소 없음. 초기화는 별도 요청 시 수행.

## 보류와 검증 상태

에디터, Render Graph, Forward+, 레이트레이싱, 물리, 애니메이션은 보류합니다.
프로젝트 설정과 문서만 확인했습니다. 빌드와 GPU / 화면 검증은 아직 미실행입니다.
