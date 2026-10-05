#include "../Common/stdafx.h"
#include "MeshResource.h"

namespace
{
    bool create_buffer(ID3D12Device* device, ID3D12GraphicsCommandList* list, const void* data, size_t size,
        D3D12_RESOURCE_STATES state, ComPtr<ID3D12Resource>& buffer, ComPtr<ID3D12Resource>& upload)
    {
        if (!size || size > UINT_MAX) return false;
        const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
        const auto description = CD3DX12_RESOURCE_DESC::buffer(size);
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
            &description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&upload)))) return false;
        void* mapped = nullptr;
        const D3D12_RANGE read_range{ 0, 0 };
        if (FAILED(upload->Map(0, &read_range, &mapped))) return false;
        std::memcpy(mapped, data, size);
        upload->Unmap(0, nullptr);
        const CD3DX12_HEAP_PROPERTIES gpu_heap(D3D12_HEAP_TYPE_DEFAULT);
        if (FAILED(device->CreateCommittedResource(&gpu_heap, D3D12_HEAP_FLAG_NONE,
            &description, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&buffer)))) return false;
        list->CopyBufferRegion(buffer.Get(), 0, upload.Get(), 0, size);
        const auto barrier = CD3DX12_RESOURCE_BARRIER::transition(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, state);
        list->ResourceBarrier(1, &barrier);
        return true;
    }
}

bool MESH_RESOURCE::initialize(ID3D12Device* device, ID3D12GraphicsCommandList* list, const GLTF_MESH& mesh)
{
    const auto& vertices = mesh.get_vertices();
    const auto& indices = mesh.get_indices();
    if (!create_buffer(device, list, vertices.data(), vertices.size() * sizeof(GLTF_VERTEX),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, _vertices, _vertex_upload) ||
        !create_buffer(device, list, indices.data(), indices.size() * sizeof(uint32_t),
            D3D12_RESOURCE_STATE_INDEX_BUFFER, _indices, _index_upload))
    {
        return false;
    }
    _vertex_view = { _vertices->GetGPUVirtualAddress(),
        static_cast<UINT>(vertices.size() * sizeof(GLTF_VERTEX)), sizeof(GLTF_VERTEX) };
    _index_view = { _indices->GetGPUVirtualAddress(),
        static_cast<UINT>(indices.size() * sizeof(uint32_t)), DXGI_FORMAT_R32_UINT };
    return true;
}
