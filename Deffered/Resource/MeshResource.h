#pragma once
#include "GltfMesh.h"

class MESH_RESOURCE
{
public:
    bool initialize(ID3D12Device* device, ID3D12GraphicsCommandList* list, const GLTF_MESH& mesh);
    void release_uploads() { _vertex_upload.Reset(); _index_upload.Reset(); }
    const D3D12_VERTEX_BUFFER_VIEW& get_vertex_view() const { return _vertex_view; }
    const D3D12_INDEX_BUFFER_VIEW& get_index_view() const { return _index_view; }
private:
    ComPtr<ID3D12Resource> _vertices;
    ComPtr<ID3D12Resource> _indices;
    ComPtr<ID3D12Resource> _vertex_upload, _index_upload;
    D3D12_VERTEX_BUFFER_VIEW _vertex_view{};
    D3D12_INDEX_BUFFER_VIEW _index_view{};
};
