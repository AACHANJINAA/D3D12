# S.T.L 대비 렌더러 발전 및 성능 검증 계획

작성일: 2026-10-02
상태: 주 개발 목표 확정 / 성능 계측 및 비교 실행 전

비교 정책 변경(2026-10-02): **객체 컬링을 제외하고 배칭 성능을 비교한다.**
양쪽 Frustum/거리/Occlusion 컬링을 끄고 생성한 모든 객체의 primitive를 제출한다.
비교 실행의 back-face culling도 양쪽 NONE으로 고정한다. GPU의 clipping/depth test는 유지한다.
화면 밖 객체까지 Draw에 제출하지만 그 객체가 실제 픽셀을 생성한다고 해석하지 않는다.

이 문서는 저장소의 개발 계획이며 Codex 실행용 SKILL.md는 아니다.
아래 부하 수치와 시험 방법은 목표 사양이다. 현재 구현 지원 범위나 측정 완료 결과를 뜻하지 않는다.

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

위 표는 포팅 전 소스 분석 기준이다. Forward의 Frustum/거리 객체 탈락 코드는 제거했다.
Deffered에 primitive/재질별 인스턴싱을 추가했고 양쪽 `--benchmark`는 같은 instancing/VSync 설정을 읽는다.
BRDF, IBL 보정, 샘플러와 프레임 대기 방식에는 차이가 남아 있어 방식 자체의 공정한 비교는 준비 중이다.
IBL의 시각적 사용 여부와 별개로 출력 품질 동등성은 아직 인증하지 않았다.

소스 근거:

- PIP: `Renderer.cpp`의 렌더 목록 구성 및 glTF 인스턴스 그룹, `GameFramework.cpp`의 `MoveToNextFrame`.
- PIP: `Gltf_Shader.hlsl`의 `PS_GLTF`, `Light.hlsl`의 `MAX_LIGHTS` 및 조명 루프.
- 현재: `Renderer.cpp`의 `move_to_next_frame`, `Pass/GBufferRenderPass.*`, `SceneRenderData.cpp`.
- PIP에는 Occlusion 관련 코드도 있지만 존재만으로 활성화된 성능 기능이라고 간주하지 않는다. 실제 비교 경로에서의 호출 여부를 확인한다.

## 3. 비교를 세 종류로 분리

### A. S.T.L 포팅 기준선 대비 비교

동일 자산, 장면 배치, 카메라, 광원, 재질 및 품질 조건에서 두 렌더러를 비교한다.
객체 컬링은 양쪽 모두 OFF로 고정한다. PIP의 원래 컬링을 제외한 변경 기준선임을 표시한다.
인스턴싱 ON/OFF는 양쪽에 같은 조건으로 적용하고 특정 경로에만 유리한 조건을 만들지 않는다.
공통 기능만 켠 비교와 각 렌더러의 최적화 경로 비교를 구분해 기록한다.

결과 명칭은 **S.T.L 정적 렌더링 경로 포팅본 대비**로 사용한다.
게임 전체의 성능 또는 PIP 원본 실행 파일과 동일한 성능이라고 표현하지 않는다.
원본 대비 포팅 차이, 보존 셰이더 해시와 기준 커밋을 함께 보관한다.

### B. 새 렌더러 내부 개선 전후 비교

동일 실행 파일의 옵션 또는 고정된 전후 버전으로 한 번에 한 가지 변경만 비교한다.
프레임 동기화, 인스턴싱, G-buffer 압축 각각의 기여를 분리한다.
Tiled 광원 목록 최적화는 객체 컬링과 다르지만 현재 배칭 시험에 섞지 않고 이후 별도 시험으로 다룬다.
이 결과를 S.T.L 대비 수치로 바꾸어 표현하지 않는다.

### C. 동일 품질 Forward/Deferred 비교

렌더링 방식 자체를 비교하려면 공통 BRDF/IBL/감쇠/톤매핑, 동일한 sampler/mip,
back-face culling NONE과 객체 컬링 OFF, 프레임 리소스 수를 적용하는 별도 비교 프로필을 만든다.
PIP 원본 보존 프로필을 덮어쓰지 않고 `pip_reference`와 `matched_quality`로 구분한다.
`matched_quality`는 PIP 수정 프로필이며 S.T.L 원본 결과라는 이름으로 발표하지 않는다.

