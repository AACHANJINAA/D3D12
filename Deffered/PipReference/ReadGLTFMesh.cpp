#include "stdafx.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"

ReadGLTFMesh::ReadGLTFMesh(const std::string & filePath, bool is_animated, std::string animation_name)
{
	_is_animated = is_animated;
	_include_animation_name = animation_name;

	if (_is_animated)
	{
		read_skinned_animation_mesh(filePath);
	}
	else
	{
		read_static_mesh(filePath);
	}
}

ReadGLTFMesh::~ReadGLTFMesh()
{
	_primitives.clear();
}

void ReadGLTFMesh::upload_to_gpu_internal(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT64 targetFenceValue)
{
	// 1. 각 프리미티브의 정점/인덱스 버퍼 생성 (기존 로직 유지)
	for (const auto& primitive : _primitives)
	{
		if (primitive->_vertexCount > 0)
		{
			// 스키닝 데이터 유무에 따라 데이터 포인터와 크기 결정
			void* data_ptr = nullptr;
			UINT stride_in_bytes = 0;
			UINT buffer_size = 0;

			if (!primitive->_skinned_vertices.empty())
			{
				// 애니메이션 메쉬 (Skinned)
				data_ptr = primitive->_skinned_vertices.data();
				stride_in_bytes = sizeof(GltfSkinnedVertex);
				buffer_size = stride_in_bytes * primitive->_vertexCount;
			}
			else
			{
				// 정적 메쉬 (Static)
				data_ptr = primitive->_vertices.data();
				stride_in_bytes = sizeof(GltfVertex);
				buffer_size = stride_in_bytes * primitive->_vertexCount;
			}

			// 정점 버퍼 생성
			primitive->_vertexBuffer = ::CreateBufferResource(
				device, commandList,
				data_ptr,
				buffer_size,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
				&primitive->_vertexUploadBuffer
			);

			// 정점 버퍼 뷰 설정
			primitive->_vertexBufferView.BufferLocation = primitive->_vertexBuffer->GetGPUVirtualAddress();
			primitive->_vertexBufferView.StrideInBytes = stride_in_bytes;
			primitive->_vertexBufferView.SizeInBytes = buffer_size;
		}

		// 인덱스 버퍼 생성
		if (primitive->_indexCount > 0)
		{
			primitive->_indexBuffer = ::CreateBufferResource(
				device, commandList,
				primitive->_indices.data(),
				sizeof(UINT) * primitive->_indexCount,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_INDEX_BUFFER,
				&primitive->_indexUploadBuffer
			);

			// 인덱스 버퍼 뷰 설정
			primitive->_indexBufferView.BufferLocation = primitive->_indexBuffer->GetGPUVirtualAddress();
			primitive->_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			primitive->_indexBufferView.SizeInBytes = sizeof(UINT) * primitive->_indexCount;
		}
	}

	// 2. [추가] 애니메이션 뼈대용 상수 버퍼(Constant Buffer) 생성
	//    이 버퍼는 매 프레임 CPU에서 갱신되므로 D3D12_HEAP_TYPE_UPLOAD로 생성합니다.
	//if (!_joints.empty() && !_bone_palette_buffer)
	//{
	//	UINT element_size = sizeof(DirectX::XMFLOAT4X4);
	//	UINT buffer_size = (UINT)(_joints.size() * element_size);
	//	buffer_size = (buffer_size + 255) & ~255;

	//	if (!_joints.empty() && !_bone_palette_buffer)
	//	{
	//		// [수정] 임시 객체의 주소를 바로 딸 수 없으므로, 변수로 먼저 만듭니다.
	//		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	//		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

	//		HRESULT hr = device->CreateCommittedResource(
	//			&heapProps,         // 이제 변수의 주소를 넘기므로 안전합니다.
	//			D3D12_HEAP_FLAG_NONE,
	//			&bufferDesc,        // 변수의 주소
	//			D3D12_RESOURCE_STATE_GENERIC_READ,
	//			nullptr,
	//			IID_PPV_ARGS(&_bone_palette_buffer)
	//		);

	//		if (FAILED(hr))
	//		{
	//			// 에러 처리
	//			return;
	//		}

	//		_bone_palette_buffer->SetName(L"BonePaletteBuffer");
	//	}
	//}

	// 3. 첫 번째 프리미티브 정보를 기본 클래스에 복사 (기존 로직)
	if (!_primitives.empty())
	{
		const auto& first_primitive = _primitives[0];
		_vertexBufferView = first_primitive->_vertexBufferView;
		_indexBufferView = first_primitive->_indexBufferView;

		_indices.resize(first_primitive->_indexCount);
		memcpy(_indices.data(), first_primitive->_indices.data(), sizeof(UINT) * first_primitive->_indexCount);
	}

	auto rm = ResourceManager::instance();
	for (auto& prim : _primitives)
	{
		if (prim->_vertexUploadBuffer) {
			rm->register_upload_buffer(prim->_vertexUploadBuffer, targetFenceValue);
			prim->_vertexUploadBuffer.Reset();
		}
		if (prim->_indexUploadBuffer) {
			rm->register_upload_buffer(prim->_indexUploadBuffer, targetFenceValue);
			prim->_indexUploadBuffer.Reset();
		}
	}

	// 업로드 완료 플래그 설정
	_isUploaded = true;
}


void ReadGLTFMesh::render(ID3D12GraphicsCommandList* commandList)
{
	if (!_isUploaded) return;

	if (_is_animated) // 애니메이션 메쉬인 경우 스키닝 렌더링 호출
	{
		render_skinned(commandList);
	}

	commandList->IASetPrimitiveTopology(_primitiveTopology);

	for (const auto& primitive : _primitives)
	{
		if (primitive->_materialIndex >= 0 && primitive->_materialIndex < _material_names.size())
		{
			ResourceManager::instance()->bind_material(_material_names[primitive->_materialIndex], commandList);
		}

		//else
		//{
			// 유효하지 않은 재질 인덱스 또는 재질 이름이 없는 경우 기본 재질 등을 바인딩할 수 있습니다.
			// TODO: 기본 재질 바인딩 로직 추가 (예: ResourceManager::instance()->bind_default_material(commandList);)
		//}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &primitive->_vertexBufferView);
		commandList->IASetIndexBuffer(&primitive->_indexBufferView);
		commandList->DrawIndexedInstanced(primitive->_indexCount, 1, 0, 0, 0);
	}
}

void ReadGLTFMesh::render_instance(ID3D12GraphicsCommandList* commandList, size_t want_instance_count)
{
	if (!_isUploaded) return;

	if (_is_animated) // 애니메이션 메쉬인 경우 스키닝 렌더링 호출
	{
		render_skinned(commandList);
	}

	commandList->IASetPrimitiveTopology(_primitiveTopology);

	for (const auto& primitive : _primitives)
	{
		if (primitive->_materialIndex >= 0 && primitive->_materialIndex < _material_names.size())
		{
			ResourceManager::instance()->bind_material(_material_names[primitive->_materialIndex], commandList);
		}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &primitive->_vertexBufferView);
		commandList->IASetIndexBuffer(&primitive->_indexBufferView);
		commandList->DrawIndexedInstanced(primitive->_indexCount, static_cast<UINT>(want_instance_count), 0, 0, 0);
	}
}

void ReadGLTFMesh::render_instance_CascadeShadowMap(ID3D12GraphicsCommandList* commandList, size_t want_instance_count)
{
	if (!_isUploaded || want_instance_count == 0) return;

	if (_is_animated) return; // 애니메이션 메쉬는 instancing 미지원 (각 인스턴스마다 다른 bone 필요)

	commandList->IASetPrimitiveTopology(_primitiveTopology);

	// 모든 프리미티브를 instancing으로 렌더링
	for (const auto& primitive : _primitives)
	{
		if (primitive->_vertexCount == 0 || primitive->_indexCount == 0) continue;

		// VBV, IBV 바인딩
		commandList->IASetVertexBuffers(0, 1, &primitive->_vertexBufferView);
		commandList->IASetIndexBuffer(&primitive->_indexBufferView);

		// DrawIndexedInstanced: 모든 인스턴스를 한 드로우 콜로 렌더링
		commandList->DrawIndexedInstanced(
			primitive->_indexCount,    // IndexCountPerInstance
			(UINT)want_instance_count, // InstanceCount
			0,                         // StartIndexLocation
			0,                         // BaseVertexLocation
			0                          // StartInstanceLocation
		);
	}
}

void ReadGLTFMesh::release_upload_buffers()
{
	for (auto& primitive : _primitives)
	{
		if (primitive->_vertexUploadBuffer) primitive->_vertexUploadBuffer.Reset();
		if (primitive->_indexUploadBuffer) primitive->_indexUploadBuffer.Reset();
	}
}

void ReadGLTFMesh::render_CascadeShadowMap(ID3D12GraphicsCommandList* commandList)
{
	if (!_isUploaded) return;

	// 1. 토폴로지 설정 (삼각형 리스트)
	commandList->IASetPrimitiveTopology(_primitiveTopology);

	// 2. glTF 내부의 모든 세부 메쉬(Primitives)를 순회하며 렌더링
	for (const auto& primitive : _primitives)
	{
		// 각 프리미티브의 전용 정점 버퍼 바인딩
		commandList->IASetVertexBuffers(0, 1, &primitive->_vertexBufferView);

		if (primitive->_indexCount > 0)
		{
			// 인덱스 버퍼가 있는 경우
			commandList->IASetIndexBuffer(&primitive->_indexBufferView);

			// [중요] 인스턴싱 카운트를 3으로 설정 (3개의 Cascade에 동시 렌더링)
			commandList->DrawIndexedInstanced(primitive->_indexCount, 3, 0, 0, 0);
		}
		else
		{
			// 인덱스 버퍼가 없는 경우
			commandList->DrawInstanced(primitive->_vertexCount, 3, 0, 0);
		}
	}
}

