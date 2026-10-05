#if defined(TEST_FORWARD)
#include "../Forward/UI/ViewerPanels.h"
#include "../Forward/Renderer/Pass/PipForwardPass.h"
#else
#include "../Deffered/UI/ViewerPanels.h"
#include "../Deffered/Renderer/SceneRenderData.h"
#endif
#include "../Benchmark/BenchmarkScene.h"
#include "../Benchmark/FrameProfiler.h"
#include <roapi.h>
#include <iostream>

namespace
{
    void check(bool isvalid, const char* message)
    {
        if (!isvalid) throw std::runtime_error(message);
    }

    ComPtr<ID3D12Resource> texture(ID3D12Device* device, UINT16 faces)
    {
        auto description = CD3DX12_RESOURCE_DESC::texture_2d(DXGI_FORMAT_R16G16B16A16_FLOAT, 1, 1);
        description.DepthOrArraySize = faces;
        const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
        ComPtr<ID3D12Resource> resource;
        check(SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&resource))), "Test texture creation failed.");
        return resource;
    }
}

int main()
{
    if (FAILED(RoInitialize(RO_INIT_MULTITHREADED))) return 1;
    int result = 0;
    try
    {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<ID3D12Device> device;
        check(SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))), "Factory creation failed.");
        check(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))), "WARP adapter unavailable.");
        check(SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))), "WARP device failed.");
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> list;
        check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))), "Allocator failed.");
        check(SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&list))), "Command list failed.");
        auto model = std::make_shared<MODEL_RESOURCE>();
        RESOURCE_CACHE<IMAGE_TEXTURE> cache;
        std::string error;
        check(model->load(device.Get(), list.Get(), L"Asset/Mesh/BoxTextured/glTF/BoxTextured.gltf", cache, error), error.c_str());
        check(SUCCEEDED(list->Close()), "Close failed.");
        // Upload the fixture and verify timestamp/fence readback without a window or capture.
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12Fence> fence;
        D3D12_COMMAND_QUEUE_DESC queue_description{};
        check(SUCCEEDED(device->CreateCommandQueue(&queue_description, IID_PPV_ARGS(&queue))), "Queue failed.");
        check(SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))), "Fence failed.");
        ID3D12CommandList* upload_lists[] = {list.Get()};
        queue->ExecuteCommandLists(1, upload_lists);
        check(SUCCEEDED(queue->Signal(fence.Get(), 1)), "Upload signal failed.");
        check(SUCCEEDED(fence->SetEventOnCompletion(1, nullptr)), "Upload wait failed.");
        model->release_uploads();
        const auto primitive_count = model->get_mesh().get_primitives().size();
        check(primitive_count == 1, "BoxTextured fixture changed.");
        SCENE scene;
        BENCHMARK_SCENE benchmark;
        const auto invalid = std::filesystem::path(L"Tests/bin/invalid-batching.json");
        auto reject = [&](const char* json) {
            { std::ofstream file(invalid); file << json; }
            check(!benchmark.load_file(invalid, error), "Invalid benchmark configuration accepted.");
        };
        reject(R"({"frustum_culling":true})");
        reject(R"({"distance_culling":true})");
        reject(R"({"occlusion_culling":true})");
        reject(R"({"backface_culling":true})");
        reject(R"({"grid":{"model":"unused.gltf","count":50001,"columns":1}})");
        reject(R"({"grid":{"model":"unused.gltf","count":1.5,"columns":1}})");
        reject(R"({"grid":{"model":"unused.gltf","count":1,"columns":0}})");
        reject(R"({"grid":{"model":"unused.gltf","count":1,"columns":1,"scale":[0,0,0]}})");
        reject(R"({"grid":{"model":"unused.gltf","count":1,"columns":1,"scale":[1,2,1]}})");
        reject(R"({"light_count":257,"grid":{"model":"unused.gltf","count":1,"columns":1}})");
        reject(R"({"light_count":-1,"grid":{"model":"unused.gltf","count":1,"columns":1}})");
        reject(R"({"profile":"native","measurement_seconds":1})");
        reject(R"({"warmup_seconds":-1})");
        reject(R"({"measurement_seconds":601})");
        check(benchmark.load_file(L"Benchmark/comparison.json", error), error.c_str());
        const auto first_light = benchmark.light_data.points[0];
        {
            std::ofstream file(invalid);
            file << R"({"light_count":256,"grid":{"model":"unused.gltf","count":1,"columns":1}})";
        }
        check(benchmark.load_file(invalid, error) && benchmark.light_data.count == 256,
            "256-light boundary must be accepted.");
        check(benchmark.load_file(L"Benchmark/comparison.json", error), error.c_str());
        check(std::memcmp(&first_light, &benchmark.light_data.points[0], sizeof(first_light)) == 0,
            "Light generation must be deterministic.");
        FRAME_PROFILER profiler;
        check(profiler.initialize(device.Get(), queue.Get(), factory.Get(), benchmark, "Test"), "Profiler initialization failed.");
        check(SUCCEEDED(allocator->Reset()) && SUCCEEDED(list->Reset(allocator.Get(), nullptr)), "Query reset failed.");
        check(profiler.begin(0, fence.Get()), "Query begin failed.");
        for (UINT index = 0; index < FRAME_PROFILER::query_count; ++index) profiler.stamp(list.Get(), 0, index);
        profiler.resolve(list.Get(), 0);
        check(SUCCEEDED(list->Close()), "Query close failed.");
        queue->ExecuteCommandLists(1, upload_lists);
        check(SUCCEEDED(queue->Signal(fence.Get(), 2)), "Query signal failed.");
        profiler.submit(0, 2, {});
        check(SUCCEEDED(fence->SetEventOnCompletion(2, nullptr)), "Query wait failed.");
        check(profiler.begin(0, fence.Get()) && profiler.latest().gpu_ms >= 0, "Timestamp collection failed.");
        check(profiler.finish(fence.Get(), true), "Profiler finish failed.");
        check(benchmark.load_file(L"Benchmark/batching-10000.json", error), error.c_str());
        check(benchmark.objects.size() == 10000, "Grid did not generate 10000 objects.");
        check(benchmark.load_file(L"Benchmark/batching-50000.json", error), error.c_str());
        check(benchmark.objects.size() == 50000, "Grid did not generate 50000 objects.");
        size_t loads = 0;
        check(benchmark.populate(scene, [&](const VIEWER_REQUEST&) { ++loads; return model; }, error), error.c_str());
        check(loads == 1 && scene.get_objects().size() == 50000, "Shared-model bulk import failed.");
        check(scene.get_selected_id() == 0, "Benchmark selection must be empty.");
        // These objects must still be submitted despite the old Forward distance/frustum rules.
        scene.get_objects()[0].position = {10000, 0, -10000};
        scene.get_objects()[1].position = {0, 0, -10000};
        VIEWER_SETTINGS settings;