인스턴싱은 양쪽 동일 설정으로 맞추고 객체 컬링은 사용하지 않는다.
각 구현의 기본 최적화를 유지한 종합 비교도 허용하지만 그 결과를 Forward/Deferred 방식만의 효과로 해석하지 않는다.

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
- 반복 메시 인스턴싱 ON/OFF를 비교한다. Frustum/거리/Occlusion 컬링은 비교 항목에서 제외한다.
- 주요 지표: 렌더링 CPU 작업 ms, Fence 대기 ms, Present 대기 ms, Draw Call 수, 제출 객체 수, 프레임 p95.
- 일반 UI는 128개 제한을 유지한다. benchmark 전용 일괄 생성은 최대 50,000개 객체와
  65,536개 primitive-instance를 허용한다. 다중 primitive 모델은 객체 한도보다 먼저 제출 한도에 도달할 수 있다.
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
| 실행 길이 | 기본 10초 워밍업 후 30초 측정, 조건별 5회. 측정 중 로딩/컴파일 제외 |
| 순서 | 비교 순서를 교차하고 발열 및 백그라운드 작업에 의한 이상 구간 기록 |
| 요약 | 실행별 평균, 중앙값, p95/p99 프레임 시간과 5회 실행 간 최소/최대 및 변동 보고 |

- CPU 시간과 GPU 시간을 분리한다. 중첩 실행되므로 단순 합을 전체 프레임 시간으로 취급하지 않는다.
- CPU는 렌더 준비/명령 기록, Fence 대기, Present 호출 구간을 구분한다.
- GPU는 Timestamp Query와 큐의 Timestamp Frequency로 측정한다. 결과 읽기 때문에 매 프레임 GPU를 기다리지 않는다.
- 기본 산출물은 CSV/JSON과 수치 그래프다. PIX 타이밍 추적은 별도 승인 후 보조 분석에만 사용하며
  도구가 연결된 실행과 순수 성능 측정 실행을 분리한다. 스크린샷이나 영상은 생성하지 않는다.
- 메모리는 렌더러가 소유한 리소스 할당량과 프로세스 비디오 메모리 사용량을 구분한다.
- 화질 검증은 고정 카메라에서 사용자 육안 확인과 재질/광원 입력 검사를 우선한다.
  동일 셰이딩을 사용하는 내부 최적화는 필요 시 GPU readback으로 선형 HDR 오차 수치만 산출한다.
  readback 검사는 별도 정확성 실행으로 분리하고 이미지 파일이나 화면 캡처는 저장하지 않는다.
  수치 검증 기능이 없거나 원래 셰이딩이 다른 프로필은 품질 동등성 미검증으로 표시한다.
- 샘플 부족, 측정 실패, 미지원 항목을 0으로 저장하지 않는다. 미측정 상태를 명시한다.
- 개선율은 `(기준 시간 - 개선 시간) / 기준 시간 * 100`으로 계산하며 기준 조건을 함께 표시한다.

## 6. 계측 결과 형식

실행 단위 메타데이터에는 다음을 기록한다.

`run_id, renderer, commit, dirty_diff_id, scene_id, asset_hashes, gpu, driver, build, resolution, vsync, quality_settings, object_count, light_count, warmup_seconds, measurement_seconds`

추가 메타데이터: `test_id, profile, repeat_index, scene_hash, seed, adapter_luid,
frames_in_flight, instancing, frustum_culling, distance_culling, draw_order, sampler,
shadow_mode, texture_mip_policy, light_distribution, light_radius, status, failure_reason`.

프레임별 CSV에는 다음을 기록한다.

`run_id, frame_id, frame_ms, render_cpu_ms, fence_wait_ms, present_ms, render_gpu_ms, gbuffer_gpu_ms, lighting_gpu_ms, light_cull_gpu_ms, draw_calls, submitted_objects, visible_primitives, resource_bytes`

추가 지표: `scene_update_cpu_ms, batching_cpu_ms, command_record_cpu_ms,
forward_mesh_gpu_ms, skybox_gpu_ms, instances_drawn, visible_objects, submitted_triangles,
unique_meshes, unique_materials, pso_switches, descriptor_heap_switches, gbuffer_allocated_bytes,
video_memory_current_usage_bytes, tile_lights_mean, tile_lights_max, light_list_overflows`.
객체 1개가 여러 primitive로 나뉘므로 객체 수, instance 수, primitive 수와 Draw Call을 혼용하지 않는다.
메모리 사용량 조회는 샘플링 주기를 기록하고 핫 루프 밖에서 수행한다. 인스턴스가 공유하는 버퍼는 한 번만 센다.

GPU Query 결과가 늦게 도착해도 원래 frame_id에 연결한다. Forward 등 해당 패스가 없는 경우 빈 값으로 구분한다.
측정 범위가 다른 지표는 이름과 메타데이터로 구분하고, 원본 CSV와 실행별 요약을 함께 보존한다.

