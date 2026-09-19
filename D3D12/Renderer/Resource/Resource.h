#pragma once

#include "../../Common/stdafx.h"

struct MESH_VERTEX
{
    float position[3];
    float color[4];
};

class RESOURCE
{
public:
    ~RESOURCE();
    bool initialize(ID3D12Device* device);
    void update_transforms(const MATH::MATRIX4X4& grid, const MATH::MATRIX4X4& cube);
    D3D12_VERTEX_BUFFER_VIEW get_grid_vertex_view() const;
    D3D12_VERTEX_BUFFER_VIEW get_cube_vertex_view() const;
    D3D12_INDEX_BUFFER_VIEW get_cube_index_view() const;
    D3D12_GPU_VIRTUAL_ADDRESS get_constant_address(UINT slot) const;
    ID3D12Resource* get_depth_buffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE get_depth_stencil() const;
    bool create_depth_stencil_view(ID3D12Device* device);

private:
    bool create_upload_buffer(const void* data, UINT64 size, ComPtr<ID3D12Resource>& resource);

    ID3D12Device* _device = nullptr;
    ComPtr<ID3D12Resource> _grid_vertex_buffer;
    ComPtr<ID3D12Resource> _cube_vertex_buffer;
    ComPtr<ID3D12Resource> _cube_index_buffer;
    D3D12_VERTEX_BUFFER_VIEW _grid_vertex_view{};
    D3D12_VERTEX_BUFFER_VIEW _cube_vertex_view{};
    D3D12_INDEX_BUFFER_VIEW _cube_index_view{};
    ComPtr<ID3D12Resource> _depth_buffer;
    ComPtr<ID3D12DescriptorHeap> _depth_stencil_heap;
    ComPtr<ID3D12Resource> _constant_buffer;
    UINT8* _constant_data = nullptr;
};
