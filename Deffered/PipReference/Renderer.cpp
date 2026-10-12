#include "stdafx.h"

#include "Renderer.h"

#include "Shader.h"
#include "DebugShader.h"
#include "DefaultObjectShader.h"
#include "GlbShader.h"
#include "GltfShader.h"
#include "PlayerShader.h"
#include "BillboardUIShader.h"
#include "SkyboxShader.h"
#include "GltfSkinnedShader.h"
#include "TerrainShader.h"
#include "UIShader.h"
#include "MonsterHPUIShader.h"
#include "ShadowDepthShader.h"
#include "ShadowDepthSkinnedShader.h"
#include "UIFrameShader.h"
#include "UIManager.h"

#include "GameObject.h"
#include "ObjectManager.h"
#include "AnimationComponent.h"
#include "FreeCameraScript.h"

#include "Camera.h"
#include "CameraComponent.h"
#include "DebugDrawManager.h"
#include "GameFramework.h"
#include "InstancedRenderComponent.h"
#include "InstanceglTFShader.h"
#include "LightManager.h"
#include "RenderComponent.h"
#include "OcclusionManager.h"
#include "OcclusionQueryShader.h"
#include "ParticleShader.h"
#include "ParticleSystemComponent.h"
#include "ParticleRenderComponent.h"


#include "TerrainLoader.h"
#include "ResourceManager.h"
#include "ShadowManager.h"
#include "MinimapShader.h"
#include "FoliageRenderComponent.h"

void Renderer::initialize(ID3D12Device* device)
{
	_device = device;
	
	_descriptor_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 
	_unitCube = Mesh::create_unit_cube();
	OcclusionManager::instance()->initialize(device, 20'000);

	create_dynamic_descriptor_heap(1000000);

	// [추가] 사용할 루트 시그니처 생성기들을 등록합니다.
	_rootSignatureGenerators.push_back(std::make_unique<DebugRootSignatureGenerator>()); // 추가
	_rootSignatureGenerators.push_back(std::make_unique<DefaultRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<GltfRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<SkyBoxRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<SkinnedRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<TerrainRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<MonsterHPUIRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<UIRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<CsmDepthRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<CsmDepthSkinnedRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<UIFrameRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<MinimapRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<OcclusionRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<ComputeParticleRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<ParticleRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<BillboardUIRootSignatureGenerator>());
	// 새 루트 시그니처가 필요하면 여기에 생성기만 추가하면 끝입니다.

	// [추가] PSO를 생성할 셰이더 프로토타입들을 등록합니다.
	// [변경] 셰이더 프로토타입을 등록할 때, 셰이더의 이름을 키로 하여 map에 저장합니다.
	auto default_shader = std::make_shared<DefaultObjectShader>();
	_shaderPrototypes[default_shader->pso_name()] = default_shader;

	auto player_shader = std::make_shared<PlayerShader>();
	_shaderPrototypes[player_shader->pso_name()] = player_shader;

	auto debug_shader = std::make_shared<DebugShader>();
	_shaderPrototypes[debug_shader->pso_name()] = debug_shader;

	auto glb_shader = std::make_shared<GlbShader>();
	_shaderPrototypes[glb_shader->pso_name()] = glb_shader;

	auto gltf_shader = std::make_shared<GltfShader>();
	_shaderPrototypes[gltf_shader->pso_name()] = gltf_shader;

	auto skybox_shader = std::make_shared<SkyboxShader>();
	_shaderPrototypes[skybox_shader->pso_name()] = skybox_shader;

	auto gltf_animation_shader = std::make_shared<GltfSkinnedShader>();
	_shaderPrototypes[gltf_animation_shader->pso_name()] = gltf_animation_shader;

	auto terrain_shader = std::make_shared<TerrainShader>();
	_shaderPrototypes[terrain_shader->pso_name()] = terrain_shader;

	auto monster_hp_ui_shader = std::make_shared<MonsterHPUIShader>();
	_shaderPrototypes[monster_hp_ui_shader->pso_name()] = monster_hp_ui_shader;

	auto ui_shader = std::make_shared<UIShader>();
	_shaderPrototypes[ui_shader->pso_name()] = ui_shader;

	auto shadow_depth_shader = std::make_shared<ShadowDepthShader>();
	_shaderPrototypes[shadow_depth_shader->pso_name()] = shadow_depth_shader;

	auto shadow_depth_skinned_shader = std::make_shared<ShadowDepthSkinnedShader>();
	_shaderPrototypes[shadow_depth_skinned_shader->pso_name()] = shadow_depth_skinned_shader;

	auto ui_frame_shader = std::make_shared<UIFrameShader>();
	_shaderPrototypes[ui_frame_shader->pso_name()] = ui_frame_shader;

	auto minimap_shader = std::make_shared<MinimapShader>();
	_shaderPrototypes[minimap_shader->pso_name()] = minimap_shader;

	auto occlusion_shader = std::make_shared<OcclusionQueryShader>();
	_shaderPrototypes[occlusion_shader->pso_name()] = occlusion_shader;

	auto particle_shader = std::make_shared<ParticleShader>();
	_shaderPrototypes[particle_shader->pso_name()] = particle_shader;

	auto instanced_gltf = std::make_shared<InstancedglTFShader>();
	_shaderPrototypes[instanced_gltf->pso_name()] = instanced_gltf;

	auto billboardUI = std::make_shared<BillboardUIShader>();
	_shaderPrototypes[billboardUI->pso_name()] = billboardUI;

	create_root_signatures(device);
	create_pipeline_state_objects(device);
}
void Renderer::create_root_signatures(ID3D12Device* device)
{
	_rootSignatures.clear();
	for (const auto& generator : _rootSignatureGenerators)
	{
		_rootSignatures[generator->name()] = generator->create(device);
	}
}   

void Renderer::create_pipeline_state_objects(ID3D12Device* device)
{
	_pipelineStates.clear(); // 기존 PSO 맵 비우기

	// 등록된 모든 셰이더 프로토타입을 순회합니다.
	for (const auto& [shader_name, shader_prototype] : _shaderPrototypes)
	{
		// 1. 셰이더가 요구하는 루트 시그니처를 가져옵니다.
		ID3D12RootSignature* root_signature =
			get_root_signature(shader_prototype->required_root_signature());
		if (!root_signature) continue;

		// 2. 셰이더 프로토타입에게 PSO 생성을 위임합니다.
		ComPtr<ID3D12PipelineState> pso = shader_prototype->create_pso(device, root_signature);

		// 3. 반환된 PSO를 맵에 저장합니다. 이름은 셰이더가 직접 알려줍니다.
		if (pso)
		{
			_pipelineStates[shader_prototype->pso_name()] = pso;
		}
	}
}

void Renderer::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
	// 프레임 렌더링 시작 시, 동적 디스크립터 힙의 인덱스를 리셋
	_current_dynamic_descriptor_index = frame_index * _max_descriptors_per_frame;

	CameraComponent* camera = CameraComponent::get_main();
	if (!camera)
	{
		// 렌더링할 카메라가 없으면 아무것도 하지 않습니다.
		// CERROR("렌더시에 카메라 1개이상은 필요함")
		CERROR("No main camera found for rendering. Skipping frame.");
		return;
	}

	// [최적화] 매 프레임 전체 객체 중 일부(400개)만 인덱스 회수 검사
	static size_t cleanupCursor = 0;
	const auto& allObjects = ObjectManager::instance()->get_all_game_objects();
	if (!allObjects.empty()) {
		size_t checkCount = std::min<size_t>(400, allObjects.size());
		f3 camPos = camera->game_object()->transform()->get_world_position();

		for (size_t i = 0; i < checkCount; ++i) {
			auto& obj = allObjects[cleanupCursor % allObjects.size()];
			cleanupCursor++;

			if (!obj || obj->is_destroyed()) continue;

			auto rc = obj->get_component<RenderComponent>();
			if (rc && rc->has_allocated_index()) { // 이미 할당된 인덱스가 있는 경우만
				float dist = Vector3::Length(Vector3::Subtract(camPos, obj->transform()->get_world_position()));
				if (dist > 300.0f) { // 300m 이상 멀어지면 회수
					OcclusionManager::instance()->release_query_index(rc->get_occlusion_query_index());
					rc->set_occlusion_query_index(0xFFFFFFFF);
				}
			}
		}
	}

	// 1. 이번 프레임에 그릴 객체들을 추려낸다.
	build_render_list(camera);

	// 2. 추려낸 목록을 바탕으로 실제 그리기를 수행한다.
	draw_render_list(commandList, camera, frame_index);
	//draw_render_occlusion_culling_list(commandList, camera,  frame_index);

#ifdef _DEBUG_PHYSICS_VISUALIZATION
	// [수정] viewProj가 아니라 frame_index를 넘겨야 합니다!
	DebugDrawManager::instance()->Render(commandList, frame_index);
#endif
}