## 7. 개발 순서와 완료 기준

| 단계 | 작업 | 완료 기준 | 상태 |
| --- | --- | --- | --- |
| B0 | 기준선 고정, 계측 모듈, CSV 및 고정 비교 장면 | 동일 설정 반복 실행 결과와 측정 범위를 재현 가능 | 다음 작업 |
| B1 | 프레임 리소스 및 동기화 정비 | 자원 재사용 안전성 확인, 전후 CPU/GPU/대기 시간 비교 | 대기 |
| B2 | 컬링 없는 대량 생성 및 인스턴싱 | T1/T3/T5에서 객체·primitive 수 유지 및 Draw 감소 검증 | 구현 진행 / GPU 출력·성능 미검증 |
| B3 | Point Light 및 공정한 Forward/Deferred 비교 | 1/4/8/16 광원 비교 그래프, 품질 차이와 유불리 조건 설명 | 대기 |
| B4 | G-buffer 월드 위치 제거 | 복원 정확성, 명목상 저장량 및 실제 메모리/GPU 시간 비교 | 대기 |
| B5 | Tiled Deferred | 목록 구축 포함 전체 비용 비교, 광원 누락/초과 처리 검증 | 대기 |
| B6 | 포트폴리오 정리 | 코드 변경, 정확성 확인 기록, CSV, 그래프, 측정 조건과 한계 정리. 화면 캡처 제외 | 대기 |

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

## 11. 확정할 비교 장면과 부하 단계

모든 장면은 정적 불투명 glTF를 사용한다. 기본 해상도는 1920x1080,
카메라/객체 애니메이션은 정지, 그림자/AA/UI/선택 외곽선은 끈다.
Skybox와 IBL은 양쪽 동일 설정으로 켜고 별도 GPU 구간으로 구분한다.
원본 glTF의 단위와 크기를 확인해 중앙 배치 및 균일 스케일을 장면 생성 단계에서 고정한다.
Forward의 500단위 거리 탈락은 제거했다. 모든 객체를 제출하고 near/far clipping 설정은 양쪽 통일한다.

### T0. 단일 메시 및 기준 비용

- 자산: BoxTextured 1개, DamagedHelmet 1개를 각각 별도 시험. 빈 장면도 보조 기준으로 측정.
- 광원: Directional Light 1개. 위치/방향/선형 색상/강도와 환경 설정을 양쪽에 동일 적용.
- 목적: 기본 패스 비용, 로딩 성공, 재질/IBL, 카메라와 화면 크기의 비교 기준 확보.
- 지표: CPU/GPU 전체 ms, 메시/조명/배경 패스 ms, 타깃 및 자산 메모리.
- 완료: 같은 입력으로 재실행 가능하며 품질 확인 상태를 기록. 빈 장면 비용을 무조건 빼서 결과를 만들지 않는다.

### T1. 동일 메시 대량 배치

- 자산: BoxTextured처럼 저폴리·단일 primitive·단일 재질인 메시를 먼저 사용.
- 초기 확인: 1/16/64/128개. 확장 후 본 시험: 100/1,000/5,000/10,000/25,000/50,000개.
- 객체는 같은 MODEL_RESOURCE와 텍스처를 공유하고 transform만 다르게 한다. 매 객체마다 파일을 다시 읽지 않는다.
- 배치: 모든 객체가 보이는 평면 격자. 각 부하의 카메라/간격/균일 스케일을 생성해 manifest에 저장.
- 부하 단계마다 카메라가 바뀔 수 있으나 같은 단계의 두 렌더러는 정확히 같은 카메라를 사용.
- 너무 작아 픽셀이 거의 없는 객체가 많아지면 CPU 제출 시험으로 분류하고 평균 화면 크기와 가시 수를 기록.
- 비교: Deffered 인스턴싱 OFF/ON, 그다음 양쪽 인스턴싱 ON의 Forward/Deffered 비교.
- 지표: 준비/배칭/명령 기록 CPU ms, GPU ms, Draw Call, instance 수, 버퍼 용량, 프레임 p95/p99.
- 가설: 반복 메시의 개별 Draw 비용을 인스턴싱으로 줄일 수 있다. 예상 향상률을 미리 정하지 않는다.

### T2. 정점 및 삼각형 부하

