#include "stdafx.h"
#include "RenderComponent.h"

#include "GameFramework.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "Renderer.h"
#include "Shader.h"
#include "OtherPlayerScript.h"
#include "MainPlayerScript.h"
#include "OcclusionManager.h"

Material_Shader::Material_Shader()
{
}
Material_Shader::~Material_Shader()
{
}

void Material_Shader::set_shader(const std::shared_ptr<Shader>& shader)
{
	_shader = shader;
}

void Material_Shader::set_shader_root_signature(ComPtr<ID3D12RootSignature> root_signature)
{
	if (_shader)
	{
		_rootSignature = root_signature;
	}
}

void Material_Shader::set_root_signature(ComPtr<ID3D12GraphicsCommandList> command_list)
{
	if (_rootSignature)
	{
		command_list->SetGraphicsRootSignature(_rootSignature.Get());
	}
}
//------------------------------------------------------------ RenderComponent ------------------------------------------------------------//

RenderComponent::RenderComponent()
{
	set_name("RenderComponent");

	ComPtr<ID3D12Device> device = GameFramework::instance()->device();
	if (!device)
	{
		CERROR("RenderComponent::awake: Device를 가져올 수 없습니다.");
		return;
	}

	// 2. 상수 버퍼의 힙(Heap) 속성을 정의합니다.
	// CPU에서 매 프레임 업데이트해야 하므로, CPU 쓰기 및 GPU 읽기가 모두 가능한 UPLOAD 힙 타입 사용합니다.
	D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_props.CreationNodeMask = 1;
	heap_props.VisibleNodeMask = 1;

	// 3. 상수 버퍼의 리소스(Resource) 속성을 정의합니다.
	// D3D12의 규칙에 따라, 상수 버퍼의 크기는 반드시 256바이트의 배수여야 합니다.
	UINT buffer_size = (sizeof(CbGameObjectInfo) + 255) & ~255;

	D3D12_RESOURCE_DESC resource_desc = {};
	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource_desc.Alignment = 0;
	resource_desc.Width = buffer_size;
	resource_desc.Height = 1;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 1;
	resource_desc.Format = DXGI_FORMAT_UNKNOWN;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	for (int i = 0; i < _cbGameObjectInfo.size(); ++i)
	{
		// 4. 위에서 정의한 속성들을 사용하여 실제 상수 버퍼 리소스를 생성합니다.
		HRESULT hr = device->CreateCommittedResource(
			&heap_props,                    // 힙 속성
			D3D12_HEAP_FLAG_NONE,
			&resource_desc,                 // 리소스 속성
			D3D12_RESOURCE_STATE_GENERIC_READ, // 업로드 힙의 초기 상태는 GENERIC_READ 입니다.
			nullptr,                        // 최적화된 초기화 값 없음
			IID_PPV_ARGS(&_cbGameObjectInfo[i])); // 생성된 리소스는 _cbGameObjectInfo 멤버에 저장됩니다.

		if (FAILED(hr))
		{
			CERROR("RenderComponent: 상수 버퍼 생성에 실패했습니다.");
			return;
		}

		// 5. 생성된 리소스의 가상 주소를 CPU가 쓰기 가능하도록 영구적으로 맵핑(mapping)합니다.
		//    _mappedCbGameObjectInfo 포인터를 통해 CPU에서 이 버퍼에 데이터를 쓸 수 있게 됩니다.
		//    (업로드 힙에 생성한 리소스는 Unmap을 호출할 필요가 없습니다.)
		_cbGameObjectInfo[i]->Map(0, nullptr, reinterpret_cast<void**>(&_mappedCbGameObjectInfo[i]));
	}
}

RenderComponent::~RenderComponent()
{
	for (int i = 0; i < _cbGameObjectInfo.size(); ++i)
	{
		if (_cbGameObjectInfo[i])
		{
			_cbGameObjectInfo[i]->Unmap(0, nullptr);
			_mappedCbGameObjectInfo[i] = nullptr;
			_cbGameObjectInfo[i].Reset();
		}
	}

	if (_occlusionQueryIndex != 0xFFFFFFFF) {
		OcclusionManager::instance()->release_query_index(_occlusionQueryIndex);
	}
}


BoundingOrientedBox RenderComponent::get_world_bounding_box() const
{
	if (_isStatic && _isCached)
		return _cachedWorldOBB;

	return get_world_bounding_box_internal();
}

void RenderComponent::mark_as_static()
{
	_isStatic = true;
	if (!_mesh) return;
	_cachedWorldOBB = get_world_bounding_box_internal();
	_isCached = true;
}

BoundingOrientedBox RenderComponent::get_world_bounding_box_internal() const
{
	BoundingOrientedBox localObb = _mesh->bounding_box();
	BoundingOrientedBox worldObb; //    결과를 담을 별도의 변수

	if (game_object() && game_object()->transform())
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&game_object()->transform()->world_matrix());
		localObb.Transform(worldObb, worldMatrix); // [수정] 첫 번째와 두 번째 파라미터 분리
	}
	else
	{
		worldObb = localObb;
	}

	return worldObb;
}