void ReadGLTFMesh::update_animation(float& delta_time, const std::string& animation_name, UINT8* mapped_buffer, bool _isLoop)
{
	// T-Pose 또는 유효하지 않은 클립 인덱스 체크
	if (animation_name == "t_pose" || !_animations.contains(animation_name))
	{
		for (size_t i = 0; i < _nodes.size(); ++i) {
			if (_nodes[i]._parent_index == -1) {
				update_node_hierarchy((int)i, XMMatrixIdentity());
			}
		}

		// [핵심] 애니메이션이 없으면 모든 뼈대를 항등행렬로 밀어버림 (T-Pose)
		DirectX::XMFLOAT4X4 identity;
		DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
		std::fill(_final_bone_transforms.begin(), _final_bone_transforms.end(), identity);

		if (mapped_buffer)
		{
			memcpy(mapped_buffer, _final_bone_transforms.data(), _final_bone_transforms.size() * sizeof(DirectX::XMFLOAT4X4));
		}
		return;
	}

	const AnimationClip& clip = _animations.find(animation_name)->second;

	// 1. 시간 갱신 (Looping 처리)
	//_current_animation_time += delta_time;
	if (_isLoop)
	{
		if (clip._duration > 0.0f) {
			delta_time = fmod(delta_time, clip._duration);
		}
	}
	else
	{
		if (delta_time > clip._duration) {
			delta_time = clip._duration;
		}
	}
	

	// 2. 채널별 키프레임 보간 수행
	for (const auto& channel : clip._channels)
	{
		if (channel._keyframes.empty()) continue;

		NodeInfo& target_node = _nodes[channel._node_index];

		// 현재 시간에 맞는 키프레임 인덱스 찾기
		size_t prev_idx = 0;
		size_t next_idx = 0;

		if (channel._keyframes.size() <= 1) 
		{
			prev_idx = next_idx = 0;
		}
		else {
			// [최적화 적용] O(N) 선형 탐색을 O(log N) 이진 탐색으로 변경 -> delta_time 보다 큰 첫 번째 키프레임을 찾음
			auto it = std::upper_bound(channel._keyframes.begin(), channel._keyframes.end(), delta_time,
				[](float t, const Keyframe& key) {
					return t < key._time;
				});

			if (it == channel._keyframes.begin())  // delta_time이 첫 번째 키프레임보다 작은 경우 -> 처음
			{
				prev_idx = next_idx = 0;
			}
			else if (it == channel._keyframes.end()) // delta_time이 마지막 키프레임보다 큰 경우 -> 끝
			{
				prev_idx = next_idx = channel._keyframes.size() - 1;
			}
			else // delta_time이 두 키프레임 사이에 있는 경우 -> 가장 일반적인 경우 -> 중간
			{
				next_idx = std::distance(channel._keyframes.begin(), it); // 이터레이터에서 인덱스로 변환
				prev_idx = next_idx - 1; // 바로 이전 프레임
			}
		}

		const Keyframe& prev_key = channel._keyframes[prev_idx];
		const Keyframe& next_key = channel._keyframes[next_idx];

		float duration = next_key._time - prev_key._time;
		float t = (duration > 0.0f) ? (delta_time - prev_key._time) / duration : 0.0f;

		XMVECTOR final_value;

		// 보간 방식에 따른 계산
		if (channel._interpolation == AnimationInterpolation::Step)
		{
			final_value = XMLoadFloat4(&prev_key._value);
		}
		else if (channel._interpolation == AnimationInterpolation::CubicSpline)
		{
			float t2 = t * t;
			float t3 = t2 * t;

			XMVECTOR p0 = XMLoadFloat4(&prev_key._value);
			XMVECTOR p1 = XMLoadFloat4(&next_key._value);
			XMVECTOR m0 = XMLoadFloat4(&prev_key._out_tangent) * duration;
			XMVECTOR m1 = XMLoadFloat4(&next_key._in_tangent) * duration;

			final_value = (2 * t3 - 3 * t2 + 1) * p0 +
				(t3 - 2 * t2 + t) * m0 +
				(-2 * t3 + 3 * t2) * p1 +
				(t3 - t2) * m1;
		}
		else // Linear
		{
			XMVECTOR v0 = XMLoadFloat4(&prev_key._value);
			XMVECTOR v1 = XMLoadFloat4(&next_key._value);

			if (channel._path == "rotation") {
				final_value = XMQuaternionSlerp(v0, v1, t);
			}
			else {
				final_value = XMVectorLerp(v0, v1, t);
			}
		}

		// 3. 노드 상태 업데이트
		if (channel._path == "translation") {
			XMStoreFloat3(&target_node._translation, final_value);
		}
		else if (channel._path == "rotation") {
			// 정규화 수행
			final_value = XMQuaternionNormalize(final_value);
			XMStoreFloat4(&target_node._rotation, final_value);
		}
		else if (channel._path == "scale") {
			XMStoreFloat3(&target_node._scale, final_value);
		}
	}

	// 4. 노드 계층 구조 전체 갱신 (Local -> Global)
	// DW설명 : 여기서 Global은 월드 좌표계가 아닌 부모 노드 기준 모델의 변환을 담은 좌표계를 뜻한다.
	// 즉 이걸로 올바른 좌표를 구하고 싶다면 월드 행렬을 곱해주어야 함
	for (size_t i = 0; i < _nodes.size(); ++i) {
		if (_nodes[i]._parent_index == -1) {
			update_node_hierarchy((int)i, XMMatrixIdentity());
		}
	}

	// 5. 스키닝 행렬(Matrix Palette) 계산
	for (size_t i = 0; i < _joints.size(); ++i)
	{
		int node_idx = _joints[i];
		XMMATRIX global_transform = XMLoadFloat4x4(&_nodes[node_idx]._global_transform);
		XMMATRIX inverse_bind_matrix = XMLoadFloat4x4(&_skeleton[i]._inverse_bind_matrix);

		// Final = InverseBindMatrix * GlobalTransform
		XMMATRIX final_matrix = inverse_bind_matrix * global_transform;

		// GPU 전송을 위해 Transpose (Row-Major)
		XMStoreFloat4x4(&_final_bone_transforms[i], XMMatrixTranspose(final_matrix));
		// XMStoreFloat4x4(&_final_bone_transforms[i], final_matrix);
	}

	// 6. GPU 상수 버퍼 업로드
	if (mapped_buffer)
	{
		memcpy(mapped_buffer, _final_bone_transforms.data(), _final_bone_transforms.size() * sizeof(DirectX::XMFLOAT4X4));
	}
}

void ReadGLTFMesh::update_animation(float& delta_time, std::string animation_name, std::vector<DirectX::XMFLOAT4X4>& bone_transforms, bool _isLoop)
{
	if (animation_name == "t_pose" || !_animations.contains(animation_name))
	{
		for (size_t i = 0; i < _nodes.size(); ++i) {
			if (_nodes[i]._parent_index == -1) {
				update_node_hierarchy((int)i, XMMatrixIdentity());
			}
		}

		// [핵심] 애니메이션이 없으면 모든 뼈대를 항등행렬로 밀어버림 (T-Pose)
		DirectX::XMFLOAT4X4 identity;
		DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
		std::fill(bone_transforms.begin(), bone_transforms.end(), identity);

		return;
	}

	const AnimationClip& clip = _animations.find(animation_name)->second;

	// 1. 시간 갱신 (Looping 처리)
	//_current_animation_time += delta_time;
	if (_isLoop)
	{
		if (clip._duration > 0.0f) {
			delta_time = fmod(delta_time, clip._duration);
		}
	}
	else
	{
		if (delta_time > clip._duration) {
			delta_time = clip._duration;
		}
	}

	// 2. 채널별 키프레임 보간 수행
	for (const auto& channel : clip._channels)
	{
		if (channel._keyframes.empty()) continue;

		NodeInfo& target_node = _nodes[channel._node_index];

		// 현재 시간에 맞는 키프레임 인덱스 찾기
		size_t prev_idx = 0;
		size_t next_idx = 0;

		if (channel._keyframes.size() == 1) {
			prev_idx = next_idx = 0;
		}
		else {
			for (size_t i = 0; i < channel._keyframes.size() - 1; ++i) {
				if (delta_time >= channel._keyframes[i]._time &&
					delta_time < channel._keyframes[i + 1]._time) {
					prev_idx = i;
					next_idx = i + 1;
					break;
				}
			}
			if (delta_time >= channel._keyframes.back()._time) {
				prev_idx = next_idx = channel._keyframes.size() - 1;
			}
		}

		const Keyframe& prev_key = channel._keyframes[prev_idx];
		const Keyframe& next_key = channel._keyframes[next_idx];

		float duration = next_key._time - prev_key._time;
		float t = (duration > 0.0f) ? (delta_time - prev_key._time) / duration : 0.0f;

		XMVECTOR final_value;

		// 보간 방식에 따른 계산
		if (channel._interpolation == AnimationInterpolation::Step)
		{
			final_value = XMLoadFloat4(&prev_key._value);
		}
		else if (channel._interpolation == AnimationInterpolation::CubicSpline)
		{
			float t2 = t * t;
			float t3 = t2 * t;

			XMVECTOR p0 = XMLoadFloat4(&prev_key._value);
			XMVECTOR p1 = XMLoadFloat4(&next_key._value);
			XMVECTOR m0 = XMLoadFloat4(&prev_key._out_tangent) * duration;
			XMVECTOR m1 = XMLoadFloat4(&next_key._in_tangent) * duration;

			final_value = (2 * t3 - 3 * t2 + 1) * p0 +
				(t3 - 2 * t2 + t) * m0 +
				(-2 * t3 + 3 * t2) * p1 +
				(t3 - t2) * m1;
		}
		else // Linear
		{
			XMVECTOR v0 = XMLoadFloat4(&prev_key._value);
			XMVECTOR v1 = XMLoadFloat4(&next_key._value);

			if (channel._path == "rotation") {
				final_value = XMQuaternionSlerp(v0, v1, t);
			}
			else {
				final_value = XMVectorLerp(v0, v1, t);
			}
		}

		// 3. 노드 상태 업데이트
		if (channel._path == "translation") {
			XMStoreFloat3(&target_node._translation, final_value);
		}
		else if (channel._path == "rotation") {
			// 정규화 수행
			final_value = XMQuaternionNormalize(final_value);
			XMStoreFloat4(&target_node._rotation, final_value);
		}
		else if (channel._path == "scale") {
			XMStoreFloat3(&target_node._scale, final_value);
		}
	}

	// 4. 노드 계층 구조 전체 갱신 (Local -> Global)
	for (size_t i = 0; i < _nodes.size(); ++i) {
		if (_nodes[i]._parent_index == -1) {
			update_node_hierarchy((int)i, XMMatrixIdentity());
		}
	}

	// 5. 스키닝 행렬(Matrix Palette) 계산
	for (size_t i = 0; i < _joints.size(); ++i)
	{
		int node_idx = _joints[i];
		XMMATRIX global_transform = XMLoadFloat4x4(&_nodes[node_idx]._global_transform);
		XMMATRIX inverse_bind_matrix = XMLoadFloat4x4(&_skeleton[i]._inverse_bind_matrix);

		// Final = InverseBindMatrix * GlobalTransform
		XMMATRIX final_matrix = inverse_bind_matrix * global_transform;

		// GPU 전송을 위해 Transpose (Row-Major)
		XMStoreFloat4x4(&bone_transforms[i], XMMatrixTranspose(final_matrix));
	}

	// 6. GPU 업로드
	// 이건 바깥에서 처리할것임 -> 인스턴싱을 위해 추후에 SRV에 넣어질 예정
}

void ReadGLTFMesh::render_skinned(ID3D12GraphicsCommandList* commandList)
{
	// 뼈대 행렬 팔레드 GPU 상수 버퍼에 바인딩
	// SkinnedRootSignatureGenerator에서 뼈대 버퍼는 8번 파라미터 (b4)로 정의

	// 만약 AnimationComponent에서 제공한 버퍼가 있으면 그것을 사용
	/*if (_bone_palette_buffer_from_animation_component)
	{
		commandList->SetGraphicsRootConstantBufferView(12, _bone_palette_buffer_from_animation_component->GetGPUVirtualAddress());
	}
	else if (_bone_palette_buffer)
	{
		commandList->SetGraphicsRootConstantBufferView(12, _bone_palette_buffer->GetGPUVirtualAddress());
	}*/
}

void ReadGLTFMesh::render_instance_skinned(ID3D12GraphicsCommandList* commandList)
{
	// 뼈대 행렬 팔레드 GPU 상수 버퍼에 바인딩
	// SkinnedRootSignatureGenerator에서 뼈대 버퍼는 8번 파라미터 (b4)로 정의

	// 만약 AnimationComponent에서 제공한 버퍼가 있으면 그것을 사용
	//if(_bone_palette_buffer_from_animation_component)
	//{
	//	commandList->SetGraphicsRootConstantBufferView(12, _bone_palette_buffer_from_animation_component->GetGPUVirtualAddress());
	//}
	//else if (_bone_palette_buffer)
	//{
	//	commandList->SetGraphicsRootConstantBufferView(12, _bone_palette_buffer->GetGPUVirtualAddress());
	//}
}