- 자산: 저폴리 BoxTextured, 중간 복잡도 모델, DamagedHelmet 등 상대적으로 복잡한 모델.
- 분류는 이름이 아니라 Import 후 실제 정점/삼각형/primitive/재질 수로 결정한다.
- 목표 제출 삼각형 단계: 10만/100만/500만/1,000만. 모델별 객체 수는 목표에 맞추고 실제 값을 기록.
- 인스턴싱 ON, 광원 1개, 가능한 비슷한 화면 점유율의 고정 배치 사용.
- 메시 복잡도뿐 아니라 텍스처와 재질 비용도 달라지므로 일반 재질 결과를 순수 정점 성능으로 표현하지 않는다.
- 순수 기하 비용을 확인하려면 동일한 단색 재질을 사용하는 별도 프로필을 양쪽에 구현한다.
- 지표: 실제 제출 삼각형 수, 메시 GPU ms, 전체 GPU ms, 정적 geometry 할당량.
- 용량 초과나 과도한 실행 시간은 한계 결과로 기록하고 다음 부하를 자동 강행하지 않는다.

### T3. 여러 메시 및 재질

- 자산 후보: BoxTextured, Duck, Avocado, BoomBox, DamagedHelmet. 지원 여부를 사전 확인.
- 각 자산을 동일한 수로 배치해 총 100/1,000/5,000/10,000개를 시험한다.
- 같은 5종 반복은 '고유 메시 5종' 시험이지 '서로 다른 메시 1만 개' 시험이 아니다.
- 재질 다양성 시험은 동일 메시에 material override 1/8/32종을 적용하고 총 객체 수는 고정.
- 공유 자원 수와 실제 인스턴스 그룹 수, 재질 변경 및 PSO/descriptor heap 변경 횟수를 함께 기록.
- 목적: 공유 리소스 메모리 효과, 재질 다양성으로 인한 배칭 분할과 CPU 비용 파악.
- 재질 정렬 전후 비교는 다른 기능 설정을 고정한 내부 실험으로 분리한다.

### T4. 다중 Point Light

- 고정 장면: 충분한 화면 면적을 차지하는 메시 격자. 객체/삼각형 수와 카메라는 모든 광원 단계에서 고정.
- PIP 범위 시험: Point Light 1/4/8/16개. 추가 Directional Light는 끄며 총 광원 수가 16개를 넘지 않게 한다.
- 확장 시험: 32/64/128/256개. 양쪽 광원 저장 구조와 순회 코드를 확장한 `matched_quality` 프로필만 사용.
- Forward가 해당 개수를 지원하지 않으면 미지원으로 기록하고 Deffered 내부 확장성 실험만 수행.
- 분포 A: 반경이 작은 광원을 장면 전체에 분산해 중첩을 적게 만든다.
- 분포 B: 넓은 반경의 광원을 중앙에 겹쳐 한 픽셀이 여러 광원의 영향을 받게 한다.
- 같은 seed의 최대 광원 목록에서 앞 N개를 사용하며 위치/반경/감쇠/강도는 renderer 간 동일.
- 광원 수에 따라 총 조명이 강해지는 것은 허용하지만 어느 한쪽에서만 자동 정규화하지 않는다.
- 그림자 없는 시험부터 수행. Shadow Map 추가 시 별도 T4-shadow로 이름을 나누고 해상도/갱신 규칙까지 통일.
- 비교: Forward 전체 메시+조명 vs G-buffer+Deferred 조명; 단순 Deferred vs Tiled Deferred.
- 지표: 전체 GPU ms, 각 패스 ms, 타일 구축 비용, 타일당 평균/최대 광원 수, overflow 횟수.
- 평균뿐 아니라 과밀 타일과 overflow를 검사한다. 조명을 누락시켜 얻은 성능은 유효하지 않다.

### T5. 무컬링 제출 무결성 및 배치 분할

- 기존 Frustum 컬링 성능 시험은 취소한다. 이 항목은 배칭 정확성 확인용이다.
- 동일 모델 10,000개 중 일부를 카메라 뒤 또는 500단위 밖으로 이동해도 제출 primitive-instance 수가 유지되어야 한다.
- instancing ON/OFF에서 생성 객체 수와 제출 삼각형 합이 같아야 한다. 실제 화면 픽셀 수와는 구분한다.
- 같은 메시·primitive·재질은 묶이고 서로 다른 재질 override는 별도 그룹으로 나뉘어야 한다.
- 선택 외곽선과 비균일 스케일의 normal/tangent 처리는 Deffered 인스턴스별 데이터로 유지한다.
- 제출 Draw 감소는 이론적 배칭 결과이며 실제 GPU 시간 향상으로 간주하지 않는다.

### T6. Overdraw와 제출 순서

