# Decisions

확정 결정만 기록합니다. 제안 세부사항은 Implementation Plan / PLANS에 둡니다.

| 날짜 / ID | 결정 | 근거 / 영향 |
| --- | --- | --- |
| 2026-09-18 / D001 | C++ / DX12 3D 엔진 | 사용자 목표. 기존 VS 프로젝트 활용 |
| 2026-09-18 / D002 | Forward 시작, 패스 단위 Deferred 확장 | 사용자 선택. 장면과 GPU 관리 분리 |
| 2026-09-18 / D003 | Manager / Component 패턴 | 사용자 선택. 시스템과 오브젝트 책임 분리 |
| 2026-09-18 / D004 | 첫 구현은 삼각형 | 최소 렌더 경로 검증 |
| 2026-09-18 / D005 | Base_MD 고도화 선행 | 구현 전 책임과 완료 기준 정리 |
| 2026-09-23 / D006 | 최종 결과물은 Rendering Editor | S.T.L. 렌더링 기술을 Import, Light, Debug View까지 확장 |
| 2026-09-23 / D007 | glTF Import를 기본 에셋 입력으로 사용 | S.T.L.에서 검증한 리소스 파이프라인 재사용 |
| 2026-09-23 / D008 | Mesh 직접 편집은 제외하고 Import / Transform 편집에 집중 | 렌더링과 리소스 처리 범위에 집중 |
| 2026-09-23 / D009 | Deferred Rendering까지 구현 | 다중 Light와 G-buffer 기반 렌더링 역량 증명 |
| 2026-09-23 / D010 | Debug View를 지원 | PBR Texture와 G-buffer 전달 검증 |

Singleton, 셰이더 컴파일러, 좌표계, glTF Import 라이브러리, Scene 저장 포맷은 미확정입니다.