std::vector<DirectX::XMFLOAT3> ReadGLTFMesh::extract_particle_targets(UINT particleCount) const
{
	std::vector<DirectX::XMFLOAT3> targets;
	targets.reserve(particleCount);

	struct Triangle {
		DirectX::XMFLOAT3 v0, v1, v2;
		float area;
	};
	std::vector<Triangle> triangles;
	std::vector<float> cumulativeAreas;
	float totalArea = 0.0f;

	// 1. 모든 프리미티브를 순회하며 삼각형 단위로 쪼개고, 각 삼각형의 넓이를 계산
	for (const auto& primitive : _primitives)
	{
		// 무기는 보통 vertices에 데이터가 있음
		if (primitive->_vertices.empty() || primitive->_indices.empty()) continue;

		const auto& vertices = primitive->_vertices;
		const auto& indices = primitive->_indices;

		// 인덱스를 3개씩 묶어 하나의 삼각형으로 처리
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			UINT i0 = indices[i];
			UINT i1 = indices[i + 1];
			UINT i2 = indices[i + 2];

			DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&vertices[i0]._position);
			DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&vertices[i1]._position);
			DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&vertices[i2]._position);

			// 외적(Cross Product)을 이용해 삼각형의 넓이를 구함 (Area = 0.5 * |(A-B) x (A-C)|)
			DirectX::XMVECTOR cross = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(p1, p0), DirectX::XMVectorSubtract(p2, p0));
			float area = 0.5f * DirectX::XMVectorGetX(DirectX::XMVector3Length(cross));

			if (area > 0.0f) {
				triangles.push_back({ vertices[i0]._position, vertices[i1]._position, vertices[i2]._position, area });
				totalArea += area;
				cumulativeAreas.push_back(totalArea);
			}
		}
	}

	if (triangles.empty()) return targets;

	// 2. 넓이에 비례하여 무작위로 삼각형을 선택하고, 그 내부에 점을 찍기
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> areaDist(0.0f, totalArea);
	std::uniform_real_distribution<float> baryDist(0.0f, 1.0f);

	for (UINT i = 0; i < particleCount; ++i)
	{
		// 누적 넓이를 이용해 가중치 랜덤 선택 (넓은 삼각형일수록 뽑힐 확률이 높음 굳!)
		float randomArea = areaDist(gen);
		auto it = std::lower_bound(cumulativeAreas.begin(), cumulativeAreas.end(), randomArea);
		size_t triIndex = std::min(static_cast<size_t>(std::distance(cumulativeAreas.begin(), it)), triangles.size() - 1);

		const Triangle& tri = triangles[triIndex];

		// 3. 무게중심 좌표계(Barycentric)를 이용해 삼각형 내부의 랜덤한 점을 구하기
		float r1 = baryDist(gen);
		float r2 = baryDist(gen);

		// 점들이 꼭짓점에 뭉치지 않고 균일하게 퍼지도록 제곱근(sqrt)을 적용하기
		float sqrtR1 = std::sqrt(r1);
		float u = 1.0f - sqrtR1;
		float v = sqrtR1 * (1.0f - r2);
		float w = sqrtR1 * r2;

		DirectX::XMFLOAT3 point;
		point.x = u * tri.v0.x + v * tri.v1.x + w * tri.v2.x;
		point.y = u * tri.v0.y + v * tri.v1.y + w * tri.v2.y;
		point.z = u * tri.v0.z + v * tri.v1.z + w * tri.v2.z;

		targets.push_back(point);
	}

	return targets;
}