- 동일 객체/삼각형 수를 유지하고 깊이 방향 배치만 바꿔 낮은 겹침/높은 겹침 장면을 만든다.
- 고정 카메라와 동일한 back-face culling, depth 비교 및 depth write 설정 사용.
- 양쪽 동일 Draw 순서로 먼저 비교. Front-to-back 정렬 ON/OFF는 별도 내부 실험.
- 지표: 메시/G-buffer/전체 GPU ms와 실제 Draw 수. 픽셀 처리 통계가 없으면 겹침 단계만 기록.
- 스크린샷만 보고 정확한 overdraw 배수를 주장하지 않는다. 인스턴싱 묶음에 따른 순서 차이도 기록.

### T7. 해상도 및 G-buffer

- 내부 렌더 해상도: 1280x720/1920x1080/2560x1440/3840x2160. 창 크기가 아닌 실제 RT 크기 기준.
- 해상도 외 카메라/FOV/객체/광원/품질은 고정. 1광원 및 16광원 장면을 각각 시험.
- 현재 창/Swap Chain에 해상도가 묶여 있으므로 모니터보다 큰 해상도는 offscreen RT 지원 후 진행.
- 지표: 전체 GPU ms, G-buffer/조명 ms, 타깃별 할당량 및 프로세스 비디오 메모리 사용량.
- 같은 Deffered 장면에서 월드 위치 저장 ON/OFF를 비교해 40 -> 24 B/pixel 목표를 검증.
- 명목상 픽셀 저장량, 실제 allocation 크기, 전체 GPU 메모리와 시간을 각각 별도 열로 유지.

## 12. 자산과 자동 장면 생성 사양

### 자산 manifest

- glTF뿐 아니라 참조 BIN/이미지 및 IBL 파일의 해시를 기록한다.
- 모델별 정점/삼각형/primitive/재질 수, bounding box, 텍스처 크기/mip/색 공간을 기록한다.
- 텍스처 대체, 빠진 파일, 지원하지 않는 확장은 시험 실패 또는 제외 사유로 남긴다.
- 정적 geometry와 texture는 로딩 시 한 번만 업로드하고 모든 인스턴스가 공유한다.
- 테스트 중 resource cache miss나 업로드가 발생하면 정상 steady-state 표본과 구분한다.

### Benchmark JSON 확장 목표

현재 `static-grid.json`의 명시적 객체 목록을 유지하면서 다음 생성 옵션을 추가한다.
아래는 신규 목표 스키마이며 현재 파서에 이미 지원된 필드가 아니다.

| 영역 | 계획 필드 / 역할 |
| --- | --- |
| 식별 | schema_version, test_id, scene_id, seed, profile |
| 실행 | warmup_seconds=10, measurement_seconds=30, repeats=5, output_directory |
| 화면 | width, height, vsync=0, camera position/target/FOV/near/far |
| 객체 생성 | model 목록, count, distribution, grid dimensions, spacing, uniform_scale, material_variants |
| 광원 생성 | type, count, positions/seed, radius, linear color/intensity, attenuation, overlap mode |
| 기능 토글 | instancing, draw_order. frustum/distance/occlusion_culling은 false만 허용. tiled_lighting/compact_gbuffer는 이후 별도 시험 |
| 품질 | IBL asset IDs, exposure, tone mapper, sampler/mip, back-face culling, shadows, AA |

생성은 측정 전에 완료한다. 최종 transform/광원 목록을 resolved-scene JSON으로 저장하고
두 renderer는 그 동일 파일을 읽도록 한다. 같은 seed라도 구현별 난수 차이가 생기는 것을 방지한다.
지원하지 않는 설정은 조용히 무시하거나 개수를 줄이지 말고 명시적으로 실패시킨다.
대량 생성은 benchmark 전용 경로에서 수행하고 UI의 객체 선택 목록을 수만 번 갱신하지 않는다.

## 13. 실행 및 결과 판정 절차

1. 소스 커밋과 dirty diff를 고정하고 두 Release 빌드 및 shader 컴파일을 확인한다.
2. 별도 Debug 정확성 실행에서 API 오류, 누락 객체/광원, 버퍼 범위 초과 및 품질 설정을 확인한다.
3. 동일 resolved-scene과 profile을 사용하며 두 실행 파일을 동시에 실행하지 않는다.
4. 자산 로딩/PSO 생성/업로드 완료 후 10초 준비, 30초 측정, 종료 시 미회수 GPU query를 회수한다.
5. A-B, B-A 순서가 편중되지 않도록 조건마다 5회 반복한다. 실행 순서와 환경 이상을 기록한다.
6. raw frame CSV와 run metadata, 실행별 summary를 보존한다. 종료 후 회수 대기는 측정 프레임 시간에서 제외한다.
7. 평균 FPS는 프레임별 FPS의 평균이 아닌 측정 프레임 수/실제 측정 초로 산출한다. 주요 결과는 ms로 제시한다.
8. 각 실행의 평균/중앙값/p95/p99를 먼저 계산하고 실행별 요약의 중앙값과 최소/최대를 보고한다.
9. 프레임 표본을 전부 합쳐 반복 간 차이를 숨기지 않는다. p95/p99는 표본 수와 함께 기록한다.
10. 오류/OOM/device removed/timeout은 실패 원인과 마지막 유효 부하를 기록하며 0ms로 처리하지 않는다.