bool RenderComponent::is_visible(const BoundingFrustum& frustum) const
{
	if (!_mesh) return false;
	if (!_frustumCullingEnabled) return true;

	if (_isStatic && _isCached)
		return frustum.Intersects(_cachedWorldOBB);

	if (_isStatic && !_isCached && _mesh)
	{
		return frustum.Intersects(get_world_bounding_box_internal());
	}

	BoundingOrientedBox obb = get_world_bounding_box();

	// [박스 유효성 검사] 박스가 0이거나 깨졌으면 그리지 않음 (false)
	if (obb.Extents.x <= 0.0f || std::isnan(obb.Center.x))
	{
		return false;
	}

	return frustum.Intersects(obb);
}

void RenderComponent::pre_render(ID3D12GraphicsCommandList* commandList, class Renderer* renderer)
{
}

const std::string& RenderComponent::pso_name() const
{
	return _psoName;
}

void RenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
	// 여기서 체크: 이미 Shadow Pass에서 계산했다면 아무것도 안 하고 통과함
	update_world_matrix_cb(frame_index);

	// 계산된(혹은 이미 있던) 상수 버퍼 주소만 GPU에 전달
	commandList->SetGraphicsRootConstantBufferView(0, _cbGameObjectInfo[frame_index]->GetGPUVirtualAddress());

	pre_render(commandList, Renderer::instance());
	_mesh->render(commandList);
}

void RenderComponent::render_CascadeShadowMap(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
	if (!_mesh) return;

	// 여기서 체크: 3개의 캐스케이드 중 첫 번째 호출 때만 계산하고 나머지는 통과
	update_world_matrix_cb(frame_index);

	commandList->SetGraphicsRootConstantBufferView(0, _cbGameObjectInfo[frame_index]->GetGPUVirtualAddress());
	_mesh->render_CascadeShadowMap(commandList);
}


UINT RenderComponent::get_occlusion_query_index()
{
	// 처음 호출될 때 매니저로부터 인덱스 할당
	if (_occlusionQueryIndex == 0xFFFFFFFF) {
		_occlusionQueryIndex = OcclusionManager::instance()->allocate_query_index();
	}
	return _occlusionQueryIndex;
}

XMMATRIX RenderComponent::get_occlusion_box_world_matrix() {
	// 1. 객체의 로컬 바운딩 박스 정보 가져오기
	BoundingOrientedBox obb = get_world_bounding_box();

	// 2. 바운딩 박스의 중심점(Center)과 크기(Extents)를 행렬로 변환
	// 쿼리용 Unit Cube가 (-0.5~0.5) 크기라고 가정할 때:
	float box_scale = 2.0f;
	XMMATRIX scale = XMMatrixScaling(obb.Extents.x * box_scale, obb.Extents.y * box_scale, obb.Extents.z * box_scale);
	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&obb.Orientation));
	XMMATRIX translation = XMMatrixTranslation(obb.Center.x, obb.Center.y, obb.Center.z);

	// 3. 최종 박스 월드 행렬 = Scale * Rotation * Translation
	return scale * rotation * translation;
}

void RenderComponent::update_world_matrix_cb(UINT frame_index)
{
	// 1. 현재 엔진의 전역 프레임 번호를 가져옵니다.
	UINT64 currentTotalFrame = GameFramework::instance()->get_total_frame_count();

	// 2. 이번 실제 프레임에 이미 계산을 마쳤다면 즉시 리턴 (중복 계산 방지 핵심)
	if (_lastUpdatedFrame == currentTotalFrame)
		return;

	// 3. 행렬 계산 시작 (이제 이 부분은 프레임당 딱 한 번만 실행됩니다.)
	const XMFLOAT4X4& worldMatrixData = game_object()->transform()->world_matrix();
	XMMATRIX worldMatrix = XMLoadFloat4x4(&worldMatrixData);

	// World Matrix (Transpose해서 GPU 전송용으로 저장)
	XMStoreFloat4x4(&_mappedCbGameObjectInfo[frame_index]->_world, XMMatrixTranspose(worldMatrix));

	// 역행렬 계산 (Shadow Pass와 Main Pass에서 중복으로 하던 비싼 연산)
	XMMATRIX worldInverse = XMMatrixInverse(nullptr, worldMatrix);
	XMStoreFloat4x4(&_mappedCbGameObjectInfo[frame_index]->_worldInverseTranspose, worldInverse);

	// 4. 기타 상태값 설정 (기존 render() 함수에 있던 로직들)
	if (_forcePlayerColorId != -1) {
		_mappedCbGameObjectInfo[frame_index]->otherplayer_id = _forcePlayerColorId;
	}
	else if (auto op_script = game_object()->get_component<OtherPlayerScript>()) {
		_mappedCbGameObjectInfo[frame_index]->otherplayer_id = static_cast<int>(op_script->id());
	}
	else if (auto mp_script = game_object()->get_component<MainPlayerScript>()) {
		_mappedCbGameObjectInfo[frame_index]->otherplayer_id = static_cast<int>(mp_script->id());
	}
	else {
		_mappedCbGameObjectInfo[frame_index]->otherplayer_id = -1;
	}

	// 그림자 수신 여부 설정
	if (_psoName == "skinned") {
		_mappedCbGameObjectInfo[frame_index]->bReceiveShadow = 0;
	}
	else {
		_mappedCbGameObjectInfo[frame_index]->bReceiveShadow = 1;
	}

	// 5. 업데이트 완료 표시
	_lastUpdatedFrame = currentTotalFrame;
}