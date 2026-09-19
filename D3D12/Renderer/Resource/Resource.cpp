#include "../../Common/stdafx.h"
#include "Resource.h"

namespace
{
    constexpr MESH_VERTEX cube_vertices[] =
    {
        { { -1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
        { { -1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 0.25f, 0.25f, 0.25f, 1.0f } }
    };

    constexpr uint16_t cube_indices[] =
    {
        0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1, 3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2, 0, 3, 7, 0, 7, 4
    };
}

RESOURCE::~RESOURCE()
{
    if (_constant_buffer != nullptr) _constant_buffer->Unmap(0, nullptr);
}

bool RESOURCE::initialize(ID3D12Device* device)
{
    _device = device;
    std::vector<MESH_VERTEX> grid_vertices;
    constexpr int grid_extent = 10;
    for (int line = -grid_extent; line <= grid_extent; ++line)
    {
        const float coordinate = static_cast<float>(line);
        constexpr float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        grid_vertices.push_back({ { -10.0f, 0.0f, coordinate }, { white[0], white[1], white[2], white[3] } });
        grid_vertices.push_back({ {  10.0f, 0.0f, coordinate }, { white[0], white[1], white[2], white[3] } });
        grid_vertices.push_back({ { coordinate, 0.0f, -10.0f }, { white[0], white[1], white[2], white[3] } });
        grid_vertices.push_back({ { coordinate, 0.0f,  10.0f }, { white[0], white[1], white[2], white[3] } });
    }

    if (!create_upload_buffer(grid_vertices.data(), sizeof(MESH_VERTEX) * grid_vertices.size(), _grid_vertex_buffer) ||
        !create_upload_buffer(cube_vertices, sizeof(cube_vertices), _cube_vertex_buffer) ||
        !create_upload_buffer(cube_indices, sizeof(cube_indices), _cube_index_buffer)) return false;

    _grid_vertex_view = { _grid_vertex_buffer->GetGPUVirtualAddress(), sizeof(MESH_VERTEX),
        static_cast<UINT>(sizeof(MESH_VERTEX) * grid_vertices.size()) };
    _cube_vertex_view = { _cube_vertex_buffer->GetGPUVirtualAddress(), sizeof(MESH_VERTEX), sizeof(cube_vertices) };
    _cube_index_view = { _cube_index_buffer->GetGPUVirtualAddress(), sizeof(cube_indices), DXGI_FORMAT_R16_UINT };

    constexpr UINT constant_buffer_size = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * 2;
    if (!create_upload_buffer(nullptr, constant_buffer_size, _constant_buffer)) return false;
    void* mapped_data = nullptr;
    if (FAILED(_constant_buffer->Map(0, nullptr, &mapped_data))) return false;
    _constant_data = static_cast<UINT8*>(mapped_data);

    const CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    const CD3DX12_RESOURCE_DESC resource_description = CD3DX12_RESOURCE_DESC::texture2d(
        DXGI_FORMAT_D32_FLOAT, 1280, 720, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    const CD3DX12_CLEAR_VALUE clear_value(DXGI_FORMAT_D32_FLOAT, 1.0f);
    return SUCCEEDED(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE,
        &resource_description, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value,
        IID_PPV_ARGS(&_depth_buffer)));
}

bool RESOURCE::create_depth_stencil_view(ID3D12Device* device)
{
    const CD3DX12_DESCRIPTOR_HEAP_DESC description(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    if (FAILED(device->CreateDescriptorHeap(&description, IID_PPV_ARGS(&_depth_stencil_heap)))) return false;
    device->CreateDepthStencilView(_depth_buffer.Get(), nullptr, get_depth_stencil());
    return true;
}

bool RESOURCE::create_upload_buffer(const void* data, UINT64 size, ComPtr<ID3D12Resource>& resource)
{
    const CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    const CD3DX12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::buffer(size);
    if (FAILED(_device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)))) return false;
    void* mapped_data = nullptr;
    if (FAILED(resource->Map(0, nullptr, &mapped_data))) return false;
    if (data != nullptr) std::memcpy(mapped_data, data, static_cast<size_t>(size));
    resource->Unmap(0, nullptr);
    return true;
}

void RESOURCE::update_transforms(const MATH::MATRIX4X4& grid, const MATH::MATRIX4X4& cube)
{
    std::memcpy(_constant_data, &grid, sizeof(grid));
    std::memcpy(_constant_data + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &cube, sizeof(cube));
}

D3D12_VERTEX_BUFFER_VIEW RESOURCE::get_grid_vertex_view() const { return _grid_vertex_view; }
D3D12_VERTEX_BUFFER_VIEW RESOURCE::get_cube_vertex_view() const { return _cube_vertex_view; }
D3D12_INDEX_BUFFER_VIEW RESOURCE::get_cube_index_view() const { return _cube_index_view; }
D3D12_GPU_VIRTUAL_ADDRESS RESOURCE::get_constant_address(UINT slot) const
{
    return _constant_buffer->GetGPUVirtualAddress() +
        D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * slot;
}
ID3D12Resource* RESOURCE::get_depth_buffer() const { return _depth_buffer.Get(); }
D3D12_CPU_DESCRIPTOR_HANDLE RESOURCE::get_depth_stencil() const
{
    return _depth_stencil_heap->GetCPUDescriptorHandleForHeapStart();
}