GPU 총 시간의 범위는 첫 GPU 작업부터 마지막 렌더 작업까지로 정의한다. Present 대기 시간과는 다르다.
CPU frame_interval_ms는 준비 시작 간 간격(UI 준비 포함), frame_ms는 UI 이후 렌더 준비부터 슬롯 대기까지다.
CPU 작업 시간은 prepare/record/submit/present/sync로 구분한다.
두 renderer의 패스 합산 범위가 같은지 확인한 뒤 개선율을 계산한다.
시스템 이상을 이유로 제외한 실행도 원본과 제외 사유를 남긴다. 유리한 실행만 선택하지 않는다.
성능 차이가 반복 변동과 비슷하면 '뚜렷한 개선 미확인'으로 결론 낸다.
프로파일·측정 범위·품질이 다른 결과는 같은 향상률로 묶지 않는다.

## 14. 우선 산출물과 포트폴리오 구성

최초 필수 시험은 T0, T1, T5, T4의 1~16광원이다. T2/T3/T6/T7과 32~256광원은 기반 구축 후 확장한다.
전체 조합을 한 번에 실행하지 않고 기본 설정에서 변수 하나씩 바꾸는 시험부터 구성한다.

| 결과 그래프 | X축 | Y축 / 보조 값 | 설명할 개선 |
| --- | --- | --- | --- |
| 객체 확장성 | 100~50,000 객체 | CPU 준비/기록 ms, Draw Call | 인스턴싱과 제출 병목 |
| 무컬링 배칭 | 생성 객체 수 / 재질 종류 | 제출 primitive-instance, Draw Call, 배칭 CPU ms | 객체를 누락하지 않고 제출 횟수 감소 |
| 광원 확장성 | 광원 수, 분포별 분리 | 전체 GPU ms, 패스별 ms | Forward/Deferred 교차점과 Tiled 구축 비용 |
| 메모리와 해상도 | 내부 해상도 | G-buffer allocation MiB, GPU ms | 위치 복원 및 타깃 축소 |
| 재질 다양성 | 고유 재질 수 | 배칭 CPU ms, Draw Call, 공유 자원 수 | 배칭 분할과 자원 재사용 |

가장 좋은 결과만이 아니라 단일 광원, 전부 가시, 작은 장면 등 최적화에 불리한 조건도 포함한다.
최종 자료는 재현 명령, JSON, 코드 버전, CSV, 수치 그래프와 한계 설명으로 구성한다.
화면 사진/영상 생성은 이 계획에 포함하지 않는다. 벤치마크 실행은 별도 요청 후 진행한다.

## 15. 이번 구현 및 실행 입력

- Deffered: G-buffer 패스의 `SV_InstanceID` + t8 StructuredBuffer로 월드/법선 행렬, tangent 부호, 선택 mask 전달.
- 모델/primitive/유효 재질 상수가 같은 객체를 그룹화하며, 재질 override가 다르면 별도 Draw로 분리.
- Forward: 객체 Frustum/거리 탈락 제거. 원본 PIP HLSL은 유지. benchmark의 rasterizer는 양쪽 NONE.
- 공통 JSON `instancing: false`는 primitive-instance마다 개별 Draw, `true`는 그룹별 인스턴스 Draw.
- `grid`는 model/count/columns/origin/spacing/rotation/scale로 결정론적 평면 격자를 생성하며 명시적 objects 목록과 병용 가능.
- 고유 모델마다 한 번 Import한 뒤 공유 리소스로 일괄 Scene을 구성한다. 객체마다 GPU 대기나 파일 로딩을 반복하지 않는다.
- `Benchmark/batching-10000.json`, `Benchmark/batching-50000.json`은 반복 배치 입력이다. 모델은 각 JSON의 grid.model을 따른다.
- 첫 제출 준비 시 `Benchmark/results/<renderer>-<scene>-batched.json` 또는 `-unbatched.json`에
  객체 수, primitive-instance 수, 메시 Draw Call과 제출 삼각형 합을 기록한다. 같은 이름은 재실행 시 갱신된다.
