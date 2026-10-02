#include "../Common/stdafx.h"
#include "MeshResource.h"

namespace
{
    bool create_buffer(ID3D12Device* device, const void* data, size_t size,
        ComPtr<ID3D12Resource>& buffer)
    {
        if (!size || size > UINT_MAX) return false;
        const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
        const auto description = CD3DX12_RESOURCE_DESC::buffer(size);
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
            &description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&buffer)))) return false;
        void* mapped = nullptr;
        const D3D12_RANGE read_range{ 0, 0 };
        if (FAILED(buffer->Map(0, &read_range, &mapped))) return false;
        std::memcpy(mapped, data, size);
        buffer->Unmap(0, nullptr);
        return true;
    }
}

bool MESH_RESOURCE::initialize(ID3D12Device* device, const GLTF_MESH& mesh)
{
    const auto& vertices = mesh.get_vertices();
    const auto& indices = mesh.get_indices();
    if (!create_buffer(device, vertices.data(), vertices.size() * sizeof(GLTF_VERTEX), _vertices) ||
        !create_buffer(device, indices.data(), indices.size() * sizeof(uint32_t), _indices))
    {
        return false;
    }
    _vertex_view = { _vertices->GetGPUVirtualAddress(),
        static_cast<UINT>(vertices.size() * sizeof(GLTF_VERTEX)), sizeof(GLTF_VERTEX) };
    _index_view = { _indices->GetGPUVirtualAddress(),
        static_cast<UINT>(indices.size() * sizeof(uint32_t)), DXGI_FORMAT_R32_UINT };
    return true;
}