void Renderer::register_static_object(const std::shared_ptr<GameObject>& obj)
{
	auto renderComp = obj->get_component<RenderComponent>();
	if (!renderComp) return;

	renderComp->mark_as_static();
	_staticRenderList[renderComp->pso_name()].push_back(obj);
}

void Renderer::clear_static_render_list()
{
	_staticRenderList.clear();
	_staticGltfInstanceGroups.clear();
	_staticListBuilt = false;
}

void Renderer::build_render_list(const CameraComponent* camera)
{
	// 1. 기존 맵 비우기
	_renderMap.clear();
	_gltfInstanceGroups.clear();
	_gltfShadowInstanceGroups.clear();
	_gltfDynamicShadowInstanceGroups.clear();
	_shadowRenderMap.clear();

	const auto& allGameObjects = ObjectManager::instance()->get_all_game_objects();
	const BoundingFrustum& frustum = camera->frustum();

	// 카메라 정보 미리 획득
	f3 camPos = camera->game_object()->transform()->get_world_position();
	f3 camForward = camera->game_object()->transform()->forward();

	bool isCinematic = false;
	if (camera && camera->game_object()) {
		auto freeCam = camera->game_object()->get_component<FreeCameraScript>();
		if (freeCam) {
			isCinematic = freeCam->is_sinamatic_camera_mode();
		}
	}

	auto process_object = [&](const std::shared_ptr<GameObject>& gameObject, const std::shared_ptr<RenderComponent>& renderComp) {
		const std::string& psoName = renderComp->pso_name();

		// 시네마틱 모드 필터링: 플레이어, 다른 플레이어, 몬스터/NPC 제외
		if (isCinematic) {
			if (gameObject->is_in_layer("Player") || 
				gameObject->is_in_layer("OtherPlayer") || 
				gameObject->is_in_layer("Enemy")) 
			{
				return;
			}
		}

		// UI, Skybox 등 특수 객체 처리
		if (psoName == "ui" || psoName == "Monster_HP_UI" || psoName == "ui_frame" ||
			psoName == "skybox" || psoName == "particle_draw" || psoName == "billboard_ui")
		{
			// 시네마틱 모드일 경우 일반 UI 객체 제외 (단, 이름에 "Cinematic" 포함 시 허용)
			if (isCinematic && (psoName == "ui" || psoName == "Monster_HP_UI" || psoName == "ui_frame" || psoName == "billboard_ui")) {
				std::string objName = gameObject->name();
				if (objName.find("Cinematic") == std::string::npos && objName.find("Cutscene") == std::string::npos) {
					return;
				}
			}

			if (psoName != "ui") _renderMap[psoName].push_back(gameObject);
			return;
		}

		// --- 위치 연산 (한 번만 수행) ---
		f3 objPos = gameObject->transform()->get_world_position();
		f3 toObj = Vector3::Subtract(objPos, camPos);
		float distSq = toObj.x * toObj.x + toObj.y * toObj.y + toObj.z * toObj.z; // 직접 제곱 계산 (가장 빠름)

		// --- 1. 일반 렌더링 리스트 빌드 (View Frustum Culling) ---
		float renderLimit = (psoName == "terrain") ? 700.0f : 500.0f;
		if (psoName == "gltf_instanced") renderLimit = 3000.0f;

		if (renderComp->culling_distance() >= 0.0f) renderLimit = renderComp->culling_distance();

		if (distSq < (renderLimit * renderLimit))
		{
			bool isVisible = renderComp->is_visible(frustum);
			if (psoName == "gltf_instanced") isVisible = true;

			if (isVisible)
			{
				if (psoName == "gltf") {
					_gltfInstanceGroups[renderComp->mesh()].push_back(gameObject);
				}
				else {
					_renderMap[psoName].push_back(gameObject);
				}
			}
		}

		// --- 2. 그림자 렌더링 리스트 빌드 (전후방 차등 거리 컬링) ---
		// 그림자 생성에 부적합한 객체 제외
		if (psoName == "terrain" || psoName == "gltf" || psoName == "skinned")
		{
			float dist = sqrtf(distSq);
			f3 dirToObj = { toObj.x / dist, toObj.y / dist, toObj.z / dist };
			float dot = Vector3::DotProduct(camForward, dirToObj);

			// 1. dot 값을 0~1 범위로 정규화 (보간을 위해)
			// dot이 1.0(정면)이면 t = 1.0
			// dot이 -1.0(후방)이면 t = 0.0
			float t = (dot + 1.0f) * 0.5f;

			// 2. 최소 거리(100)와 최대 거리 사이를 부드럽게 보간
			float maxShadowDist = ShadowManager::instance()->get_shadow_max_distance();
			float shadowLimit = 100.0f + (t * (maxShadowDist - 100.0f));

			// 3. 평면 거리(XZ) 계산
			float distXZ = sqrtf(toObj.x * toObj.x + toObj.z * toObj.z);

			// 4. 보간된 한계값으로 판정
			if (distXZ < shadowLimit)
			{
				if (psoName == "gltf") {
					if (renderComp->is_static()) {
						_gltfShadowInstanceGroups[renderComp->mesh()].push_back(gameObject);
					} else {
						_gltfDynamicShadowInstanceGroups[renderComp->mesh()].push_back(gameObject);
					}
				}
				else {
					_shadowRenderMap[psoName].push_back(gameObject);
				}
			}
		}
	};

	// STEP A: Static 오브젝트 (미리 구축된 리스트)
	for (auto& pair : _staticRenderList)
	{
		for (auto& gameObject : pair.second)
		{
			if (!gameObject || !gameObject->is_enable() || gameObject->is_destroyed()) continue;
			auto renderComp = gameObject->get_component<RenderComponent>();
			if (!renderComp || !renderComp->is_enabled()) continue;
			process_object(gameObject, renderComp);
		}
	}

	// STEP B: Dynamic 오브젝트 (전체 목록에서 Static 제외하고 순회)
	for (const auto& gameObject : allGameObjects)
	{
		if (!gameObject || !gameObject->is_enable() || gameObject->is_destroyed()) continue;
		auto renderComp = gameObject->get_component<RenderComponent>();
		if (!renderComp || !renderComp->is_enabled()) continue;

		if (renderComp->is_static()) continue;

		process_object(gameObject, renderComp);
	}

	// UI 추가 로직 (기존 유지)
	UIManager::instance()->set_render_vector();
	const auto& render_vec = UIManager::instance()->ui_render_vector();
	for (const auto& vec : render_vec)
	{
		for (const auto& gameObject : vec)
		{
			if (!gameObject || !gameObject->is_enable() || gameObject->is_destroyed()) continue;
			auto rc = gameObject->get_component<RenderComponent>();
			if (rc) {
				// 시네마틱 모드 UI 필터링 (ui 레이어 객체들도 필터링)
				if (isCinematic) {
					std::string psoName = rc->pso_name();
					if (psoName == "ui" || psoName == "ui_frame" || psoName == "Monster_HP_UI" || psoName == "billboard_ui") {
						std::string objName = gameObject->name();
						if (objName.find("Cinematic") == std::string::npos && objName.find("Cutscene") == std::string::npos) {
							continue;
						}
					}
				}
				_renderMap[rc->pso_name()].push_back(gameObject);
			}
		}
	}
}