void ReadGLTFMesh::read_static_mesh(const std::string& filePath)
{
	// DW설명 : 파일 경로를 이름으로 설정
	set_name(filePath);

	json gltf_json;
	std::vector<char> binary_buffer;

	//CLOG("현재 디렉토리: " << std::filesystem::current_path() << std::endl);
	if (!load_gltf_file(filePath, gltf_json, binary_buffer))
	{
		CERROR("glTF 파일 로딩에 실패했습니다.");
		return;
	}

	// [추가] ResourceManager를 통해 재질 로드 및 이름 목록 채우기
	_material_names = ResourceManager::instance()->load_materials_from_gltf(filePath);

	int scene_idx = gltf_json.value("scene", 0);
	const json& scene = gltf_json["scenes"][scene_idx];

	XMFLOAT4X4 identity_matrix;
	XMStoreFloat4x4(&identity_matrix, XMMatrixIdentity());

	for (const auto& node_idx : scene["nodes"])
	{
		process_node(gltf_json, binary_buffer, node_idx.get<int>(), identity_matrix);
	}


	bounding_box_merge();

	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void ReadGLTFMesh::read_skinned_animation_mesh(const std::string& filePath)
{
	// --- 1. 공통 코드 (파일 로드) ---
	set_name(filePath);

	json gltf_json;
	std::vector<char> binary_buffer;

	if (!load_gltf_file(filePath, gltf_json, binary_buffer))
	{
		CERROR("glTF 파일 로딩에 실패했습니다.");
		return;
	}

	// 재질 로드
	_material_names = ResourceManager::instance()->load_materials_from_gltf(filePath);

	// --- 2. [신규] 스키닝/애니메이션 데이터 로드 ---
	// 
	// skins 배열을 파싱 -> 뼈대 정보 관련 파싱
	load_skins(gltf_json, binary_buffer);

	// animations 배열을 파싱하여 키프레임 데이터를
	// _animations 멤버 변수에 저장
	load_animations(gltf_json, binary_buffer);
	load_nodes(gltf_json);

	// --- 3. 메쉬 노드 처리 ---
	// ReadStaticMesh의 process_node와 달리, 
	// T-포즈를 유지하고 뼈대 데이터를 로드하기 위해 씬을 직접 순회

	int scene_idx = gltf_json.value("scene", 0);

	const json& scene = gltf_json["scenes"][scene_idx];

	std::stack<int> node_stack;
	for (const auto& node_idx_json : scene["nodes"])
	{
		node_stack.push(node_idx_json.get<int>());
	}

	while (!node_stack.empty())
	{
		int node_index = node_stack.top();
		node_stack.pop();

		const json& node = gltf_json["nodes"][node_index];

		// 메쉬 노드를 찾으면, 스키닝용 메쉬 처리 함수를 호출
		if (node.contains("mesh"))
		{
			// 이 메쉬가 스키닝을 사용하는지 확인 (중요)
			if (node.contains("skin"))
			{
				// T-포즈를 유지하고 JOINTS_0, WEIGHTS_0를 읽는 헬퍼 함수
				int skin_index = node["skin"].get<int>(); // 파츠의 스킨 번호 획득
				process_skinned_mesh(gltf_json, binary_buffer, gltf_json["meshes"][node["mesh"].get<int>()], skin_index);
			}
			// else 
			// {
			//     (선택) 스키닝이 없는 메쉬도 T-포즈로 로드 (예: 무기)
			//     process_static_mesh_tpose(...);
			// }
		}

		if (node.contains("children"))
		{
			for (const auto& child_index_json : node["children"])
			{
				node_stack.push(child_index_json.get<int>());
			}
		}
	}

	// --- 4. 바운딩 박스 계산 ---
	bounding_box_merge();

	// 프리미티브 토폴로지 설정
	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void ReadGLTFMesh::load_skins(const json& gltf_json, const std::vector<char>& binary_buffer)
{
	// 스킨이 없으면 애니메이션 모델이 아님 (또는 스키닝이 없음)
	if (!gltf_json.contains("skins"))
	{
		CLOG("glTF 파일에 'skins' 배열이 없습니다. 스키닝 로드를 건너뜁니다.");
		return;
	}

	// 우리는 이 예제에서 0번 스킨만 로드한다고 가정 -> 이것도 수정예정, 아직 여러개 있는 경우를 못찾음

	// DW 설명 : [마스터 스켈레톤 방식 적용] -> 또 안불러와지는 것이 있다면 나에게로...
	// 헬멧, 장갑 등 여러 파츠로 인해 skins 배열에 여러 항목이 존재하더라도,
	// 메인 몸통인 0번 스킨 딱 하나만 로드하여 이를 '마스터 뼈대'로 삼아버림
	// 나머지 파츠들은 렌더링 시 이 마스터 뼈대를 참조하여 움직임
	const json& skin = gltf_json["skins"][0];

	// --- 1. 'joints' 배열 로드 (GPU 행렬 팔레트 순서) ---
	// _joints 멤버 변수에 뼈대로 사용될 'node' 인덱스 목록을 복사
	_joints = skin["joints"].get<std::vector<int>>();

	size_t num_joints = _joints.size();
	if (num_joints == 0)
	{
		CERROR("Skin에 'joints'가 없습니다.");
		return;
	}

	// --- 2. 뼈대 정보 컨테이너 크기 초기화 ---
	// _skeleton 멤버 변수의 크기를 뼈대 개수(72개)만큼 설정 -> 여기서 72라는 숫자는 일단 BruteHi가 72개라 적어둔것임
	// 이제 72개 뿐만 아니라 다르게 불러오기 가능 수정함
	_skeleton.resize(num_joints);
	// _final_bone_transforms 멤버 변수의 크기도 뼈대 개수만큼 설정하고,
	// 모두 단위 행렬로 초기화
	_final_bone_transforms.resize(num_joints);
	DirectX::XMFLOAT4X4 identity_matrix;
	DirectX::XMStoreFloat4x4(&identity_matrix, DirectX::XMMatrixIdentity());
	std::fill(_final_bone_transforms.begin(), _final_bone_transforms.end(), identity_matrix);


	// --- 3. 'inverseBindMatrices' 로드 ---
	int ibm_accessor_index = skin["inverseBindMatrices"].get<int>();
	// get_attribute_data 헬퍼 함수를 사용하여 바이너리 데이터 로드
	std::vector<DirectX::XMFLOAT4X4> inverse_bind_matrices = get_attribute_data<DirectX::XMFLOAT4X4>(gltf_json, binary_buffer, ibm_accessor_index);

	// --- 4. 뼈대 계층 구조 (부모-자식 관계) 구축 ---
	// 뼈대의 부모 인덱스를 찾기 위해 전체 'nodes' 배열을 순회하며 맵을 만든다람쥐
	std::unordered_map<int, int> node_to_parent_map; // key: 자식 노드 인덱스, value: 부모 노드 인덱스

	for (size_t i = 0; i < gltf_json["nodes"].size(); ++i)
	{
		const json& node = gltf_json["nodes"][i];
		if (node.contains("children"))
		{
			for (const auto& child_index_json : node["children"])
			{
				int child_node_index = child_index_json.get<int>();
				node_to_parent_map[child_node_index] = static_cast<int>(i); // 'i'가 부모 노드의 인덱스
			}
		}
	}

	// _skeleton 배열(0~71)의 인덱스를 쉽게 찾기 위해
	// 'node' 인덱스를 'joint' 인덱스로 변환하는 맵을 만든다.
	std::unordered_map<int, int> node_to_joint_map; // key: 노드 인덱스, value: joint 인덱스(0~71)
	for (size_t i = 0; i < num_joints; ++i)
	{
		node_to_joint_map[_joints[i]] = static_cast<int>(i);
	}

	// --- 5. 최종 _skeleton 멤버 변수 채우기 ---
	for (size_t i = 0; i < num_joints; ++i)
	{
		int node_index = _joints[i]; // glTF 'nodes' 배열에서의 실제 인덱스 (예: 72)
		const json& node = gltf_json["nodes"][node_index];

		_skeleton[i]._node_index = node_index;
		_skeleton[i]._name = node.value("name", "Bone_" + std::to_string(node_index));


		// DW설명 : 이 부분 절대 놓치지 말 것 이거 때문에 후..
		// 1. 파일에서 읽은 원본 행렬 (오른손 좌표계, Column-Major)
		// 메모리 레이아웃 상 Translation이 끝에 있으므로, DXMath(Row-Major)로 읽으면 포맷은 맞음
		DirectX::XMMATRIX gltf_matrix = DirectX::XMLoadFloat4x4(&inverse_bind_matrices[i]);

		// 수정
		//gltf_matrix = DirectX::XMMatrixTranspose(gltf_matrix);

		// 2. [추가] 좌표계 변환 (Right-Handed -> Left-Handed)
		// Z축을 반전시키는 스케일 행렬 생성
		DirectX::XMMATRIX z_flip = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);

		// 변환 공식: LHS_Matrix = Scale(1,1,-1) * RHS_Matrix * Scale(1,1,-1)
		// 이렇게 하면 Z와 관련된 회전/이동 성분들의 부호가 올바르게 반전됩니다.
		DirectX::XMMATRIX converted_matrix = z_flip * gltf_matrix * z_flip;

		// 3. 변환된 행렬 저장 (전치 X)
		DirectX::XMStoreFloat4x4(&_skeleton[i]._inverse_bind_matrix, converted_matrix);

		//// glTF는 열 우선(Column-Major), DirectX는 행 우선(Row-Major)
		//// 로드한 행렬을 전치(Transpose)하여 저장
		//DirectX::XMMATRIX col_major_matrix = DirectX::XMLoadFloat4x4(&inverse_bind_matrices[i]);
		//DirectX::XMMATRIX row_major_matrix = DirectX::XMMatrixTranspose(col_major_matrix);
		////DirectX::XMStoreFloat4x4(&_skeleton[i]._inverse_bind_matrix, row_major_matrix);
		//DirectX::XMStoreFloat4x4(&_skeleton[i]._inverse_bind_matrix, col_major_matrix);	

		// 위에서 만든 맵을 사용하여 부모 뼈대의 인덱스(_skeleton 기준)를 찾는다.
		if (node_to_parent_map.find(node_index) != node_to_parent_map.end())
		{
			int parent_node_index = node_to_parent_map[node_index];

			// 부모 노드도 뼈대('joints' 목록에 포함)인지 확인
			if (node_to_joint_map.find(parent_node_index) != node_to_joint_map.end())
			{
				// _skeleton 배열 내의 부모 인덱스를 저장
				_skeleton[i]._parent_index = node_to_joint_map[parent_node_index];
			}
			else
			{
				// 부모가 뼈대가 아닌 경우 (예: 일반 노드 밑에 뼈대가 있음)
				_skeleton[i]._parent_index = -1; // 루트 뼈대로 취급
			}
		}
		else
		{
			// 부모가 없는 경우 (씬의 루트 노드이거나 스켈레톤의 루트)
			_skeleton[i]._parent_index = -1; // 루트 뼈대
		}
	}
}

void ReadGLTFMesh::load_animations(const json& gltf_json, const std::vector<char>& binary_buffer)
{
	if (!gltf_json.contains("animations")) return;

	const json& animations_node = gltf_json["animations"];
	//_animations.resize(animations_node.size());

	for (size_t i = 0; i < animations_node.size(); ++i)
	{
		const json& anim_node = animations_node[i];
		std::string anim_name = gltf_json["animations"][i]["name"].get<std::string>();
		if ("null_name" != _include_animation_name)
		{
			anim_name = _include_animation_name;
		}
		//_animations.emplace(anim_name, AnimationClip);
		AnimationClip clip{};

		clip._name = anim_node.value("name", "anim_" + std::to_string(i));
		clip._duration = 0.0f;

		// 1. Samplers 데이터 미리 로드
		struct SamplerInfo {
			std::vector<float> times;
			std::vector<float> values;
			AnimationInterpolation interpolation;
			int type_component_count; // VEC3=3, VEC4=4
		};
		std::vector<SamplerInfo> samplers_data;

		const json& samplers_json = anim_node["samplers"];
		samplers_data.resize(samplers_json.size());

		for (size_t s = 0; s < samplers_json.size(); ++s)
		{
			const json& sampler_node = samplers_json[s];

			// Input (Time) Accessor 읽기
			int input_accessor_index = sampler_node["input"].get<int>();
			samplers_data[s].times = get_attribute_data<float>(gltf_json, binary_buffer, input_accessor_index);

			if (!samplers_data[s].times.empty())
			{
				float max_time = samplers_data[s].times.back();
				if (max_time > clip._duration) clip._duration = max_time;
			}

			// Interpolation 모드 설정
			std::string interpolation_str = sampler_node.value("interpolation", "LINEAR");
			samplers_data[s].interpolation = string_to_interpolation(interpolation_str);

			// Output (Values) Accessor 읽기
			int output_accessor_index = sampler_node["output"].get<int>();
			const json& output_accessor = gltf_json["accessors"][output_accessor_index];

			std::string type_str = output_accessor["type"].get<std::string>();
			int component_count = 0;

			// 타입에 따라 데이터를 올바르게 읽어서 flat float vector로 변환
			if (type_str == "SCALAR")
			{
				component_count = 1;
				samplers_data[s].values = get_attribute_data<float>(gltf_json, binary_buffer, output_accessor_index);
			}
			else if (type_str == "VEC3")
			{
				component_count = 3;
				std::vector<XMFLOAT3> vec3_data = get_attribute_data<XMFLOAT3>(gltf_json, binary_buffer, output_accessor_index);
				samplers_data[s].values.resize(vec3_data.size() * 3);
				memcpy(samplers_data[s].values.data(), vec3_data.data(), vec3_data.size() * sizeof(XMFLOAT3));
			}
			else if (type_str == "VEC4")
			{
				component_count = 4;
				std::vector<XMFLOAT4> vec4_data = get_attribute_data<XMFLOAT4>(gltf_json, binary_buffer, output_accessor_index);
				samplers_data[s].values.resize(vec4_data.size() * 4);
				memcpy(samplers_data[s].values.data(), vec4_data.data(), vec4_data.size() * sizeof(XMFLOAT4));
			}

			samplers_data[s].type_component_count = component_count;
		}

		// 2. Channels 파싱 및 데이터 매핑
		const json& channels_json = anim_node["channels"];
		for (const auto& channel_node : channels_json)
		{
			AnimationChannel channel;

			channel._node_index = channel_node["target"]["node"].get<int>();
			channel._path = channel_node["target"]["path"].get<std::string>();

			int sampler_index = channel_node["sampler"].get<int>();
			const SamplerInfo& sampler = samplers_data[sampler_index];

			channel._interpolation = sampler.interpolation;

			size_t keyframe_count = sampler.times.size();
			channel._keyframes.resize(keyframe_count);

			int values_per_keyframe = (channel._interpolation == AnimationInterpolation::CubicSpline) ? 3 : 1;
			int stride = sampler.type_component_count;

			// Weights 채널을 위한 Stride 재계산
			// Weights는 SCALAR 타입이지만, 모프 타겟 개수(N)만큼 값이 연속됨
			if (channel._path == "weights" && keyframe_count > 0)
			{
				// 전체 값 개수 / (프레임 수 * 키프레임당 세트 수) = 모프 타겟 개수
				size_t total_values = sampler.values.size();
				stride = (int)(total_values / (keyframe_count * values_per_keyframe));
			}

			for (size_t k = 0; k < keyframe_count; ++k)
			{
				channel._keyframes[k]._time = sampler.times[k];

				// 현재 키프레임의 데이터 시작 위치
				size_t base_data_index = k * values_per_keyframe * stride;

				auto read_and_convert = [&](size_t offset_index) -> DirectX::XMFLOAT4 {
					DirectX::XMFLOAT4 result(0, 0, 0, 0);

					// Translation, Rotation, Scale 처리
					if (channel._path != "weights")
					{
						float x = sampler.values[offset_index + 0];
						float y = (stride > 1) ? sampler.values[offset_index + 1] : 0.0f;
						float z = (stride > 2) ? sampler.values[offset_index + 2] : 0.0f;
						float w = 0.0f;
						if (stride > 3) w = sampler.values[offset_index + 3];

						if (channel._path == "translation")
						{
							// result = DirectX::XMFLOAT4(x, y, -z, 0.0f); // Z 반전
							result = DirectX::XMFLOAT4(x, y, -z, 0.0f); // Z 반전
						}
						else if (channel._path == "rotation")
						{
							// result = DirectX::XMFLOAT4(-x, -y, z, w); // 회전축 반전
							result = DirectX::XMFLOAT4(-x, -y, z, w); // 회전축 반전
						}
						else if (channel._path == "scale")
						{
							result = DirectX::XMFLOAT4(x, y, z, 0.0f);
						}
					}
					// Weights 처리 (최대 4개 모프 타겟 지원)
					else if (channel._path == "weights")
					{
						float w0 = (stride > 0) ? sampler.values[offset_index + 0] : 0.0f;
						float w1 = (stride > 1) ? sampler.values[offset_index + 1] : 0.0f;
						float w2 = (stride > 2) ? sampler.values[offset_index + 2] : 0.0f;
						float w3 = (stride > 3) ? sampler.values[offset_index + 3] : 0.0f;

						// 모프 타겟 가중치는 좌표계 변환 불필요
						result = DirectX::XMFLOAT4(w0, w1, w2, w3);
					}
					return result;
					};

				if (channel._interpolation == AnimationInterpolation::CubicSpline)
				{
					channel._keyframes[k]._in_tangent = read_and_convert(base_data_index + 0 * stride);
					channel._keyframes[k]._value = read_and_convert(base_data_index + 1 * stride);
					channel._keyframes[k]._out_tangent = read_and_convert(base_data_index + 2 * stride);
				}
				else
				{
					channel._keyframes[k]._value = read_and_convert(base_data_index);
				}
			}

			clip._channels.push_back(channel);
		}
		// 정보 다 채우고 넣기
		_animations.emplace(anim_name, clip);
	}
}

void ReadGLTFMesh::process_skinned_mesh(const json& gltf_json, const std::vector<char>& binary_buffer, const json& mesh, int skin_index)
{
	// 마스터 스켈레톤 매핑 테이블 생성
	std::vector<int> local_to_master_map;
	if (gltf_json.contains("skins") && skin_index < gltf_json["skins"].size())
	{
		const json& current_skin = gltf_json["skins"][skin_index];
		const auto& local_joints = current_skin["joints"];

		local_to_master_map.resize(local_joints.size());

		for (size_t j = 0; j < local_joints.size(); ++j)
		{
			int local_node_idx = local_joints[j].get<int>();
			std::string bone_name = gltf_json["nodes"][local_node_idx].value("name", "unnamed");

			// 파츠의 j번째 뼈가 마스터 스켈레톤(몸통)의 몇 번째 뼈인지 찾아 기록
			local_to_master_map[j] = get_palette_index_by_name(bone_name);
		}
	}

	for (const auto& primitive_json : mesh["primitives"])
	{
		_primitives.emplace_back(std::make_unique<GltfPrimitive>());
		auto& primitive = _primitives.back();

		// 1. 기본 속성 읽기 (Position, Normal, UV, Tangent)
		// get_attribute_data는 내부적으로 Stride를 처리하므로 안전합니다.
		std::vector<XMFLOAT3> positions = get_attribute_data<XMFLOAT3>(gltf_json, binary_buffer, primitive_json["attributes"]["POSITION"]);

		std::vector<XMFLOAT3> normals;
		if (primitive_json["attributes"].contains("NORMAL"))
			normals = get_attribute_data<XMFLOAT3>(gltf_json, binary_buffer, primitive_json["attributes"]["NORMAL"]);

		//Material이 요구하는 UV 채널 확인 후 읽기
			std::vector<XMFLOAT2> texcoords;
		int material_index = primitive_json.value("material", -1);

		// Material이 요구하는 UV 채널 확인
		int required_uv_channel = 0;  // 기본값
		if (material_index >= 0 && material_index < _material_names.size()) {
			auto mat_info = ResourceManager::instance()->get_material_info(_material_names[material_index]);
			if (mat_info) {
				// BaseColor UV 채널 사용 (대부분의 텍스처가 동일한 채널 사용)
				required_uv_channel = mat_info->base_color_uv_channel;
			}
		}

		// 요구되는 UV 채널 읽기
		std::string uv_attr_name = "TEXCOORD_" + std::to_string(required_uv_channel);
		if (primitive_json["attributes"].contains(uv_attr_name)) {
			texcoords = get_attribute_data<XMFLOAT2>(gltf_json, binary_buffer, primitive_json["attributes"][uv_attr_name]);
		}
		else if (required_uv_channel != 0 && primitive_json["attributes"].contains("TEXCOORD_0")) {
			// Fallback: 요구된 채널이 없으면 TEXCOORD_0 사용
			texcoords = get_attribute_data<XMFLOAT2>(gltf_json, binary_buffer, primitive_json["attributes"]["TEXCOORD_0"]);
		}

		if (texcoords.empty()) {
			std::string mesh_name = mesh.contains("name") ? mesh["name"].get<std::string>() : "Unnamed";
			CLOG("Warning: Mesh '" + name() + "', Primitive in mesh '" + mesh_name + "' has no texture coordinates.");
		}

		std::vector<XMFLOAT4> tangents;
		if (primitive_json["attributes"].contains("TANGENT"))
			tangents = get_attribute_data<XMFLOAT4>(gltf_json, binary_buffer, primitive_json["attributes"]["TANGENT"]);

		// 2. 스키닝 속성 읽기 (JOINTS_0, WEIGHTS_0)
		std::vector<XMUINT4> joint_indices_vec;
		std::vector<XMFLOAT4> weights_vec;

		// JOINTS_0 읽기 (u8, u16 대응 + Stride 적용)
		if (primitive_json["attributes"].contains("JOINTS_0"))
		{
			int accessor_idx = primitive_json["attributes"]["JOINTS_0"];
			const json& accessor = gltf_json["accessors"][accessor_idx];
			const json& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<int>()];

			const char* buffer_start = 
				binary_buffer.data() 
				+ buffer_view.value("byteOffset", 0) 
				+ accessor.value("byteOffset", 0);

			int count = accessor["count"];
			int component_type = accessor["componentType"]; // 5121(u8), 5123(u16)

			// 버퍼 뷰의 Stride를 가져오기 (없으면 0)
			int byte_stride = buffer_view.value("byteStride", 0);

			joint_indices_vec.resize(count);

			for (int i = 0; i < count; ++i)
			{
				const char* current_ptr = nullptr;

				if (component_type == 5123) // Unsigned Short (2 byte)
				{
					// Stride가 0이면 요소 크기(2*4=8) 사용, 아니면 Stride 사용
					int step = (byte_stride > 0) ? byte_stride : (sizeof(uint16_t) * 4);

					// i번째 요소의 정확한 메모리 위치 계산
					current_ptr = buffer_start + i * step;

					const uint16_t* ptr = reinterpret_cast<const uint16_t*>(current_ptr);
					joint_indices_vec[i] = XMUINT4(ptr[0], ptr[1], ptr[2], ptr[3]);
				}
				else if (component_type == 5121) // Unsigned Byte (1 byte)
				{
					// Stride가 0이면 요소 크기(1*4=4) 사용
					int step = (byte_stride > 0) ? byte_stride : (sizeof(uint8_t) * 4);

					// i번째 요소의 정확한 메모리 위치 계산
					current_ptr = buffer_start + i * step;

					const uint8_t* ptr = reinterpret_cast<const uint8_t*>(current_ptr);
					joint_indices_vec[i] = XMUINT4(ptr[0], ptr[1], ptr[2], ptr[3]);
				}
			}
		}

		if (primitive_json["attributes"].contains("WEIGHTS_0"))
		{
			int acc_idx = primitive_json["attributes"]["WEIGHTS_0"];
			const json& accessor = gltf_json["accessors"][acc_idx];
			int component_type = accessor["componentType"];

			// [중요] normalized 속성 확인
			bool is_normalized = accessor.value("normalized", false);

			// WEIGHTS_0 읽기
			// [개선] 만약 파일 데이터가 Unsigned Byte(5121)라면? 
			if (component_type == 5121) // Unsigned Byte
			{
				std::vector<XMBYTE4> byte_weights = get_attribute_data<XMBYTE4>(gltf_json, binary_buffer, acc_idx);
				weights_vec.resize(byte_weights.size());

				for (size_t i = 0; i < byte_weights.size(); ++i) {
					if (is_normalized) {
						// normalized=true: GPU가 자동으로 정규화하므로 그대로 사용
						// 하지만 CPU에서 사용하려면 정규화 필요
						weights_vec[i] = XMFLOAT4(
							static_cast<uint8_t>(byte_weights[i].x) / 255.0f,
							static_cast<uint8_t>(byte_weights[i].y) / 255.0f,
							static_cast<uint8_t>(byte_weights[i].z) / 255.0f,
							static_cast<uint8_t>(byte_weights[i].w) / 255.0f
						);
					}
					else {
						// normalized=false: 이미 0~1 범위 (실제로는 거의 없음)
						weights_vec[i] = XMFLOAT4(
							byte_weights[i].x,
							byte_weights[i].y,
							byte_weights[i].z,
							byte_weights[i].w
						);
					}
				}
			}
			else if (component_type == 5123) // Unsigned Short
			{
				const json& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<int>()];
				const char* buffer_start = binary_buffer.data() + buffer_view.value("byteOffset", 0) + accessor.value("byteOffset", 0);
				int count = accessor["count"];
				int byte_stride = buffer_view.value("byteStride", 0);

				weights_vec.resize(count);

				for (int i = 0; i < count; ++i)
				{
					int step = (byte_stride > 0) ? byte_stride : (sizeof(uint16_t) * 4);
					const uint16_t* ptr = reinterpret_cast<const uint16_t*>(buffer_start + i * step);

					if (is_normalized) {
						weights_vec[i] = XMFLOAT4(
							ptr[0] / 65535.0f,
							ptr[1] / 65535.0f,
							ptr[2] / 65535.0f,
							ptr[3] / 65535.0f
						);
					}
					else {
						// 이미 float 범위
						weights_vec[i] = XMFLOAT4(ptr[0], ptr[1], ptr[2], ptr[3]);
					}
				}
			}
			else if (component_type == 5126) // Float
			{
				weights_vec = get_attribute_data<XMFLOAT4>(gltf_json, binary_buffer, primitive_json["attributes"]["WEIGHTS_0"]);
			}

			// 3. 정점 조립 및 좌표계 변환 (Right -> Left Handed)
			primitive->_vertexCount = (UINT)positions.size();
			primitive->_skinned_vertices.resize(primitive->_vertexCount);

			for (size_t i = 0; i < primitive->_vertexCount; ++i)
			{
				auto& v = primitive->_skinned_vertices[i];

				// 위치와 노멀 좌표계 변환 (Z축 반전)
				v._position = XMFLOAT3(positions[i].x, positions[i].y, -positions[i].z);
				v._normal = XMFLOAT3(normals[i].x, normals[i].y, -normals[i].z);
				v._texCoord = XMFLOAT2(texcoords[i].x, texcoords[i].y);

				// 억지로 Tangent를 만들지 않고 glTF 원본 데이터를 사용합니다.
				if (i < tangents.size())
				{
					// DX12(왼손 좌표계)에 맞게 Z축과 Bitangent 방향(W) 부호를 반전시켜 줍니다.
					v._tangent = XMFLOAT4(tangents[i].x, tangents[i].y, -tangents[i].z, -tangents[i].w);
				}
				else
				{
					// glTF 파일 자체에 Tangent 데이터가 아예 없는 예외적인 경우에만 임시로 계산합니다.
					XMVECTOR normal = XMLoadFloat3(&v._normal);
					XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

					if (abs(XMVectorGetY(normal)) > 0.99f) {
						up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
					}

					XMVECTOR tangent = XMVector3Normalize(XMVector3Cross(up, normal));

					XMFLOAT3 tangentF3;
					XMStoreFloat3(&tangentF3, tangent);
					v._tangent = XMFLOAT4(tangentF3.x, tangentF3.y, tangentF3.z, 1.0f);
				}

				// Skinning Data (배열에 값 대입) - 이 아래는 기존과 동일합니다.
				if (i < joint_indices_vec.size()) {
					UINT local_j0 = joint_indices_vec[i].x;
					UINT local_j1 = joint_indices_vec[i].y;
					UINT local_j2 = joint_indices_vec[i].z;
					UINT local_j3 = joint_indices_vec[i].w;

					v._boneIndices[0] = (local_j0 < local_to_master_map.size()) ? local_to_master_map[local_j0] : 0;
					v._boneIndices[1] = (local_j1 < local_to_master_map.size()) ? local_to_master_map[local_j1] : 0;
					v._boneIndices[2] = (local_j2 < local_to_master_map.size()) ? local_to_master_map[local_j2] : 0;
					v._boneIndices[3] = (local_j3 < local_to_master_map.size()) ? local_to_master_map[local_j3] : 0;
				}
				else {
					v._boneIndices[0] = v._boneIndices[1] = v._boneIndices[2] = v._boneIndices[3] = 0;
				}

				if (i < weights_vec.size()) {
					v._boneWeights[0] = weights_vec[i].x;
					v._boneWeights[1] = weights_vec[i].y;
					v._boneWeights[2] = weights_vec[i].z;
					v._boneWeights[3] = weights_vec[i].w;
				}
				else {
					v._boneWeights[0] = v._boneWeights[1] = v._boneWeights[2] = v._boneWeights[3] = 0.0f;
				}

				float sum = v._boneWeights[0] + v._boneWeights[1] + v._boneWeights[2] + v._boneWeights[3];

				if (sum > 0.0f) {
					v._boneWeights[0] /= sum;
					v._boneWeights[1] /= sum;
					v._boneWeights[2] /= sum;
					v._boneWeights[3] /= sum;
				}
				else {
					v._boneWeights[0] = 1.0f;
				}
			}

			// 4. 인덱스 버퍼 처리 (Winding Order Flip 포함)
			if (primitive_json.contains("indices"))
			{
				const json& accessor = gltf_json["accessors"][primitive_json["indices"].get<size_t>()];
				const json& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<size_t>()];
				const char* data_ptr = binary_buffer.data() + buffer_view.value("byteOffset", 0) + accessor.value("byteOffset", 0);

				primitive->_indexCount = accessor["count"];
				primitive->_indices.resize(primitive->_indexCount);

				if (accessor["componentType"] == 5121) { // unsigned byte
					const uint8_t* p_indices = reinterpret_cast<const uint8_t*>(data_ptr);
					for (size_t i = 0; i < primitive->_indexCount; ++i) {
						primitive->_indices[i] = static_cast<UINT>(p_indices[i]);
					}
				}
				else if (accessor["componentType"] == 5123) { // unsigned short
					const uint16_t* p_indices = reinterpret_cast<const uint16_t*>(data_ptr);
					for (size_t i = 0; i < primitive->_indexCount; ++i) {
						primitive->_indices[i] = static_cast<UINT>(p_indices[i]);
					}
				}
				else if (accessor["componentType"] == 5125) { // unsigned int -> 너무 헷갈리는데 이거 나중에 enum으로 바꿔야 하나?
					memcpy(primitive->_indices.data(), data_ptr, primitive->_indexCount * sizeof(UINT));
				}
				for (size_t i = 0; i < primitive->_indexCount; i += 3) {
					std::swap(primitive->_indices[i + 1], primitive->_indices[i + 2]);
				}
			}

			BoundingOrientedBox::CreateFromPoints(primitive->_orientedBoundingBox, primitive->_skinned_vertices.size(), &primitive->_skinned_vertices[0]._position, sizeof(GltfSkinnedVertex));
			primitive->_materialIndex = primitive_json.value("material", -1);
		}
	}
}

