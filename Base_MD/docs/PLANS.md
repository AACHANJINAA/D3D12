# Plans

## 현재 주 계획 (2026-10-02)

[S.T.L 대비 렌더러 성능 검증 계획](RENDERER_BENCHMARK_PLAN.md)을 주 개발 목표로 사용합니다.
다음 작업은 B0: 기준선 고정, CPU/GPU 계측, CSV 저장 및 동일 비교 장면 구성입니다.
구현 유무보다 재현 가능한 개선 수치를 우선하며, 성능 결과는 아직 미측정입니다.

## 초기 계획 기록

아래 표와 항목은 초기 설계 당시의 기록이며 현재 구현 상태나 다음 작업을 나타내지 않습니다.
현재 구현은 루트 README, 현재 우선순위는 위 성능 검증 계획을 기준으로 판단합니다.

| 작업 | 상태 | 근거 |
| --- | --- | --- |
| M0 문서 고도화 | 완료 | 목표, 소유권, 패스, 검증 기준 작성 |
| 환경 조사 | 일부 완료 | C++20 / v145 / SDK 10.0 설정 확인. 설치 미검증 |
| M1 삼각형 | 대기 | 이번 범위는 문서. C++ / HLSL 미작성 |
| M2 Component 장면 | 대기 | M1 이후 |
| M3 glTF Import / Forward 조명 | 대기 | M2 이후 |
| M4 PBR / HDR | 대기 | M3 이후 |
| M5 Deferred | 대기 | M4 이후 |
| M6 Rendering Editor | 대기 | M5 이후 |

## 다음 작업

M1: 도구 확인 -> 창 / Application -> RHI / RenderManager ->
ForwardPass / HLSL -> Fence -> resize / 종료 -> 사용자의 빌드와 실제 실행 확인.

## 미확정 항목

- Win32 진입점과 기존 Console 설정 처리: M1에서 결정.
- 셰이더 컴파일러 / Shader Model, 어댑터 지원: M1에서 확인.
- 백버퍼 2개와 단일 Direct 큐: 제안 기본값.
- 좌표계 / 행렬 규약: M2 전에 확정.
- G-buffer 포맷: BaseColor / Normal / Metallic-Roughness-AO / Depth를 제안 기본값으로 사용.
- glTF Import 라이브러리와 Scene 저장 포맷: M3 및 M6에서 결정.
- Git: 현재 저장소 없음. 초기화는 별도 요청 시 수행.

## 보류와 검증 상태

Vertex / Edge / Polygon Mesh 편집, Render Graph, Forward+, 레이트레이싱, 물리, 애니메이션은 보류합니다.
프로젝트 설정과 문서만 확인했습니다. 빌드와 GPU / 화면 검증은 아직 미실행입니다.
