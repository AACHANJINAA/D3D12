# S.T.L 대비 렌더러 발전 및 성능 검증 계획

작성일: 2026-10-02
상태: 주 개발 목표 확정 / 성능 계측 및 비교 실행 전

## 1. 주 목표

PIP의 S.T.L 프로젝트에서 사용한 렌더링 구조를 분석하고, 동일한 입력과 출력 품질에서
새 D3D12 렌더러의 개선점을 CPU 시간, GPU 시간, 메모리 사용량으로 입증한다.

핵심 결과물은 기능 목록이 아니라 **병목 발견 -> 개선 설계 -> 동일 조건 측정 -> 결과와 한계 설명**이다.
Deferred라는 이름 자체나 게임보다 가벼운 뷰어라는 이유로 성능 향상을 주장하지 않는다.
이 문서를 향후 개발 우선순위의 기준으로 사용한다. 기존 장기 계획과 충돌하면 이 계획을 우선한다.

## 2. 기준 구현과 현재 상태

- 비교 기준: `C:/Users/ckswl/Desktop/GITHUB/PIP/Client/Client`
- 개발 대상: `C:/Users/ckswl/Desktop/GITHUB/D3D12/Deffered`
- 독립 비교 실행부: `C:/Users/ckswl/Desktop/GITHUB/D3D12/Forward`
- 공통 입력: 루트 `Benchmark/static-grid.json`. PIP 정적 경로의 포팅 범위와 원본과의 차이는
  `Forward/PipReference/PORTING_NOTES.md` 참고. 기준선 분리와 입력 고정은 구현했지만
  B0 계측, 출력 품질 동등성 및 실제 성능 비교는 아직 완료하지 않았다.
- 아래 내용은 2026-10-02 소스 확인 결과이며 실측 성능 결과가 아니다.

| 항목 | PIP S.T.L | 현재 D3D12 렌더러 | 의미 |
| --- | --- | --- | --- |
| 메시 조명 | Forward에서 PBR 및 IBL 계산 | G-buffer와 Deferred Lighting 분리 | 다중 광원 처리 비용 비교 후보 |
| 반복 메시 | 메시별 인스턴싱 경로 | 객체 및 primitive별 개별 Draw | 현재 구현의 보완 과제 |
| 가시성 | 거리 및 Frustum 컬링 | 객체 표시 여부 검사 | 컬링 추가만으로 신규 차별점은 아님 |
| 프레임 동기화 | 재사용할 프레임의 Fence 대기 | 매 프레임 GPU 완료 대기 | CPU/GPU 중첩 실행 구조 정비 필요 |
| Present | `Present(0, ...)` | `Present(1, 0)` | 현재 FPS 직접 비교는 불공정 |
| 광원 | `Light.hlsl`의 최대 광원 배열 크기 16 | Directional Light 중심 | Point Light와 공통 조명 조건부터 구축 |

소스 근거:

- PIP: `Renderer.cpp`의 렌더 목록 구성 및 glTF 인스턴스 그룹, `GameFramework.cpp`의 `MoveToNextFrame`.
- PIP: `Gltf_Shader.hlsl`의 `PS_GLTF`, `Light.hlsl`의 `MAX_LIGHTS` 및 조명 루프.
- 현재: `Renderer.cpp`의 `move_to_next_frame`, `Pass/GBufferRenderPass.*`, `SceneRenderData.cpp`.
- PIP에는 Occlusion 관련 코드도 있지만 존재만으로 활성화된 성능 기능이라고 간주하지 않는다. 실제 비교 경로에서의 호출 여부를 확인한다.

## 3. 비교를 두 종류로 분리

### A. S.T.L 대비 비교

동일 자산, 장면 배치, 카메라, 광원, 재질 및 품질 조건에서 두 렌더러를 비교한다.
PIP의 인스턴싱과 컬링을 끄고 새 렌더러만 켜는 식으로 유리한 조건을 만들지 않는다.
공통 기능만 켠 비교와 각 렌더러의 최적화 경로 비교를 구분해 기록한다.