int ReadGLTFMesh::get_palette_index_by_name(const std::string& name) const
{
	for (size_t i = 0; i < _skeleton.size(); ++i)
	{
		// 이름이 같으면 GPU 행렬 팔레트의 인덱스 반환
		if (_skeleton[i]._name == name)
		{
			return static_cast<int>(i);
		}
	}
	return 0; // 못 찾으면 루트 뼈대(0번)로 반환한다 -> 보통 이 경우일것
}

void ReadGLTFMesh::load_animation_only(const std::string& file_path, const std::string& want_name)
{
	if (want_name != "null_name" && _animations.contains(want_name)) {
		return;
	}
	json gltf_json;
	std::vector<char> binary_buffer;

	// 1. 파일 로드
	if (!load_gltf_file(file_path, gltf_json, binary_buffer)) {
		CERROR("애니메이션 파일 로딩 실패: " + file_path);
		return;
	}

	// 2. 임시 노드 이름-인덱스 맵 생성 (애니메이션 파일 기준)
	std::unordered_map<int, std::string> temp_index_to_name;
	for (size_t i = 0; i < gltf_json["nodes"].size(); ++i) {
		temp_index_to_name[(int)i] = gltf_json["nodes"][i].value("name", "");
	}

	// 3. 현재 내 캐릭터의 이름-인덱스 맵 생성
	std::unordered_map<std::string, int> my_name_to_index;
	for (const auto& bone : _skeleton) {
		my_name_to_index[bone._name] = bone._node_index;
	}

	// 4. 애니메이션 파싱 시작
	if (!gltf_json.contains("animations"))
		return;

	const json& animations_node = gltf_json["animations"];
	int animation_index = 0;
	for (const auto& anim_node : animations_node) {

		std::string anim_name = anim_node.value("name", "anim_" + std::to_string(animation_index));
		if (want_name != "null_name")
		{
			anim_name = want_name;
		}

		AnimationClip clip;
		clip._name = anim_name;
		clip._duration = 0.0f;

		// ---------------------------------------------------------
		// (A) Samplers 데이터 미리 로드
		// ---------------------------------------------------------
		struct SamplerData {
			std::vector<float> times;
			std::vector<float> values;
			AnimationInterpolation interpolation;
			int component_count; // VEC3=3, VEC4=4
		};
		std::vector<SamplerData> samplers_temp;

		for (const auto& sampler_json : anim_node["samplers"]) {
			SamplerData sData;
			// Input (Time)
			sData.times = get_attribute_data<float>(gltf_json, binary_buffer, sampler_json["input"].get<int>());

			// 애니메이션 길이 갱신
			if (!sData.times.empty()) {
				float max_t = sData.times.back();
				if (max_t > clip._duration) clip._duration = max_t;
			}

			// Interpolation
			sData.interpolation = string_to_interpolation(sampler_json.value("interpolation", "LINEAR"));

			// Output (Values)
			int output_idx = sampler_json["output"].get<int>();
			const json& accessor = gltf_json["accessors"][output_idx];
			std::string type = accessor["type"].get<std::string>();

			// 타입에 따라 데이터 읽기
			if (type == "VEC3") {
				sData.component_count = 3;
				std::vector<XMFLOAT3> vec3s = get_attribute_data<XMFLOAT3>(gltf_json, binary_buffer, output_idx);
				sData.values.resize(vec3s.size() * 3);
				memcpy(sData.values.data(), vec3s.data(), vec3s.size() * sizeof(XMFLOAT3));
			}
			else if (type == "VEC4") {
				sData.component_count = 4;
				std::vector<XMFLOAT4> vec4s = get_attribute_data<XMFLOAT4>(gltf_json, binary_buffer, output_idx);
				sData.values.resize(vec4s.size() * 4);
				memcpy(sData.values.data(), vec4s.data(), vec4s.size() * sizeof(XMFLOAT4));
			}
			else if (type == "SCALAR") {
				sData.component_count = 1;
				sData.values = get_attribute_data<float>(gltf_json, binary_buffer, output_idx);
			}
			samplers_temp.push_back(sData);
		}

		// ---------------------------------------------------------
		// (B) Channels 파싱 및 데이터 매핑
		// ---------------------------------------------------------
		for (const auto& channel_node : anim_node["channels"]) {
			int file_node_idx = channel_node["target"]["node"].get<int>();
			std::string bone_name = temp_index_to_name[file_node_idx];

			// [중요] 이름 불일치 보정 로직 (Stretching 해결의 핵심)
			// 내 메쉬에 해당 이름이 없다면, 접두어 제거나 대체 이름 등을 시도합니다.
			if (my_name_to_index.find(bone_name) == my_name_to_index.end())
			{
				// 1. "Armature" 같은 최상위 노드는 보통 무시
				if (bone_name == "Armature") continue;

				if (bone_name == "Root" || bone_name == "root") {
					// 내 메쉬에 "root"가 있으면 그걸로 매칭
					if (my_name_to_index.count("root")) 
						bone_name = "root";
					// 내 메쉬에 "Root"가 있으면 그걸로 매칭
					else if (my_name_to_index.count("Root")) 
						bone_name = "Root";
					// 둘 다 없고 "Hips"나 "Pelvis"가 있다면 루트 대신 연결 (선택 사항)
					// else if (my_name_to_index.count("Hips")) bone_name = "Hips";
				}

				// 2. 접두어 제거 시도 (예: "mixamorig:Hips" -> "Hips")
				size_t colon_pos = bone_name.find(':');
				if (colon_pos != std::string::npos) {
					std::string suffix = bone_name.substr(colon_pos + 1);
					if (my_name_to_index.count(suffix)) {
						bone_name = suffix;
					}
				}
				// 3. 언더바 접두어 제거 시도 (예: "MagicConstruct_Hips" -> "Hips")
				else {
					size_t underscore_pos = bone_name.find('_');
					if (underscore_pos != std::string::npos) {
						std::string suffix = bone_name.substr(underscore_pos + 1);
						if (my_name_to_index.count(suffix)) {
							bone_name = suffix;
						}
					}
				}
			}

			// 매칭되는 뼈가 있을 때만 처리
			if (my_name_to_index.count(bone_name)) {
				AnimationChannel channel;
				channel._node_index = my_name_to_index[bone_name];
				channel._path = channel_node["target"]["path"].get<std::string>();

				int sampler_idx = channel_node["sampler"].get<int>();
				const SamplerData& sampler = samplers_temp[sampler_idx];
				channel._interpolation = sampler.interpolation;

				size_t keyframe_count = sampler.times.size();

				// CubicSpline은 키프레임당 3개의 데이터 세트(InTangent, Value, OutTangent)를 가짐
				int values_per_key = (channel._interpolation == AnimationInterpolation::CubicSpline) ? 3 : 1;
				int component_cnt = sampler.component_count;

				for (size_t k = 0; k < keyframe_count; ++k) {
					Keyframe kf;
					kf._time = sampler.times[k];

					// 데이터 읽기 시작 위치 (float 단위 인덱스)
					size_t base_idx = k * values_per_key * component_cnt;

					// [람다] 좌표계 변환을 포함하여 값 읽기 (RH -> LH)
					auto read_converted = [&](size_t offset) -> XMFLOAT4 {
						float x = (offset + 0 < sampler.values.size()) ? sampler.values[offset + 0] : 0.0f;
						float y = (offset + 1 < sampler.values.size()) ? sampler.values[offset + 1] : 0.0f;
						float z = (offset + 2 < sampler.values.size()) ? sampler.values[offset + 2] : 0.0f;
						float w = (component_cnt > 3 && offset + 3 < sampler.values.size()) ? sampler.values[offset + 3] : 0.0f;

						if (channel._path == "translation") {
							return XMFLOAT4(x, y, -z, 0.0f); // Z 반전
						}
						else if (channel._path == "rotation") {
							return XMFLOAT4(-x, -y, z, w); // 회전축 반전
						}
						else if (channel._path == "scale") {
							return XMFLOAT4(x, y, z, 0.0f); // 변환 없음
						}
						return XMFLOAT4(x, y, z, w);
						};

					if (channel._interpolation == AnimationInterpolation::CubicSpline) {
						// 순서: In-Tangent -> Value -> Out-Tangent
						kf._in_tangent = read_converted(base_idx + (0 * component_cnt));
						kf._value = read_converted(base_idx + (1 * component_cnt));
						kf._out_tangent = read_converted(base_idx + (2 * component_cnt));
					}
					else {
						kf._value = read_converted(base_idx);
						kf._in_tangent = XMFLOAT4(0, 0, 0, 0);
						kf._out_tangent = XMFLOAT4(0, 0, 0, 0);
					}
					channel._keyframes.push_back(kf);
				}
				clip._channels.push_back(channel);
			}
			else {
				// 디버깅: 매칭 실패한 뼈 이름 출력 (필요 시 주석 해제)
				CLOG("Warning: Bone mismatch in animation load: " << bone_name);
			}
		}
		_animations.emplace(anim_name, clip);
		++animation_index;
	}
}

