#pragma once
#include "GltfMesh.h"

class MESH_RESOURCE
{
public:
    bool initialize(ID3D12Device* device, const GLTF_MESH& mesh);
    const D3D12_VERTEX_BUFFER_VIEW& get_vertex_view() const { return _vertex_view; }
    const D3D12_INDEX_BUFFER_VIEW& get_index_view() const { return _index_view; }
private:
    ComPtr<ID3D12Resource> _vertices;
    ComPtr<ID3D12Resource> _indices;
    D3D12_VERTEX_BUFFER_VIEW _vertex_view{};
    D3D12_INDEX_BUFFER_VIEW _index_view{};
};