### B. 새 렌더러 내부 개선 전후 비교

동일 실행 파일의 옵션 또는 고정된 전후 버전으로 한 번에 한 가지 변경만 비교한다.
프레임 동기화, G-buffer 압축, 광원 컬링 각각의 기여를 분리한다.
이 결과를 S.T.L 대비 수치로 바꾸어 표현하지 않는다.

## 4. 대표 개선 주제

### 4.1 다중 광원 처리와 Tiled Deferred

1. Point Light와 동일한 BRDF, 감쇠, 범위 및 광량 조건을 두 비교 경로에 마련한다.
2. 동일 장면에서 광원 수를 1, 4, 8, 16개로 증가시킨다.
3. 전체 렌더링 GPU 시간과 조명 관련 시간을 측정한다.
4. 이후 화면 타일별 영향 광원 목록을 사용하는 Tiled Deferred를 구현한다.
5. 단순 Deferred와 Tiled Deferred를 동일 조건에서 비교한다.

- 주요 지표: 전체 GPU ms, G-buffer ms, 조명 ms, 타일 광원 목록 구축 ms, 평균 및 최대 타일당 광원 수.
- 광원 개수뿐 아니라 화면 점유율, 반경, 서로 겹치는 정도를 고정하고 기록한다.
- Forward는 재질과 조명이 한 패스에 있으므로 해당 패스 시간을 Deferred 조명 패스만과 직접 비교하지 않는다.
- 주요 비교는 전체 메시 및 조명 처리 비용으로 하고 패스별 시간은 원인 분석에 사용한다.
- 광원이 적을 때 Deferred가 더 느리거나 타일 구축 비용이 이득보다 큰 경우도 그대로 보고한다.
- 16개를 넘는 실험은 새 렌더러 내부 확장성 실험으로 분리한다. PIP 확장 시 수정 내용과 새 기준선을 별도 기록한다.
- 광원 목록 용량 초과 시 누락된 조명이 생기지 않도록 초과 감지와 안전한 처리 경로를 마련한다.

### 4.2 G-buffer 저장량과 처리 비용

현재 색상 G-buffer 포맷은 픽셀당 `4 + 8 + 4 + 8 + 16 = 40`바이트다.
월드 위치 `RGBA32F` 저장 대신 Depth와 역행렬로 위치를 복원하면 색상 타깃 저장량은 24바이트가 된다.

**이론적 목표: 색상 G-buffer 저장량 40 -> 24바이트/픽셀, 40% 감소.**

- 이 계산은 Depth, 백버퍼, 정렬, 드라이버 압축 및 기타 리소스를 제외한 명목상 저장량이다.
- 실제 VRAM 사용량, 전체 메모리 대역폭 또는 GPU 시간의 40% 감소를 의미하지 않는다.
- 주요 지표: 타깃별 할당 크기, 전체 G-buffer 할당량, G-buffer 패스 및 전체 GPU ms.
- Depth SRV, 리소스 상태 전환, 역행렬/좌표 규약, 리사이즈 및 근거리/원거리 복원 정확도를 검증한다.
- 선택 외곽선, 재질 디버그 뷰 및 깊이 가림이 유지되어야 한다.
- 이후 Normal 등의 추가 압축은 화질 오차를 확인하며 독립된 실험으로 진행한다.

### 4.3 CPU 제출 비용과 객체 확장성

- 프레임별 allocator, 상수 버퍼, Fence 값 및 필요한 descriptor 수명을 분리한다.
- 리소스를 다시 쓰거나 해제할 때 필요한 완료 시점만 기다린다. Fence 대기 코드를 단순 삭제하지 않는다.
- 반복 메시 인스턴싱과 Frustum 컬링을 추가하고 PIP의 기존 기능과 동일 조건으로 비교한다.
- 주요 지표: 렌더링 CPU 작업 ms, Fence 대기 ms, Present 대기 ms, Draw Call 수, 제출 객체 수, 프레임 p95.
- 현재 객체 제한은 128개이므로 먼저 1, 16, 64, 128개를 사용한다. 더 큰 부하는 제한과 버퍼 용량 정비 후 추가한다.
- 동일 메시 반복 장면과 서로 다른 메시/재질 장면을 분리한다. 무작정 Draw Call 수만 줄이는 것을 목표로 삼지 않는다.