bool ReadGLTFMesh::has_animation(const std::string& name) const
{
	return _animations.contains(name);
}

std::vector<std::string> ReadGLTFMesh::get_animation_names() const
{
	std::vector<std::string> names;
	for (const auto& pair : _animations) {
		names.push_back(pair.first);
	}
	return names;
}

std::string ReadGLTFMesh::get_parent_bone_name(const std::string& child_name) const
{
	for (size_t i = 0; i < _skeleton.size(); ++i)
	{
		if (_skeleton[i]._name == child_name)
		{
			int p_idx = _skeleton[i]._parent_index;
			if (p_idx >= 0 && p_idx < _skeleton.size())
			{
				return _skeleton[p_idx]._name;
			}
			break;
		}
	}
	return ""; // 루트이거나 부모가 없음
}

int ReadGLTFMesh::get_bone_index_by_name(const std::string& name) const
{
	for (size_t i = 0; i < _skeleton.size(); ++i)
	{
		if (_skeleton[i]._name == name)
		{
			return _skeleton[i]._node_index; // 노드 인덱스 반환
		}
	}
	return -1; // 못 찾음
}

XMFLOAT4X4 ReadGLTFMesh::get_socket_transform(std::string& bone_name) const
{
	int node_index = get_bone_index_by_name(bone_name);

	if (node_index != -1)
	{
		// 해당 뼈(노드)의 현재 모델 좌표계의 변환 행렬 반환
		return _nodes[node_index]._global_transform;
	}

	// 못 찾으면 단위 행렬 반환
	DirectX::XMFLOAT4X4 identity;
	DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
	return identity;
}