- 해당 파일은 **CPU 제출 검증 기록**이지 GPU 완료/화질 검증이나 성능 측정 결과가 아니다.
- 2026-10-05: 아래 16절의 2프레임 슬롯, GPU timestamp, CPU 구간 CSV, 반복 실행,
  Point Light 및 matched_quality 비교 경로를 구현했다.

사용자가 저장소 루트에서 각각 실행할 명령:

```powershell
& .\Deffered\bin\x64\Release\Deffered.exe --benchmark .\Benchmark\batching-10000.json
& .\Forward\bin\x64\Release\Forward.exe --benchmark .\Benchmark\batching-10000.json
```

동일 JSON에서 instancing만 false로 바꿔 개별 Draw와 비교한다. 5만 개 입력도 같은 방식으로 실행한다.
GPU 렌더링 결과와 시간은 직접 실행 후 확인해야 하며, 빌드 및 CPU 테스트 통과와 구분한다.

검증 상태(2026-10-02): 두 앱 Debug/Release x64 빌드 성공. `Tests/BatchingTests.vcxproj`를
`RendererUnderTest=Deffered`와 `Forward`로 각각 빌드해 실행한다. WARP 장치에서 자원/PSO만 만들고
명령 큐 제출, 창 생성, 화면 캡처 및 성능 계측은 하지 않는 회귀 테스트다.
5만 개의 1/50,000 Draw 구성, 공유 모델 1회 로딩, 원거리/카메라 뒤 객체의 제출 수 유지,
재질 변경에 따른 2개 그룹 분리/재병합, 빈 장면을 검사한다.

인스턴싱 구현 참고: [DrawIndexedInstanced](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-drawindexedinstanced),
[Root SRV와 StructuredBuffer](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-descriptors-directly-in-the-root-signature).

## 16. 비교 UI와 실측 경로 (2026-10-05)

### 수동 비교

```powershell
& .\Forward\bin\x64\Release\Forward.exe --benchmark .\Benchmark\comparison.json
& .\Deffered\bin\x64\Release\Deffered.exe --benchmark .\Benchmark\comparison.json
```

- 제목 표시줄에 Forward / Deffered와 Comparison / Viewer를 표시한다.
- JSON 실행 시 기존 편집 패널 대신 Comparison 패널을 띄운다.
- 객체 수, 메시 Draw Call, CPU 준비/기록 ms, GPU 총/패스별 ms를 표시한다.
- Point lights 슬라이더는 0~256개, Instancing 체크박스는 개별 Draw/배칭 전환이다.
- 객체 수는 JSON의 grid.count에서 정한다. 런타임 객체 수 변경 컨트롤은 아니다.
- 카메라는 계속 고정한다. ImGui 입력만 허용하며 일반 카메라 입력은 전달하지 않는다.
- measurement_seconds가 0이거나 없으면 수동 실행, 양수이면 측정 후 자동 종료한다.
- 자동 측정 중 슬라이더/배칭은 잠기며 최소화 또는 해상도 변경은 실패로 처리한다.

### 공통 조건

- matched_quality는 동일한 공통 HLSL의 GGX BRDF, 감쇠, IBL, 톤 매핑으로 비교한다.
- Forward의 원본 PIP 셰이더 파일은 변경하지 않고 별도 MatchedForward 진입점을 사용한다.
- native는 기존 표시 경로 확인용이다. Point Light 및 자동 시간 비교는 matched_quality만 허용한다.
- 양쪽 모두 객체/거리/Frustum/가림/Back-face 컬링 없이 제출한다.
- 모든 표면에서 동일한 전체 Point Light 루프를 평가한다. Tiled/Clustered/광원 볼륨 최적화는 없다.
- 광원 위치는 공통 코드와 seed로 결정하며 개수를 바꿔도 앞 N개의 위치가 유지된다.
- 두 렌더러 모두 2프레임 슬롯, 정적 geometry DEFAULT heap을 사용한다.
- 배경/ImGui 비용은 GPU 총 시간에 포함되고 별도 패스 시간도 기록한다.
- Forward는 메시+라이팅을 함께 실행하므로 별도 lighting_gpu_ms는 빈 칸이다. 0ms로 해석하지 않는다.
- matched_quality는 픽셀 일치 인증이 아니다. G-buffer 양자화, 패스 구조, 업로드량 및 바인딩 비용은 다르다.
- 자동 생성 기본 자산은 opaque BoxTextured다. 다른 자산은 지원 재질/변환과 렌더 결과를 먼저 확인한다.

### 저장 항목과 측정 범위