## 5. 측정 규칙

| 조건 | 기준 |
| --- | --- |
| 하드웨어 | 동일 PC, GPU, 드라이버, 전원 설정 및 모니터 조건 |
| 빌드 | Release x64, 디버그 레이어와 GPU 검증 비활성화. 정확성 검사는 별도 실행 |
| 해상도 | 우선 1920x1080 고정. 실제 내부 렌더 해상도 기록 |
| Present | VSync 및 프레임 제한 조건 통일. 처리량과 동기화 사용 시 체감 성능을 별도 실험 |
| 장면 | 자산 버전/해시, 변환, 카메라, 광원, 난수 seed 고정 |
| 품질 | BRDF, IBL, 노출, 톤매핑, 텍스처 해상도/mip, 그림자, AA 설정 통일 또는 차이 명시 |
| 부가 기능 | 게임 로직, 물리, 네트워크 비용 제외. UI와 선택 외곽선도 동등하게 비활성화 |
| 실행 길이 | 초기안: 10초 워밍업 후 30초 측정, 조건별 3회. 측정 중 로딩/컴파일 제외 |
| 순서 | 비교 순서를 교차하고 발열 및 백그라운드 작업에 의한 이상 구간 기록 |
| 요약 | 실행별 평균, 중앙값, p95 프레임 시간과 반복 실행 간 변동 보고 |

- CPU 시간과 GPU 시간을 분리한다. 중첩 실행되므로 단순 합을 전체 프레임 시간으로 취급하지 않는다.
- CPU는 렌더 준비/명령 기록, Fence 대기, Present 호출 구간을 구분한다.
- GPU는 Timestamp Query와 큐의 Timestamp Frequency로 측정한다. 결과 읽기 때문에 매 프레임 GPU를 기다리지 않는다.
- PIX Timing Capture로 CPU/GPU 작업과 대기 원인을 교차 확인한다. 캡처 도구의 오버헤드도 별도로 고려한다.
- 메모리는 렌더러가 소유한 리소스 할당량과 프로세스 비디오 메모리 사용량을 구분한다.
- 내부 실험의 화질 검증은 고정 카메라 캡처, 차영상 및 가능하면 선형 HDR 오차 지표를 사용한다.
- 샘플 부족, 측정 실패, 미지원 항목을 0으로 저장하지 않는다. 미측정 상태를 명시한다.
- 개선율은 `(기준 시간 - 개선 시간) / 기준 시간 * 100`으로 계산하며 기준 조건을 함께 표시한다.

## 6. 계측 결과 형식

실행 단위 메타데이터에는 다음을 기록한다.

`run_id, renderer, commit, dirty_diff_id, scene_id, asset_hashes, gpu, driver, build, resolution, vsync, quality_settings, object_count, light_count, warmup_seconds, measurement_seconds`

프레임별 CSV에는 다음을 기록한다.

`run_id, frame_id, frame_ms, render_cpu_ms, fence_wait_ms, present_ms, render_gpu_ms, gbuffer_gpu_ms, lighting_gpu_ms, light_cull_gpu_ms, draw_calls, submitted_objects, visible_primitives, resource_bytes`

GPU Query 결과가 늦게 도착해도 원래 frame_id에 연결한다. Forward 등 해당 패스가 없는 경우 빈 값으로 구분한다.
측정 범위가 다른 지표는 이름과 메타데이터로 구분하고, 원본 CSV와 실행별 요약을 함께 보존한다.

## 7. 개발 순서와 완료 기준