#if defined(TEST_FORWARD)
        PIP_FORWARD_PASS pass;
        check(pass.initialize(device.Get(), false), "Forward root signature / PSO failed.");
        PIP_FORWARD_PASS matched;
        const bool ismatched_ready = matched.initialize(device.Get(), false, true);
        ComPtr<ID3D12InfoQueue> diagnostics;
        if (!ismatched_ready && SUCCEEDED(device.As(&diagnostics)))
            for (UINT64 index = 0; index < diagnostics->GetNumStoredMessages(); ++index)
            {
                SIZE_T size = 0;
                diagnostics->GetMessage(index, nullptr, &size);
                std::vector<char> storage(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                if (SUCCEEDED(diagnostics->GetMessage(index, message, &size))) std::cerr << message->pDescription << '\n';
            }
        check(ismatched_ready, "Matched Forward PSO failed.");
        auto cube = texture(device.Get(), 6), lut = texture(device.Get(), 1);
        auto update = [&](bool isinstancing) {
            return pass.update(device.Get(), 0, scene, settings, MATH::identity_matrix(), MATH::identity_matrix(),
                {}, cube.Get(), cube.Get(), lut.Get(), isinstancing);
        };
        auto draws = [&]() { return pass.get_draw_count(0); };
        auto instances = [&]() { return pass.get_instance_count(0); };
#else
        GBUFFER_RENDER_PASS pipeline;
        check(pipeline.initialize(device.Get()), "Deferred root signature / instanced PSO failed.");
        SCENE_RENDER_DATA pass;
        auto update = [&](bool isinstancing) {
            return pass.update(device.Get(), scene, settings, MATH::identity_matrix(), {}, isinstancing);
        };
        auto draws = [&]() { return pass.get_draws().size(); };
        auto instances = [&]() { return pass.get_instance_count(); };
#endif
        check(update(true), "Batched update failed.");
        check(draws() == 1 && instances() == 50000, "Batching dropped instances or failed to merge.");
        check(update(false), "Unbatched update failed.");
        check(draws() == 50000 && instances() == 50000, "Unbatched submission count mismatch.");
        scene.get_objects()[2].materials[0].roughness_multiplier = 0.5f;
        scene.get_objects()[3].scale = {2, 3, 4};
        scene.select(scene.get_objects()[3].id);
        check(update(true), "Override/transform update failed.");
        check(draws() == 2 && instances() == 50000, "Material split or per-instance selection grouping failed.");
        scene.get_objects()[2].materials[0].roughness_multiplier = 1;
        check(update(true) && draws() == 1, "Material groups did not merge after restoring override.");
        scene.clear();
        check(update(true) && draws() == 0 && instances() == 0, "Empty scene regression.");
        std::cout << "PASS: config/light validation, deterministic lights, GPU timestamps, 10000/50000 grid, shared import, no culling, 1 vs 50000 draws, material split, empty scene, PSO.\n";
    }
    catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; result = 1; }
    RoUninitialize();
    return result;
}
