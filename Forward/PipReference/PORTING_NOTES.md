# S.T.L Forward 정적 렌더러 포팅

## 기준과 검증 상태

- 원본: `C:/Users/ckswl/Desktop/GITHUB/PIP/Client/Client`
- 기준 커밋: `0c8a7856152d3a9449ecf03e7e28b8acf4578a50`
- 스냅샷: 2026-10-02. `source-manifest.json`에 복사본 SHA-256 기록.
- 이 디렉터리의 C++ 원본은 비교/추적용이며 빌드 대상이 아니다.
- `../Renderer/Shader/Pip`의 네 HLSL 파일은 원본 바이트를 유지하며 실제 실행 경로에서 컴파일한다.
- 2026-10-02: 두 프로젝트 Debug/Release x64 빌드 성공. HLSL 진입점 12개 오프라인 컴파일 성공.
- PIP 원본 셰이더의 X3556(정수 나머지), X3571(pow 입력), X4000(그림자 함수 초기화) 경고는 보존했다.
- 원본 스냅샷 SHA-256 12개, 프로젝트 소스 경로, 출력 폴더의 셰이더/DDS 복사를 확인했다.
- 앱 실행, 화면 동등성, GPU 검증과 성능 측정은 미실행이다. 화면 캡처는 하지 않았다.
- 이 프로젝트는 S.T.L 전체 게임이나 전체 Renderer의 무수정 추출본이 아니라 **정적 glTF 렌더링 경로의 포팅본**이다.

## 유지한 동작

- 원본 `VS_GLTF`, `PS_GLTF`, `Light.hlsl`, `IBL.hlsl`, `Shadow_Sample.hlsl`.
- PIP의 13개 Root Parameter와 b0/b1/b2/b3/b5, t0~t4/t8~t12/t16, s0/s1 바인딩.
- 원본의 Schlick/Smith/GGX 식, roughness 하한, IBL 배율, Reinhard 및 gamma 처리.
- Anisotropic 16, Wrap, Back-face culling, opaque depth-tested Forward PSO.
- 메시 그룹의 인스턴스 월드 행렬을 전치해 t12 StructuredBuffer로 전달.
- 정적 정점/인덱스는 DEFAULT 힙으로 업로드하고 업로드 완료 뒤 임시 버퍼 해제.
- 2개 프레임 allocator와 Fence 값, 재사용 슬롯만 대기하는 제출 흐름.
- 500단위 거리 제한과 Frustum 검사. 실제 Bounds 구성과 CPU 구현은 아래 차이 참고.
- 게임 초기 조명의 방향/색/강도 및 global ambient 0.2를 기본값으로 사용.

## 의도적으로 달라진 부분

| 구분 | 포팅 동작 / 비교상 주의 |
| --- | --- |
| 실행부 | 현재 뷰어의 창, 입력, 파일 선택, 장면 관리 코드를 복제해 독립 실행. 게임 객체/물리/네트워크/사운드 미포함 |
| 모델 로딩 | 현재 검증된 glTF importer 및 WIC texture loader 사용. PIP ReadGLTFMesh 전체 로더는 원본 참조로만 보관 |
| 정점 패킹 | TANGENT와 UV 순서가 달라 InputLayout의 오프셋만 조정. shader semantics는 원본 유지 |
| 인스턴스 그룹 | 모델/primitive/재질 override가 같은 객체를 묶음. PIP의 메시 단위 그룹보다 세분화되므로 CPU 비용 동일성은 미보장 |
| 컬링 | primitive의 변환된 8개 AABB 꼭짓점을 clip space에서 검사. PIP GameObject/RenderComponent의 OBB 경로와 CPU 비용/세분도 차이 있음 |
| 프레임 데이터 | 포팅 전용 프레임별 CB/SRV heap 사용. PIP LinearAllocator/DescriptorManager 전체를 재사용한 것이 아님 |
| 그림자 | shadow max distance를 음수 최댓값으로 설정해 원본 sampler 함수가 1을 반환하도록 구성. shadow SRV는 유효한 null descriptor |
| IBL | 두 프로젝트가 같은 cloudy DDS/BRDF LUT와 SH 계수를 사용. 게임의 원래 환경 자산 재현은 아님 |
| 재질 범위 | 불투명 정적 glTF만 지원. Alpha MASK/BLEND, 스킨/애니메이션, 확장 재질은 importer가 거절 |
| Transform | 원본 VS의 법선 처리 그대로 유지. 비교 장면의 객체 스케일은 양수 균일 스케일로 제한. 노드 비균일/음수 스케일은 별도 검증 대상 |
| UI | Lit/Wireframe, 조명, import 등 제공. Deferred 전용 텍스처 표시 모드/외곽선/노출/환경 배율은 Forward에 연결하지 않음 |
| UI 동기화 | 일반 UI 모드는 공유 preview descriptor를 안전하게 갱신하기 위해 전체 대기. `--benchmark` 모드는 UI와 전체 프레임 대기를 생략 |
| Present | 공통 JSON의 vsync 값 사용. tearing flag는 두 프로젝트 모두 사용하지 않음 |

## 비교 실행

루트 `D3D12.slnx`에서 Forward와 Deffered를 각각 Release x64로 빌드한다.
출력은 `Forward/bin/x64/Release/Forward.exe`, `Deffered/bin/x64/Release/Deffered.exe`다.
아래 명령은 저장소 루트에서 사용자가 실행한다. 동시에 실행하지 않는다.

```powershell
& .\Forward\bin\x64\Release\Forward.exe --benchmark .\Benchmark\static-grid.json
& .\Deffered\bin\x64\Release\Deffered.exe --benchmark .\Benchmark\static-grid.json
```

두 경로 모두 동일한 모델/위치/카메라/단일 Directional Light/해상도/VSync를 읽는다.
벤치마크 모드에서는 UI, 선택 외곽선, 카메라 이동/공전, 조명 공전을 사용하지 않는다.
일반 실행은 빈 장면으로 시작하며 OPEN MODEL/ADD MODEL을 사용한다.

## 아직 공정한 성능 결론을 낼 수 없는 이유

- Deferred는 자체 PBR, Linear sampler, 다른 ambient/IBL 처리, 양면 렌더링을 사용한다.
- Forward는 PIP 원본 PBR, Anisotropic sampler, Back-face culling을 사용한다.
- 두 구현의 리소스 힙/컬링/프레임 동기화도 서로 다르다. 이는 비교 항목이지만 출력 품질 차이와 구별해야 한다.
- CPU/GPU 계측과 CSV 저장은 아직 연결하지 않았다.
- 공통 JSON은 입력 조건을 통일하는 첫 단계이며 **출력 품질 동등성 인증이 아니다**.

다음 단계는 B0 계측, 원본 S.T.L과 포팅본의 고정 장면 대조,
두 프로젝트의 공통 품질 프로필 정의 및 검증이다. 이후에만 성능 향상률을 기록한다.
원본 shader를 보존하는 PIP 프로필과 동일 품질 비교를 위한 프로필은 명확히 구분한다.