std::vector<std::string> ReadGLTFMesh::get_bone_names() const
{
	std::vector<std::string> names;
	for (const auto& bone : _skeleton)
	{
		names.push_back(bone._name);
	}
	return names;
}

float ReadGLTFMesh::get_animation_duration(const std::string& name) const
{
	auto it = _animations.find(name);
	if (it != _animations.end()) {
		return it->second._duration;
	}
	return 0.0f;
}



void ReadGLTFMesh::set_shader_for_all_materials(const std::string& shader_name) 
{
	for (const auto& mat_name : _material_names)
	{
		ResourceManager::instance()->set_shader_for_material(mat_name, shader_name);
	}
}

void ReadGLTFMesh::nodes_inout_set(std::vector<NodeInfo>& nodes)
{
	nodes.clear();
	nodes = _nodes;
}

AnimationInterpolation ReadGLTFMesh::string_to_interpolation(const std::string& str)
{
	if (str == "LINEAR")
	{
		return AnimationInterpolation::Linear;
	}
	if (str == "STEP")
	{
		return AnimationInterpolation::Step;
	}
	if (str == "CUBICSPLINE")
	{
		return AnimationInterpolation::CubicSpline;
	}
	return AnimationInterpolation::Linear;
}

void ReadGLTFMesh::load_nodes(const json& gltf_json)
{
	const auto& nodes_json = gltf_json["nodes"];
	_nodes.resize(nodes_json.size());

	for (size_t i = 0; i < nodes_json.size(); ++i)
	{
		const auto& node_json = nodes_json[i];
		NodeInfo& node_info = _nodes[i];

		// 1. 초기 TRS 설정
		// Translation: Z 반전
		if (node_json.contains("translation")) {
			node_info._translation = {
				node_json["translation"][0].get<float>(),
				node_json["translation"][1].get<float>(),
				-node_json["translation"][2].get<float>()
			};
		}
		else {
			node_info._translation = { 0.0f, 0.0f, 0.0f }; // 누락 시 기본값
		}

		// Rotation: X, Y 반전 (Quaternion)
		if (node_json.contains("rotation")) {
			node_info._rotation = {
				-node_json["rotation"][0].get<float>(),
				-node_json["rotation"][1].get<float>(),
				node_json["rotation"][2].get<float>(),
				node_json["rotation"][3].get<float>()
			};
		}
		else {
			node_info._rotation = { 0.0f, 0.0f, 0.0f, 1.0f }; // 누락 시 기본값 (단위 쿼터니언)
		}

		// Scale: 변환 없음
		if (node_json.contains("scale")) {
			node_info._scale = {
				node_json["scale"][0].get<float>(),
				node_json["scale"][1].get<float>(),
				node_json["scale"][2].get<float>()
			};
		}
		else {
			node_info._scale = { 1.0f, 1.0f, 1.0f }; // 누락 시 기본값
		}

		// 2. 계층 구조 설정 (자식 -> 부모 연결)
		if (node_json.contains("children")) {
			for (const auto& child_idx_json : node_json["children"]) {
				int child_idx = child_idx_json.get<int>();
				node_info._children.push_back(child_idx);

				// 자식 노드에게 부모 인덱스 설정
				_nodes[child_idx]._parent_index = (int)i;
			}
		}

		// 초기 전역 행렬은 단위 행렬로 설정
		XMStoreFloat4x4(&node_info._global_transform, XMMatrixIdentity());
	}
}