| 단계 | 작업 | 완료 기준 | 상태 |
| --- | --- | --- | --- |
| B0 | 기준선 고정, 계측 모듈, CSV 및 고정 비교 장면 | 동일 설정 반복 실행 결과와 측정 범위를 재현 가능 | 다음 작업 |
| B1 | 프레임 리소스 및 동기화 정비 | 자원 재사용 안전성 확인, 전후 CPU/GPU/대기 시간 비교 | 대기 |
| B2 | 인스턴싱 및 Frustum 컬링 | 반복/개별 메시 장면의 올바른 출력과 객체 수별 비용 확인 | 대기 |
| B3 | Point Light 및 공정한 Forward/Deferred 비교 | 1/4/8/16 광원 비교 그래프, 품질 차이와 유불리 조건 설명 | 대기 |
| B4 | G-buffer 월드 위치 제거 | 복원 정확성, 명목상 저장량 및 실제 메모리/GPU 시간 비교 | 대기 |
| B5 | Tiled Deferred | 목록 구축 포함 전체 비용 비교, 광원 누락/초과 처리 검증 | 대기 |
| B6 | 포트폴리오 정리 | 코드 변경, 전후 캡처, CSV, 그래프, 측정 조건과 한계 정리 | 대기 |

첫 작업은 최적화가 아니라 **B0: 비교 가능한 기준선과 측정 도구**다.
프레임별 GPU 시간 수집에 필요한 Query 저장 공간과 Fence 수명은 계측 단계부터 안전하게 구성한다.
각 단계의 성능 향상은 실측 후에만 완료 결과로 기록한다. 성능이 개선되지 않으면 원인과 불리한 조건을 남긴다.
PIP 원본의 비교 계측 수정은 별도 승인 및 쓰기 권한을 확보한 뒤 진행하고 기준 버전을 보존한다.

## 8. 범위와 작업 원칙

- 정확성 오류 및 비교에 필요한 기능을 우선한다.
- GLB, 투명 재질, 애니메이션, 편집 기즈모, 추가 UI 등은 비교 실험에 필요하지 않으면 후순위다.
- 성능 계측, UI, 장면 관리 및 렌더 패스 코드를 분리하고 Renderer는 조립과 실행 순서를 담당한다.
- 기존 네이밍 규칙과 PCH 구성을 유지한다. 새로운 의존성은 필요성을 먼저 검토한다.
- 빌드는 사용자 승인 시 수행한다. 화면 캡처는 하지 않는다. 코드 점검, 빌드, GPU 실행과 실측 결과를 구별해서 보고한다.
- 프로파일링/벤치마크 자동 실행은 사용자의 별도 요청이 있을 때만 수행한다.
- 측정 전부터 향상률이나 특정 FPS를 보장하지 않는다.

## 9. 포트폴리오 결과 템플릿

> S.T.L의 [병목]을 [측정 조건]에서 확인했다. [개선 설계]를 적용해
> [지표]가 [실측 전 값]에서 [실측 후 값]으로 변화했다.
> 동일 품질 검증은 [방법]으로 수행했고, [불리한 조건/추가 비용]이 남는다.

대괄호는 실제 결과가 생긴 뒤 채운다. 이론적 저장량 감소와 실제 처리 시간 개선을 별도 표로 제시한다.

## 10. 검증 참고 자료

- [Microsoft PIX Timing Captures](https://learn.microsoft.com/en-us/windows/win32/direct3dtools/pix/articles/timing-captures/pix-timing-captures): CPU/GPU 타임라인과 병목 분석.
- [D3D12 Queries](https://learn.microsoft.com/en-us/windows/win32/direct3d12/queries): Query 기반 GPU 계측.
- [GetTimestampFrequency](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-gettimestampfrequency): Timestamp의 시간 단위 변환.
- [PBRT 4th Edition](https://www.pbr-book.org/4ed/contents): PBR 이론 검토. 실시간 Deferred/Tiled 구현의 성능 근거와는 구분한다.

구현 시 필요한 API와 이론은 공식 문서 및 원자료를 다시 확인한다.