void Renderer::draw_render_list(ID3D12GraphicsCommandList* commandList, CameraComponent* camera, UINT frame_index)
{
	_totalRenderCount = 0;
	_totalDrawCalls = 0;

	// 동적 디스크립터 힙 설정
	ID3D12DescriptorHeap* heaps[] = { _dynamic_descriptor_heap.Get() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	// 렌더링 순서 정의
	std::vector<std::string> render_order = {
		"terrain",      // 지형
		"gltf",         // 일반 메시 (GPU Instancing 적용)
		"gltf_instanced", // 인스턴싱이 적용된 glTF 메시
		"skinned",      // 애니메이션 메시
		"skybox",       // Skybox
		"particle_draw",// 파티클
		"Monster_HP_UI",// 몬스터 HP UI
		"ui_frame",     // UI Frame
		"billboard_ui",    // Billboard UI
		"ui"            // UI
	};

	for (const auto& psoName : render_order)
	{
		// ==========================================================
		// 1. gltf PSO 특수 처리 (GPU Instancing)
		// ==========================================================
		if (psoName == "gltf")
		{
			if (_gltfInstanceGroups.empty()) continue;

			ID3D12PipelineState* pso = get_pso("gltf");
			ID3D12RootSignature* rootSig = get_root_signature("gltf");
			auto proto_it = _shaderPrototypes.find("gltf");

			if (!pso || !rootSig || proto_it == _shaderPrototypes.end()) continue;

			// PSO 및 루트 시그니처 바인딩
			commandList->SetPipelineState(pso);
			commandList->SetGraphicsRootSignature(rootSig);

			// 공통 변수 바인딩 (카메라, 조명, 그림자)
			if (camera) {
				camera->update_shader_variables(commandList, frame_index);
				camera->set_viewports_and_scissor_rects(commandList);
			}
			LightManager::instance()->bind(commandList, 3);
			ShadowManager::instance()->bind_for_lighting(commandList, 10, 11, this);

			// 동일 메시 그룹별로 렌더링
			for (auto& pair : _gltfInstanceGroups)
			{
				auto mesh = pair.first;
				auto& instances = pair.second;
				if (instances.empty()) continue;

				UINT instanceCount = static_cast<UINT>(instances.size());

				// 1) 모든 인스턴스의 월드 행렬 수집
				std::vector<XMMATRIX> worldMatrices;
				worldMatrices.reserve(instanceCount);
				for (auto& obj : instances) {
					worldMatrices.push_back(XMMatrixTranspose(XMLoadFloat4x4(&obj->transform()->world_matrix())));
				}

				// 2) LinearAllocator를 통해 GPU 업로드
				auto alloc = GameFramework::instance()->linear_allocator()->allocate(sizeof(XMMATRIX) * instanceCount);
				memcpy(alloc.cpuPtr, worldMatrices.data(), sizeof(XMMATRIX) * instanceCount);

				// 3) 인스턴스 행렬 버퍼 바인딩 (Root RS의 12번 파라미터 == t12)
				commandList->SetGraphicsRootShaderResourceView(12, alloc.gpuAddr);

				// 4) 재질 정보 업데이트 (그룹 내 첫 번째 객체 기준)
				auto firstObj = instances[0];
				auto renderComp = firstObj->get_component<RenderComponent>();
				if (renderComp) {
					// 1. 상수 버퍼 업데이트 (여기서 otherplayer_id가 -1로 설정됨)
					renderComp->update_world_matrix_cb(frame_index);
					// 2. Root Parameter 0번에 해당 버퍼 바인딩
					commandList->SetGraphicsRootConstantBufferView(0, renderComp->get_cb_gpu_address(frame_index));
				}
				proto_it->second->update_per_object(commandList, this, firstObj.get());
				firstObj->prepare_render();

				// 실제 드로우 콜 & 인스턴싱 객체 카운트
				_totalRenderCount += instanceCount; // 실제 객체 수 (예: 나무 100개)
				_totalDrawCalls += 1;               // 드로우 콜은 단 1번!

				// 5) 인스턴싱 드로우 호출
				mesh->render_instance(commandList, instanceCount);
			}
			continue; // gltf 처리 완료, 다음 PSO로
		}

		// ==========================================================
		// 2. 나머지 PSO 처리 (기존 로직 유지)
		// ==========================================================
		auto it = _renderMap.find(psoName);
		if (it == _renderMap.end() || it->second.empty()) continue;

		const auto& gameObjects = it->second;

		ID3D12PipelineState* pso = get_pso(psoName);
		auto proto_it = _shaderPrototypes.find(psoName);
		if (!pso || proto_it == _shaderPrototypes.end()) continue;

		const auto& shader_prototype = proto_it->second;
		ID3D12RootSignature* root_signature = get_root_signature(shader_prototype->required_root_signature());
		if (!root_signature) continue;

		commandList->SetPipelineState(pso);
		commandList->SetGraphicsRootSignature(root_signature);

		// 조명 및 그림자 바인딩
		if (psoName == "skinned" || psoName == "terrain") {
			LightManager::instance()->bind(commandList, 3);
			if (psoName == "terrain")
				ShadowManager::instance()->bind_for_lighting(commandList, 6, 7, this);
			else
				ShadowManager::instance()->bind_for_lighting(commandList, 10, 11, this);
		}

		if (camera) {
			camera->update_shader_variables(commandList, frame_index);
			camera->set_viewports_and_scissor_rects(commandList);
		}

		// Skybox 특수 처리
		if (psoName == "skybox") {
			if (camera && camera->get_cb_skybox())
				commandList->SetGraphicsRootConstantBufferView(2, camera->get_cb_skybox()->GetGPUVirtualAddress());

			D3D12_CPU_DESCRIPTOR_HANDLE skybox_cpu_handle = ResourceManager::instance()->get_skybox_srv_cpu();
			if (skybox_cpu_handle.ptr == 0) skybox_cpu_handle = ResourceManager::instance()->get_texture("__DEFAULT_BLACK__")->cpu_handle;
			bind_texture_table(commandList, 4, { skybox_cpu_handle });

			for (const auto& gameObject : gameObjects) {
				auto renderComp = gameObject->get_component<RenderComponent>();
				if (renderComp && renderComp->is_enabled()) renderComp->render(commandList, frame_index);
			}
			continue;
		}

		// 파티클 특수 처리
		if (psoName == "particle_draw") {
			for (const auto& gameObject : gameObjects) {
				auto particleRenderComp = gameObject->get_component<ParticleRenderComponent>();
				auto psComp = gameObject->get_component<ParticleSystemComponent>();
				if (particleRenderComp && psComp && particleRenderComp->is_enabled()) {
					psComp->dispatch_compute(commandList);
					commandList->SetPipelineState(pso);
					commandList->SetGraphicsRootSignature(root_signature);
					if (camera) camera->update_shader_variables(commandList, frame_index);
					shader_prototype->update_per_object(commandList, this, gameObject.get());
					gameObject->prepare_render();
					particleRenderComp->render(commandList, frame_index);
				}
			}
			continue;
		}

		if (psoName == "gltf_instanced") {
			// 1. PSO 및 루트 시그니처 바인딩 (기존 Gltf와 동일하게 적용)
			ID3D12PipelineState* pso = get_pso(psoName);
			ID3D12RootSignature* rootSig = get_root_signature("gltf");
			commandList->SetPipelineState(pso);
			commandList->SetGraphicsRootSignature(rootSig);

			// 2. 공통 데이터 바인딩 (카메라, 조명 등)
			camera->update_shader_variables(commandList, frame_index);
			LightManager::instance()->bind(commandList, 3);
			ShadowManager::instance()->bind_for_lighting(commandList, 10, 11, this);

			// 3. 해당 PSO를 사용하는 오브젝트들(InstancedGroup)을 그리게 함
			for (const auto& gameObject : gameObjects) {
				auto renderComp = gameObject->get_component<InstancedRenderComponent>();
				if (renderComp && renderComp->is_enabled()) {
					renderComp->render(commandList, frame_index);
				}
			}
			continue;
		}

		// 일반 객체 (Skinned 등)
		for (const auto& gameObject : gameObjects) {
			auto renderComp = gameObject->get_component<RenderComponent>();
			if (!renderComp) continue;

			if (psoName == "skinned") {
				auto animComp = gameObject->get_component<AnimationComponent>();
				if (animComp) {
					D3D12_GPU_VIRTUAL_ADDRESS boneGpuAddr = animComp->get_bone_gpu_virtual_address();
					if (boneGpuAddr != 0) commandList->SetGraphicsRootConstantBufferView(12, boneGpuAddr);
				}
			}

			// 실제 객체 드로우 카운트 증가
			_totalRenderCount += 1;
			_totalDrawCalls += 1;

			shader_prototype->update_per_object(commandList, this, gameObject.get());
			gameObject->prepare_render();
			renderComp->render(commandList, frame_index);
		}
	}
}

void Renderer::render_pso_group(ID3D12GraphicsCommandList* commandList, const std::string& psoName, CameraComponent* camera, UINT frame_index) {
	auto it = _renderMap.find(psoName);
	if (it == _renderMap.end() || it->second.empty()) return;

	// 디스크립터 힙 설정 (SRV 테이블 사용을 위해 필수)
	ID3D12DescriptorHeap* heaps[] = { _dynamic_descriptor_heap.Get() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	// PSO 및 루트 시그니처 설정
	ID3D12PipelineState* pso = get_pso(psoName);
	auto proto_it = _shaderPrototypes.find(psoName);
	if (pso == nullptr || proto_it == _shaderPrototypes.end()) return;

	auto& proto = proto_it->second;
	commandList->SetPipelineState(pso);
	commandList->SetGraphicsRootSignature(get_root_signature(proto->required_root_signature()));

	if (psoName == "terrain") {
		LightManager::instance()->bind(commandList, 3);
		// Terrain은 ShadowManager의 bind_for_lighting(6, 7)을 사용함
		ShadowManager::instance()->bind_for_lighting(commandList, 6, 7, this);
	}

	if (camera) camera->update_shader_variables(commandList, frame_index);

	f3 camPos = camera->game_object()->transform()->get_world_position();

	for (auto& obj : it->second) {
		if (!obj) continue;
		proto->update_per_object(commandList, this, obj.get());
		obj->prepare_render();
		obj->get_component<RenderComponent>()->render(commandList, frame_index);
	}
}
void Renderer::draw_render_occlusion_culling_list(ID3D12GraphicsCommandList* commandList, CameraComponent* camera, UINT frame_index) {
	ID3D12DescriptorHeap* heaps[] = { _dynamic_descriptor_heap.Get() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	
	// 거리 기준 설정 (이 거리 안쪽은 가리개로 사용)
	const float nearThreshold = 200.0f;

	f3 camPos = camera->game_object()->transform()->get_world_position();
	ID3D12Resource* prevBuffer = OcclusionManager::instance()->get_result_buffer_for_predication(frame_index);

	// 0. 각 PSO 그룹 내의 객체들을 카메라 거리순으로 정렬 (Front-to-Back)
	for (auto& pair : _renderMap) {
		const std::string& psoName = pair.first;
		// UI나 Skybox는 정렬 방식이 다르거나 필요 없으므로 불투명 객체만 수행
		if (psoName == "gltf" || psoName == "skinned") {
			std::sort(pair.second.begin(), pair.second.end(), [&camPos](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
				float distA = Vector3::Length(Vector3::Subtract(camPos, a->transform()->get_world_position()));
				float distB = Vector3::Length(Vector3::Subtract(camPos, b->transform()->get_world_position()));
				return distA < distB; // 가까운 것부터
				});
		}
	}

	if (camera) {
		camera->set_viewports_and_scissor_rects(commandList);
	}

	// --- STEP 1: 지형(Terrain)을 먼저 그리기 (Occluder) ---
	// 지형은 성을 가려야 하는 "벽"이므로, 쿼리 전에 무조건 먼저 그려서 깊이 버퍼를 채웁니다.
	{
		auto it = _renderMap.find("terrain");
		if (it != _renderMap.end()) {
			render_pso_group(commandList, "terrain", camera, frame_index);
		}
	}

	// --- STEP 2: 가까운 객체들 먼저 그리기 (깊이 버퍼 채우기) ---
 // 이 객체들은 쿼리 없이 그려져서 뒤에 있는 물체들을 가리는 벽 역할을 합니다.
	std::string opaque_targets[] = { "gltf", "skinned" };
	//for (const std::string& target : opaque_targets) {
	//    auto it = _renderMap.find(target);
	//    if (it == _renderMap.end()) continue;

	//    // 해당 PSO 설정
	//    auto pso = get_pso(target);
	//    auto proto = _shaderPrototypes[target];
	//    commandList->SetPipelineState(pso);
	//    commandList->SetGraphicsRootSignature(get_root_signature(proto->required_root_signature()));

	//    if (camera) camera->update_shader_variables(commandList, frame_index);

	//    for (auto& obj : it->second) {
	//        float dist = Vector3::Length(Vector3::Subtract(camPos, obj->transform()->get_world_position()));

	//        // 가까운 물체이거나 occlusion을 스킵해야 하는 경우만 먼저 그림
	//        if (dist < nearThreshold || obj->get_component<RenderComponent>()->skip_occlusion()) {
	//            if (target == "skinned") {
	//                auto animComp = obj->get_component<AnimationComponent>();
	//                if (animComp) {
	//                    D3D12_GPU_VIRTUAL_ADDRESS boneGpuAddr = animComp->get_bone_gpu_virtual_address();
	//                    if (boneGpuAddr != 0) commandList->SetGraphicsRootConstantBufferView(12, boneGpuAddr);
	//                }
	//            }
	//            proto->update_per_object(commandList, this, obj.get());
	//            obj->prepare_render();
	//            obj->get_component<RenderComponent>()->render(commandList, frame_index);
	//        }
	//    }
	//}

	// --- STEP 3: 먼 객체들에 대해서만 쿼리 발행 ---
	auto occ_pso = get_pso("occlusion_query");
	auto occ_sig = get_root_signature("occlusion_sig");
	if (occ_pso && occ_sig && _unitCube) {
		commandList->SetPipelineState(occ_pso);
		commandList->SetGraphicsRootSignature(occ_sig);

		if (camera) camera->update_shader_variables(commandList, frame_index);

		for (const std::string& target : opaque_targets) {
			auto it = _renderMap.find(target);
			if (it == _renderMap.end()) continue;

			for (auto& obj : it->second) {
				float dist = Vector3::Length(Vector3::Subtract(camPos, obj->transform()->get_world_position()));
				auto rc = obj->get_component<RenderComponent>();
				// 이미 위에서 그린 물체는 쿼리할 필요 없음
				if (dist < nearThreshold || rc->skip_occlusion()) continue;

				XMMATRIX boxWorld = XMMatrixTranspose(rc->get_occlusion_box_world_matrix());
				commandList->SetGraphicsRoot32BitConstants(0, 16, &boxWorld, 0);

				commandList->BeginQuery(OcclusionManager::instance()->get_query_heap(), D3D12_QUERY_TYPE_OCCLUSION, rc->get_occlusion_query_index());
				_unitCube->render(commandList);
				commandList->EndQuery(OcclusionManager::instance()->get_query_heap(), D3D12_QUERY_TYPE_OCCLUSION, rc->get_occlusion_query_index());
			}
		}
	}

	OcclusionManager::instance()->resolve_queries(commandList, frame_index);

	//static int frameCount = 0;
	//if (frameCount++ % 60 == 0) { // 60프레임마다 한 번씩 출력 (너무 자주 찍히면 무거우므로)
	//    auto occ = OcclusionManager::instance();
	//    CLOG("[Occlusion Info] Active: " << occ->get_active_index_count());
	//}

	// --- STEP 4: 먼 객체들 조건부 렌더링 (Predication) ---
	for (const std::string& target : opaque_targets) {
		auto it = _renderMap.find(target);
		if (it == _renderMap.end()) continue;

		auto pso = get_pso(target);
		auto proto = _shaderPrototypes[target];
		commandList->SetPipelineState(pso);
		commandList->SetGraphicsRootSignature(get_root_signature(proto->required_root_signature()));

		if (camera) camera->update_shader_variables(commandList, frame_index);

		// [필수] 조명 및 그림자 바인딩 (원래 쓰던 슬롯 번호에 맞춰서 추가)
		if (target == "gltf" || target == "skinned") {
			LightManager::instance()->bind(commandList, 3);
			ShadowManager::instance()->bind_for_lighting(commandList, 10, 11, this);
		}

		for (auto& obj : it->second) {
			//float dist = Vector3::Length(Vector3::Subtract(camPos, obj->transform()->get_world_position()));
			auto rc = obj->get_component<RenderComponent>();

			// 이미 STEP 2에서 그린 물체는 스킵
			if (/*dist < nearThreshold ||*/ rc->skip_occlusion()) continue;

			if (target == "skinned") {
				auto animComp = obj->get_component<AnimationComponent>();
				if (animComp) {
					D3D12_GPU_VIRTUAL_ADDRESS boneGpuAddr = animComp->get_bone_gpu_virtual_address();
					if (boneGpuAddr != 0) commandList->SetGraphicsRootConstantBufferView(12, boneGpuAddr);
				}
			}

			proto->update_per_object(commandList, this, obj.get());
			obj->prepare_render();

			// 이전 프레임의 쿼리 결과에 따라 그릴지 결정
			commandList->SetPredication(prevBuffer, rc->get_occlusion_query_index() * sizeof(UINT64), D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);
			rc->render(commandList, frame_index);
			commandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
		}
	}

	// Step 4: Skybox 렌더링 (항상 마지막, Occlusion Culling 제외)
	auto itSky = _renderMap.find("skybox");
	if (itSky != _renderMap.end() && !itSky->second.empty()) {
		const std::string psoName = "skybox";
		ID3D12PipelineState* pso = get_pso(psoName);
		auto proto_it = _shaderPrototypes.find(psoName);

		if (pso && proto_it != _shaderPrototypes.end()) {
			auto& proto = proto_it->second;
			commandList->SetPipelineState(pso);
			commandList->SetGraphicsRootSignature(get_root_signature(proto->required_root_signature()));
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);

			// Skybox 전용 상수 데이터 바인딩 (슬롯 2)
			if (camera && camera->get_cb_skybox()) {
				D3D12_GPU_VIRTUAL_ADDRESS cbAddress = camera->get_cb_skybox()->GetGPUVirtualAddress();
				commandList->SetGraphicsRootConstantBufferView(2, cbAddress);
			}

			// Skybox 텍스처 바인딩 (슬롯 4)
			D3D12_CPU_DESCRIPTOR_HANDLE skybox_cpu_handle = ResourceManager::instance()->get_skybox_srv_cpu();
			if (skybox_cpu_handle.ptr == 0) {
				skybox_cpu_handle = ResourceManager::instance()->get_texture("__DEFAULT_BLACK__")->cpu_handle;
			}
			bind_texture_table(commandList, 4, { skybox_cpu_handle });

			if (camera) camera->update_shader_variables(commandList, frame_index);

			for (const auto& gameObject : itSky->second) {
				auto renderComp = gameObject->get_component<RenderComponent>();
				if (renderComp && renderComp->is_enabled()) {
					renderComp->render(commandList, frame_index);
				}
			}
		}
	}

	// Step 5: 파티클 렌더링 (항상 Occlusion Culling 이후, Skybox 이후)
	auto itParticle = _renderMap.find("particle_draw");
	if (itParticle != _renderMap.end() && !itParticle->second.empty()) {
		const std::string target = "particle_draw";
		auto pso = get_pso(target);
		auto proto = _shaderPrototypes[target];
		auto root_sig = get_root_signature(proto->required_root_signature());

		for (auto& obj : itParticle->second) {
			auto particleRenderComp = obj->get_component<ParticleRenderComponent>();
			auto psComp = obj->get_component<ParticleSystemComponent>();

			if (particleRenderComp && psComp && particleRenderComp->is_enabled()) {

				// 1. 연산 패스 (Compute) : 위치 계산
				psComp->dispatch_compute(commandList);

				// 2. 파이프라인 상태 복구 (Compute -> Graphics)
				commandList->SetPipelineState(pso);
				commandList->SetGraphicsRootSignature(root_sig);
				commandList->SetDescriptorHeaps(_countof(heaps), heaps);

				// 연산 중에 날아간 카메라 상수 버퍼(b1) 다시 세팅
				if (camera) {
					camera->update_shader_variables(commandList, frame_index);
					camera->set_viewports_and_scissor_rects(commandList);
				}

				// 3. 그리기 준비 및 호출
				proto->update_per_object(commandList, this, obj.get());
				obj->prepare_render();
				particleRenderComp->render(commandList, frame_index);
			}
		}
	}

	// --- STEP 6: UI 및 특수 렌더 (Early-Z 활용 안 함) ---
	std::string ui_targets[] = { "billboard_ui", "Monster_HP_UI", "ui_frame", "ui" };
	for (const std::string& target : ui_targets) {
		auto it = _renderMap.find(target);
		if (it == _renderMap.end() || it->second.empty()) continue;

		auto pso = get_pso(target);
		auto proto_it = _shaderPrototypes.find(target);
		if (!pso || proto_it == _shaderPrototypes.end()) continue;

		commandList->SetPipelineState(pso);
		commandList->SetGraphicsRootSignature(get_root_signature(proto_it->second->required_root_signature()));
		if (camera) camera->update_shader_variables(commandList, frame_index);

		for (auto& obj : it->second) {
			proto_it->second->update_per_object(commandList, this, obj.get());
			obj->prepare_render();
			obj->get_component<RenderComponent>()->render(commandList, frame_index);
		}
	}
}

ID3D12RootSignature* Renderer::get_root_signature(const std::string& name) const
{
	// _rootSignatures 맵에서 'name'을 키로 가지는 원소를 찾습니다.
	auto it = _rootSignatures.find(name);

	// 찾았다면, 해당 원소의 값(ComPtr<ID3D12RootSignature>)에 접근하여
	// .Get() 함수로 실제 포인터를 반환합니다.
	if (it != _rootSignatures.end())
	{
		return it->second.Get();
	}

	// 맵에 해당 이름의 루트 시그니처가 없으면 nullptr을 반환합니다.
	// (또는 에러를 로그로 남기거나 기본값을 반환할 수도 있습니다.)
	return nullptr;
}

ID3D12PipelineState* Renderer::get_pso(const std::string& name) const
{
	auto it = _pipelineStates.find(name);
	if (it != _pipelineStates.end())
	{
		return it->second.Get();
	}
	return nullptr;
}

std::shared_ptr<Shader> Renderer::get_shader(const std::string& name) const
{
	auto it = _shaderPrototypes.find(name);
	if (it != _shaderPrototypes.end())
	{
		return it->second;
	}
	return nullptr;
}

void Renderer::bind_texture_table(ID3D12GraphicsCommandList* command_list, UINT root_parameter_index, const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& cpu_handles)
{
	// bind_material에서 만들어진 실제 GPU를 가르키는 포인터가 여기서 root parameter 슬롯에 꽂힌다.

	if (cpu_handles.empty()) return;
	
	UINT num_descriptors = static_cast<UINT>(cpu_handles.size());
	
	// [수정] 현재 프레임의 할당량이 꽉 찼는지 확인
	 // 현재 인덱스 + 필요 개수 > (현재 프레임 + 1) * 구획 크기
	 // frame_index를 여기서 알기 어려우므로, 간단히 _current_dynamic_descriptor_index가 범위를 넘는지 확인해도 됩니다.

	 // 현재 인덱스가 힙 전체 용량을 넘거나, 다음 프레임 구획을 침범하려 하면 에러
	 // (간단한 버전: 전체 용량만 체크해도 되지만, 엄격하게 하려면 아래처럼)
	UINT current_frame_start = (_current_dynamic_descriptor_index / _max_descriptors_per_frame) * _max_descriptors_per_frame;
	UINT limit = current_frame_start + _max_descriptors_per_frame;

	if (_current_dynamic_descriptor_index + num_descriptors > limit)
	{
		//TODO: pkj주석 npc 1000명이상일때 동적 디스크립터 힙이 여기서 터짐 -> 해결방법 고민해보기
		CERROR("Dynamic descriptor heap segment for this frame is full! Increase capacity.");
		return;
	}
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE dest_cpu_handle_start(_dynamic_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), _current_dynamic_descriptor_index, _descriptor_size);
	CD3DX12_GPU_DESCRIPTOR_HANDLE dest_gpu_handle_start(_dynamic_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), _current_dynamic_descriptor_index, _descriptor_size);
	
	for (UINT i = 0; i < num_descriptors; ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dest_handle = dest_cpu_handle_start;
		dest_handle.ptr += i * _descriptor_size;
		_device->CopyDescriptorsSimple(1, dest_handle, cpu_handles[i], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	
	command_list->SetGraphicsRootDescriptorTable(root_parameter_index, dest_gpu_handle_start);
	
	_current_dynamic_descriptor_index += num_descriptors;
	
}

void Renderer::create_dynamic_descriptor_heap(UINT capacity)
{
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.NumDescriptors = capacity;
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heap_desc.NodeMask = 0; // 명시적으로 설정

	HRESULT hr = _device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_dynamic_descriptor_heap));
	if (FAILED(hr))
	{
		CERROR("Failed to create dynamic descriptor heap!");
		return;
	}
	_dynamic_descriptor_heap_capacity = capacity;

	_max_descriptors_per_frame = capacity / SWAP_CHAIN_BUFFERS;

	_current_dynamic_descriptor_index = 0;
}

void Renderer::post_initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	if (_unitCube) {
		_unitCube->upload_to_gpu(device, commandList, 0);
	}
}