void ReadGLTFMesh::update_node_hierarchy(int node_index, const DirectX::XMMATRIX& parent_transform)
{
	NodeInfo& node = _nodes[node_index];

	// Local Transform (Scale * Rotation * Translation)
	XMMATRIX translation_mat = XMMatrixTranslation(node._translation.x, node._translation.y, node._translation.z);
	XMMATRIX rotation_mat = XMMatrixRotationQuaternion(XMLoadFloat4(&node._rotation));
	XMMATRIX scale_mat = XMMatrixScaling(node._scale.x, node._scale.y, node._scale.z);

	XMMATRIX local_transform = scale_mat * rotation_mat * translation_mat;

	// Global Transform (Local * Parent)
	XMMATRIX global_transform = local_transform * parent_transform;
	XMStoreFloat4x4(&node._global_transform, global_transform);

	// 자식 노드들에게 전파
	for (int child_idx : node._children)
	{
		update_node_hierarchy(child_idx, global_transform);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ReadGLTFMesh::load_gltf_file(const std::string& filename, json& outJson, std::vector<char>& outBinBuffer)
{
	namespace fs = std::filesystem;

	std::ifstream gltf_file(filename);
	if (!gltf_file.is_open()) {
		std::cerr << "Error: Failed to open " << filename << std::endl;
		return false;
	}
	try {
		gltf_file >> outJson;
	}
	catch (json::parse_error& e) {
		std::cerr << "JSON parse error: " << e.what() << std::endl;
		return false;
	}
	gltf_file.close();

	if (outJson.contains("buffers") && !outJson["buffers"].empty() && outJson["buffers"][0].contains("uri")) {
		std::string bin_uri = outJson["buffers"][0]["uri"];
		fs::path gltf_path = filename;
		fs::path bin_path = gltf_path.parent_path() / bin_uri;

		std::ifstream bin_file(bin_path, std::ios::binary | std::ios::ate);
		if (!bin_file.is_open()) {
			std::cerr << "Error: Failed to open binary file " << bin_path << std::endl;
			return false;
		}

		std::streamsize size = bin_file.tellg();
		bin_file.seekg(0, std::ios::beg);

		outBinBuffer.resize(size);
		if (!bin_file.read(outBinBuffer.data(), size)) {
			std::cerr << "Error: Failed to read binary data from " << bin_path << std::endl;
			return false;
		}
		bin_file.close();
	}
	else {
		std::cerr << "Error: No buffer URI found in glTF file." << std::endl;
		return false;
	}

	return true;
}

void ReadGLTFMesh::process_node(const json& gltfJson, const std::vector<char>& binaryBuffer, int nodeIndex, const DirectX::XMFLOAT4X4& parentTransform)
{
	const json& node = gltfJson["nodes"][nodeIndex];

	XMMATRIX local_matrix = XMMatrixIdentity();
	if (node.contains("matrix")) {
		float mat[16];
		for (int i = 0; i < 16; ++i) mat[i] = node["matrix"][i].get<float>();
		XMFLOAT4X4 mat4x4 = XMFLOAT4X4(
			mat[0], mat[1], mat[2], mat[3],
			mat[4], mat[5], mat[6], mat[7],
			mat[8], mat[9], mat[10], mat[11],
			mat[12], mat[13], mat[14], mat[15]
		);
		// 행렬이 통째로 있는 경우, Scale(1,1,-1)을 양옆에 곱해 Z축을 반전시킵니다.
		DirectX::XMMATRIX raw_mat = XMLoadFloat4x4(&mat4x4);
		DirectX::XMMATRIX z_flip = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);
		local_matrix = z_flip * raw_mat * z_flip;
	}
	else {
		XMMATRIX translation_matrix = XMMatrixIdentity();
		if (node.contains("translation")) {
			// 이동(Translation) Z축 반전
			translation_matrix = XMMatrixTranslation(
				node["translation"][0].get<float>(),
				node["translation"][1].get<float>(),
				-node["translation"][2].get<float>()
			);
		}

		XMMATRIX rotation_matrix = XMMatrixIdentity();
		if (node.contains("rotation")) {
			// 회전(Rotation) X, Y축 반전 (load_nodes의 Quaternion 반전 방식과 동일)
			rotation_matrix = XMMatrixRotationQuaternion(XMVectorSet(
				-node["rotation"][0].get<float>(),
				-node["rotation"][1].get<float>(),
				node["rotation"][2].get<float>(),
				node["rotation"][3].get<float>()
			));
		}

		XMMATRIX scale_matrix = XMMatrixIdentity();
		if (node.contains("scale")) {
			// 스케일은 그대로 유지
			scale_matrix = XMMatrixScaling(
				node["scale"][0].get<float>(),
				node["scale"][1].get<float>(),
				node["scale"][2].get<float>()
			);
		}
		local_matrix = scale_matrix * rotation_matrix * translation_matrix;
	}

	XMMATRIX world_matrix = local_matrix * XMLoadFloat4x4(&parentTransform);
	XMFLOAT4X4 world_transform;
	XMStoreFloat4x4(&world_transform, world_matrix);

	if (node.contains("mesh")) {
		const json& mesh = gltfJson["meshes"][node["mesh"].get<int>()];
		process_mesh(gltfJson, binaryBuffer, mesh, world_transform);
	}

	if (node.contains("children")) {
		for (const auto& child_index : node["children"]) {
			process_node(gltfJson, binaryBuffer, child_index.get<int>(), world_transform);
		}
	}
}

void ReadGLTFMesh::process_mesh(const json& gltfJson, const std::vector<char>& binaryBuffer, const json& mesh, const DirectX::XMFLOAT4X4& transform)
{
	for (const auto& primitive_json : mesh["primitives"])
	{

		_primitives.emplace_back(std::make_unique<GltfPrimitive>());

		auto& primitive = _primitives.back();


		std::vector<XMFLOAT3> positions = get_attribute_data<XMFLOAT3>(gltfJson, binaryBuffer, primitive_json["attributes"]["POSITION"]);
		std::vector<XMFLOAT3> normals = primitive_json["attributes"].contains("NORMAL") ? get_attribute_data<XMFLOAT3>(gltfJson, binaryBuffer, primitive_json["attributes"]["NORMAL"]) : std::vector<XMFLOAT3>();
		
		// Material이 요구하는 UV 채널 확인 후 읽기
		std::vector<XMFLOAT2> texcoords;
		int material_index = primitive_json.value("material", -1);

		// Material이 요구하는 UV 채널 확인
		int required_uv_channel = 0;  // 기본값
		if (material_index >= 0 && material_index < _material_names.size()) {
			auto mat_info = ResourceManager::instance()->get_material_info(_material_names[material_index]);
			if (mat_info) {
				// BaseColor UV 채널 사용 (대부분의 텍스처가 동일한 채널 사용)
				required_uv_channel = mat_info->base_color_uv_channel;
			}
		}

		// 요구되는 UV 채널 읽기
		std::string uv_attr_name = "TEXCOORD_" + std::to_string(required_uv_channel);
		if (primitive_json["attributes"].contains(uv_attr_name)) {
			texcoords = get_attribute_data<XMFLOAT2>(gltfJson, binaryBuffer, primitive_json["attributes"][uv_attr_name]);
		}
		else if (required_uv_channel != 0 && primitive_json["attributes"].contains("TEXCOORD_0")) {
			// Fallback: 요구된 채널이 없으면 TEXCOORD_0 사용
			texcoords = get_attribute_data<XMFLOAT2>(gltfJson, binaryBuffer, primitive_json["attributes"]["TEXCOORD_0"]);
		}

		if (texcoords.empty()) {
			std::string mesh_name = mesh.contains("name") ? mesh["name"].get<std::string>() : "Unnamed";
		}

		if (texcoords.empty()) {
			std::string mesh_name = mesh.contains("name") ? mesh["name"].get<std::string>() : "Unnamed";
		}
		std::vector<XMFLOAT4> tangents = primitive_json["attributes"].contains("TANGENT") ? get_attribute_data<XMFLOAT4>(gltfJson, binaryBuffer, primitive_json["attributes"]["TANGENT"]) : std::vector<XMFLOAT4>();
		
		


		// 3. 정점 조립 및 좌표계 변환 (Right -> Left Handed)
		primitive->_vertexCount = (UINT)positions.size();
		primitive->_vertices.resize(primitive->_vertexCount);

		XMMATRIX world_mat = XMLoadFloat4x4(&transform);

		// 노멀 변환을 위한 역전치 행렬 (스케일 값 때문에 노멀이 왜곡되는 것을 방지)
		XMMATRIX inverse_transpose_mat = XMMatrixTranspose(XMMatrixInverse(nullptr, world_mat));

		for (size_t i = 0; i < primitive->_vertexCount; ++i)
		{
			auto& v = primitive->_vertices[i];

			XMFLOAT3 n = (i < normals.size()) ? normals[i] : XMFLOAT3(0.0f, 1.0f, 0.0f);
			XMFLOAT2 t = (i < texcoords.size()) ? texcoords[i] : XMFLOAT2(0.0f, 0.0f);

			// 1. 기본 왼손 좌표계(LH) 변환 (대원님이 복붙하신 로직)
			v._position = XMFLOAT3(positions[i].x, positions[i].y, -positions[i].z);
			v._normal = XMFLOAT3(normals[i].x, normals[i].y, -normals[i].z);
			v._texCoord = XMFLOAT2(texcoords[i].x, texcoords[i].y);

			if (i < tangents.size()) {
				v._tangent = XMFLOAT4(tangents[i].x, tangents[i].y, -tangents[i].z, -tangents[i].w);
			}
			else {
				// 임시 탄젠트 계산
				XMVECTOR normal = XMLoadFloat3(&v._normal);
				XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				if (abs(XMVectorGetY(normal)) > 0.99f) up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
				XMVECTOR tangent = XMVector3Normalize(XMVector3Cross(up, normal));
				XMFLOAT3 tangentF3;
				XMStoreFloat3(&tangentF3, tangent);
				v._tangent = XMFLOAT4(tangentF3.x, tangentF3.y, tangentF3.z, 1.0f);
			}

			// 2. 노드의 월드 변환 행렬을 정점에 직접 곱하기 (Baking)

			// 위치(Position) 변환
			XMVECTOR pos = XMLoadFloat3(&v._position);
			pos = XMVector3TransformCoord(pos, world_mat);
			XMStoreFloat3(&v._position, pos);

			// 노멀(Normal) 변환
			XMVECTOR normal = XMLoadFloat3(&v._normal);
			normal = XMVector3TransformNormal(normal, inverse_transpose_mat);
			normal = XMVector3Normalize(normal); // 변환 후 정규화 필수
			XMStoreFloat3(&v._normal, normal);

			// 탄젠트(Tangent) 변환
			XMVECTOR tangent = XMLoadFloat4(&v._tangent);
			XMVECTOR tangent_dir = XMVector3TransformNormal(tangent, world_mat);
			tangent_dir = XMVector3Normalize(tangent_dir);

			// W값(Handedness)은 그대로 유지하고 방향(XYZ)만 저장
			XMStoreFloat4(&v._tangent, XMVectorSet(
				XMVectorGetX(tangent_dir),
				XMVectorGetY(tangent_dir),
				XMVectorGetZ(tangent_dir),
				v._tangent.w
			));
		}

		if (primitive_json.contains("indices"))
		{
			const json& accessor = gltfJson["accessors"][primitive_json["indices"].get<size_t>()];
			const json& buffer_view = gltfJson["bufferViews"][accessor["bufferView"].get<size_t>()];
			const char* data_ptr = binaryBuffer.data() + buffer_view.value("byteOffset", 0) + accessor.value("byteOffset", 0);

			primitive->_indexCount = accessor["count"];
			primitive->_indices.resize(primitive->_indexCount);

			if (accessor["componentType"] == 5121) { // unsigned byte
				const uint8_t* p_indices = reinterpret_cast<const uint8_t*>(data_ptr);
				for (size_t i = 0; i < primitive->_indexCount; ++i) {
					primitive->_indices[i] = static_cast<UINT>(p_indices[i]);
				}
			}
			else if (accessor["componentType"] == 5123) { // unsigned short
				const uint16_t* p_indices = reinterpret_cast<const uint16_t*>(data_ptr);
				for (size_t i = 0; i < primitive->_indexCount; ++i) {
					primitive->_indices[i] = static_cast<UINT>(p_indices[i]);
				}
			}
			else if (accessor["componentType"] == 5125) { // unsigned int -> 너무 헷갈리는데 이거 나중에 enum으로 바꿔야 하나?
				memcpy(primitive->_indices.data(), data_ptr, primitive->_indexCount * sizeof(UINT));
			}
			for (size_t i = 0; i < primitive->_indexCount; i += 3) {
				std::swap(primitive->_indices[i + 1], primitive->_indices[i + 2]);
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		

		//for (size_t i = 0; i < primitive->_vertexCount; ++i) {
		//	XMVECTOR pos = XMLoadFloat3(&positions[i]);
		//	pos = XMVector3Transform(pos, world_mat);
		//	XMStoreFloat3(&primitive->_vertices[i]._position, pos);

		//	primitive->_vertices[i]._normal = (i < normals.size()) ? normals[i] : XMFLOAT3(0.0f, 1.0f, 0.0f);
		//	primitive->_vertices[i]._texCoord = (i < texcoords.size()) ? texcoords[i] : XMFLOAT2(0.0f, 0.0f);
		//	primitive->_vertices[i]._tangent = (i < tangents.size()) ? tangents[i] : XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		//}

		//if (primitive_json.contains("indices")) {
		//	const json& accessor = gltfJson["accessors"][primitive_json["indices"].get<size_t>()];
		//	const json& bufferView = gltfJson["bufferViews"][accessor["bufferView"].get<size_t>()];
		//	const char* data_ptr = binaryBuffer.data() + bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
		//	primitive->_indexCount = accessor["count"];
		//	primitive->_indices.resize(primitive->_indexCount);

		//	if (accessor["componentType"] == 5121) { // Unsigned Byte
		//		const uint8_t* p_indices = reinterpret_cast<const uint8_t*>(data_ptr);
		//		for (size_t i = 0; i < primitive->_indexCount; ++i) {
		//			primitive->_indices[i] = static_cast<UINT>(p_indices[i]);
		//		}
		//	}
		//	else if (accessor["componentType"] == 5123) {
		//		const uint16_t* p_indices = reinterpret_cast<const uint16_t*>(data_ptr);
		//		for (size_t i = 0; i < primitive->_indexCount; ++i) {
		//			primitive->_indices[i] = static_cast<UINT>(p_indices[i]);
		//		}
		//	}
		//	else if (accessor["componentType"] == 5125) {
		//		memcpy(primitive->_indices.data(), data_ptr, primitive->_indexCount * sizeof(UINT));
		//	}
		//}

		BoundingOrientedBox::CreateFromPoints(primitive->_orientedBoundingBox, primitive->_vertices.size(), &primitive->_vertices[0]._position, sizeof(GltfVertex));

		primitive->_materialIndex = primitive_json.value("material", -1);

	}
}

void ReadGLTFMesh::bounding_box_merge()
{
	if (!_primitives.empty())
	{
		std::vector<XMFLOAT3> all_points;
		all_points.reserve(_primitives.size() * 8);

		// 여기서는 모든 점을 수집해야 하므로 0부터 시작합니다.
		for (size_t i = 0; i < _primitives.size(); ++i)
		{
			auto& obb = _primitives[i]->_orientedBoundingBox;
			if (std::isnan(obb.Center.x) || std::isnan(obb.Extents.x) || std::isnan(obb.Orientation.x))
			{
				// 깨진 데이터는 무시해버림
				continue;
			}

			std::array<XMFLOAT3, 8> corners;
			_primitives[i]->_orientedBoundingBox.GetCorners(corners.data());
			all_points.insert(all_points.end(), corners.begin(), corners.end());
		}

		// 수집된 전체 점들을 바탕으로 단 한 번만 최종 OBB를 계산합니다.
		BoundingOrientedBox::CreateFromPoints(_orientedBoundingBox, all_points.size(), all_points.data(), sizeof(XMFLOAT3));
	}
}

bool ReadGLTFMesh::intersects_ray(const XMVECTOR& rayStart, const XMVECTOR& rayDir, const XMMATRIX& worldMatrix, float& outHitDist, float maxHitDist) const
{
	bool hitAnything = false;
	float closestDist = FLT_MAX;

	// 전체 합본 박스를 먼저 검사해서, 아예 근처도 안 갔으면 빠르게 패스
	BoundingOrientedBox worldOverallOBB;
	_orientedBoundingBox.Transform(worldOverallOBB, worldMatrix);

	float overallDist = 0.0f;
	if (!worldOverallOBB.Intersects(rayStart, rayDir, overallDist))
	{
		return false; // 전체 박스에 안 맞았으면 하위 프리미티브는 볼 필요도 없음
	}
	if (overallDist >= maxHitDist)
	{
		return false; 
	}

	// 전체 박스에 맞았다면, 디테일한 개별 프리미티브 OBB들을 순회하며 진짜 맞았는지 검사
	for (const auto& primitive : _primitives)
	{
		// nan 방어코드 추가
		if (std::isnan(primitive->_orientedBoundingBox.Center.x)
			|| std::isnan(primitive->_orientedBoundingBox.Extents.x)
			|| std::isnan(primitive->_orientedBoundingBox.Orientation.x))
		{
			// 깨진 데이터는 무시해버림
			continue;
		}

		BoundingOrientedBox worldPrimOBB;
		primitive->_orientedBoundingBox.Transform(worldPrimOBB, worldMatrix);

		float primHitDist = 0.0f;
		if (worldPrimOBB.Intersects(rayStart, rayDir, primHitDist))
		{
			if (primHitDist >= maxHitDist)
			{
				continue;
			}
			// OBB에 맞았거나 내부에 있다면, 실제 삼각형(Triangle)들과 정밀 교차 판정 수행
			size_t indexCount = primitive->_indices.size();
			if (indexCount % 3 != 0) continue; // 정상적인 삼각형 데이터가 아님

			bool isSkinned = !primitive->_skinned_vertices.empty();
			
			for (size_t i = 0; i < indexCount; i += 3)
			{
				UINT i0 = primitive->_indices[i];
				UINT i1 = primitive->_indices[i + 1];
				UINT i2 = primitive->_indices[i + 2];

				XMVECTOR v0, v1, v2;
				if (isSkinned)
				{
					v0 = XMLoadFloat3(&primitive->_skinned_vertices[i0]._position);
					v1 = XMLoadFloat3(&primitive->_skinned_vertices[i1]._position);
					v2 = XMLoadFloat3(&primitive->_skinned_vertices[i2]._position);
				}
				else
				{
					v0 = XMLoadFloat3(&primitive->_vertices[i0]._position);
					v1 = XMLoadFloat3(&primitive->_vertices[i1]._position);
					v2 = XMLoadFloat3(&primitive->_vertices[i2]._position);
				}

				// 정점들을 월드 좌표계로 변환
				v0 = XMVector3TransformCoord(v0, worldMatrix);
				v1 = XMVector3TransformCoord(v1, worldMatrix);
				v2 = XMVector3TransformCoord(v2, worldMatrix);

				float triHitDist = 0.0f;
				// 실제 삼각형 폴리곤과 광선의 교차 판정
				if (DirectX::TriangleTests::Intersects(rayStart, rayDir, v0, v1, v2, triHitDist))
				{
					if (triHitDist >= 0.0f && triHitDist < closestDist)
					{
						closestDist = triHitDist;
						hitAnything = true;
					}
				}
			}
		}
	}

	if (hitAnything)
	{
		outHitDist = closestDist;
		return true;
	}

	return false;
}