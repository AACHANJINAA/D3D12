# Engine Structure

## 기본 소유 구조

렌더러 관련 객체는 `RENDERER`가 생성하고 소유합니다.
각 객체는 자신의 책임만 수행하며, 렌더러는 초기화 순서와 프레임 실행 순서를 조립합니다.

```text
Application
  └─ RENDERER
      ├─ WINDOW
      ├─ DEVICE
      ├─ COMMAND
      ├─ SWAP_CHAIN
      ├─ PIPELINE
      └─ RESOURCE
```

## 모듈 책임

| 모듈 | 책임 |
| --- | --- |
| `WINDOW` | Win32 창 생성, 메시지 처리, 종료 이벤트 |
| `DEVICE` | DXGI Factory, Adapter, D3D12 Device 생성 |
| `COMMAND` | Command Queue, Allocator, List, Fence 관리 |
| `SWAP_CHAIN` | Back Buffer, RTV, Present 관리 |
| `PIPELINE` | Shader, Root Signature, PSO 관리 |
| `RESOURCE` | Vertex Buffer, Index Buffer, Constant Buffer, Depth Buffer 관리 |
| `SHADER` | HLSL 컴파일, Shader 캐시 확장 지점 |
| `RENDERER` | 모듈 초기화와 프레임 렌더링 순서 조립 |

## Singleton 적용 기준

전체 모듈을 Singleton으로 만들지 않습니다.
Singleton은 하나만 존재해야 하고 여러 시스템에서 공통으로 사용하는 서비스에만 적용합니다.

### Singleton으로 사용할 수 있는 모듈

- `SHADER`: 여러 Pipeline에서 셰이더를 컴파일하고 캐시할 수 있습니다.
- `DEBUG_LOGGER`: 전체 엔진의 로그를 하나의 출력 경로로 모읍니다.
- `TIME`: 엔진 전체에서 동일한 프레임 시간 정보를 사용할 때 적용할 수 있습니다.

### Renderer가 소유할 모듈

- `WINDOW`
- `DEVICE`
- `COMMAND`
- `SWAP_CHAIN`
- `PIPELINE`
- `RESOURCE`

이 객체들은 같은 Renderer의 GPU 상태와 수명을 공유합니다.
Renderer가 소유하면 생성과 파괴 순서를 명확하게 유지할 수 있고,
향후 여러 Renderer나 테스트용 Device를 추가할 때도 구조를 확장할 수 있습니다.

## Singleton의 장점

- 인스턴스가 하나만 존재하도록 보장합니다.
- 여러 모듈에서 접근하기 쉽습니다.
- 공용 서비스의 초기 구현이 단순합니다.
- 전역 Shader 캐시나 로그 시스템에 적합합니다.

## Singleton의 단점

- 모듈 간 의존성이 코드에 명확하게 드러나지 않습니다.
- 전역 상태가 되어 테스트가 어려워집니다.
- 생성과 파괴 순서 문제가 생길 수 있습니다.
- 여러 Device나 Renderer를 동시에 운영하기 어렵습니다.
- 멀티스레드 렌더링과 Editor 구조로 확장할 때 제약이 생길 수 있습니다.

## 초기화 순서

```text
WINDOW
  ↓
DEVICE
  ↓
COMMAND
  ↓
SWAP_CHAIN
  ↓
PIPELINE
  ↓
RESOURCE
```

## 프레임 실행 순서

```text
COMMAND Begin
  ↓
Resource 상태 전환
  ↓
Grid / Mesh Render
  ↓
Resource 상태 전환
  ↓
COMMAND Execute
  ↓
SWAP_CHAIN Present
  ↓
COMMAND Fence 대기
```

## 확장 방향

현재 `RENDERER`는 초기화와 렌더링 조립을 담당합니다.
렌더 패스가 늘어나면 `RENDER_PASS` 인터페이스와 패스 목록을 추가합니다.

```text
RENDERER
  └─ RENDER_PASS 목록
      ├─ GRID_PASS
      ├─ OPAQUE_PASS
      ├─ TRANSPARENT_PASS
      └─ POST_PROCESS_PASS
```

Deferred 렌더링으로 확장할 때는 `G_BUFFER_PASS`와 `LIGHTING_PASS`를 추가하고,
투명 오브젝트는 기존 Forward 패스로 유지합니다.