- QueryPerformanceCounter로 prepare / record / ExecuteCommandLists / Present / 슬롯 대기를 측정한다.
- GPU EndQuery 쌍과 큐 TimestampFrequency를 이용한다. ResolveQueryData 이후 해당 슬롯의 fence가
  완료된 때만 READBACK 데이터를 읽는다. 측정을 위해 매 프레임 추가 GPU 전체 대기를 넣지 않는다.
- 종료할 때 남은 쿼리를 회수하고 워밍업 표본을 제외한다. 파일은 프레임마다 쓰지 않고 종료 시 저장한다.
- CSV: frame ID, CPU 구간, GPU 총/메시/라이팅/스카이/UI, Draw Call, primitive-instance, 삼각형, 광원, 큐 제출 수.
- JSON: 원본 입력, GPU/드라이버, 빌드 종류, 측정 범위, 디버그 오류, 표본 수, 평균/중앙값/p95/p99.
- GPU 측정 범위는 첫 렌더 명령부터 마지막 전환까지다. Present 및 query resolve는 제외한다.
- CPU frame_ms는 UI 이후 준비부터 슬롯 대기 완료까지, frame_interval_ms는 연속 준비 시작 간 간격이다.
- 파일명 run_id-renderer.csv/json이 이미 있으면 덮어쓰지 않고 초기화에 실패한다.

### 반복 실행

```powershell
# 짧은 기능 검증. 성능 근거로 사용하지 않는다.
& .\Benchmark\RunComparison.ps1 -Smoke

# 정식 비교 예시: 같은 객체 수에서 광원만 변경, 반복 5회
& .\Benchmark\RunComparison.ps1 -ObjectCounts 1000 -LightCounts 0,1,4,8,16,32,64,128,256 -Modes Batched -Repeats 5

# 무컬링 배칭 확장성과 광원 중첩 조건
& .\Benchmark\RunComparison.ps1 -ObjectCounts 1000,10000,50000 -LightCounts 8 -Modes Batched,Unbatched -Distribution Overlap -Repeats 5
```

- 워밍업 기본 10초, 측정 기본 30초, 1280x720, VSync 0, Release다.
- 해상도는 -Width/-Height, 시간은 -Warmup/-Seconds로 바꾼다.
- 한 번에 한 실행파일만 실행한다. 반복마다 A-B/B-A 순서를 바꾸고 시간 초과 프로세스는 정리한다.
- results 아래 고유 폴더에 생성 JSON, raw CSV/metadata, manifest, summary.csv,
  comparison.csv, aggregate.csv, report.html을 저장한다.
- manifest는 커밋/dirty 상태, EXE/런타임 HLSL/IBL/Box 자산 해시와 실패 실행을 기록한다.
- 실패 실행은 향상률에서 제외하지만 manifest에 보존한다.
- report.html은 광원 수별 GPU 그래프, 반복 편차, CPU/GPU 비교 표다. 화면 캡처를 생성하지 않는다.
- 자동 스크립트는 Hidden 시작 옵션을 사용한다. GPU 명령 시간과 CPU 루프를 측정하며 모니터에
  실제 표시된 FPS를 측정하는 시험은 아니다. 수동 JSON 실행은 일반 비교 창으로 열린다.
- aggregate는 실행별 중앙값의 중앙값/최소/최대다. 프레임을 전부 합쳐 반복 편차를 숨기지 않는다.
- CPU 합산 비교 열은 prepare 중앙값 + record 중앙값이며 전체 프레임 시간과 다르다.
- 짧은 자동 실행은 기능 검증일 뿐이다. 장시간 반복 결과와 화면 품질 확인 없이 성능 향상을 주장하지 않는다.

코드 분리: BenchmarkScene(설정/생성), BenchmarkLights(광원 버퍼), FrameProfiler(측정/저장),
ComparisonUi(패널), Shader/MatchedLighting(공통 식), RunComparison/SummarizeComparison(실행/집계).
Renderer는 프레임 슬롯과 패스 측정 지점을 연결한다.

공식 API 참고:
[GPU Timing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing),
[ResolveQueryData](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resolvequerydata),
[Fence-based resource management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management).

검증: 두 프로젝트 Debug/Release 빌드, 양쪽 WARP 회귀 테스트 통과.
Debug 및 Release 각각 객체 4개, 광원 0/4개, 배칭 on/off의 8회 스모크 실행과
CSV/JSON/HTML 생성 확인. Debug 8건의 D3D12 오류는 0건.
장시간 반복 벤치마크와 화면 품질 동등성 검증은 아직 수행하지 않았다.
