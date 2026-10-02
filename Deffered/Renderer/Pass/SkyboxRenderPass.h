#pragma once

#include "../../Common/stdafx.h"

class SKYBOX_RENDER_PASS
{
public:
    bool initialize(ID3D12Device* device);
    bool load_cubemap(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        const std::filesystem::path& file_path);
    ID3D12Resource* get_cubemap() const;
    void render(ID3D12GraphicsCommandList* command_list, float exposure = 0.0f) const;

private:
    ComPtr<ID3D12RootSignature> _root_signature;
    ComPtr<ID3D12PipelineState> _pipeline;
    ComPtr<ID3D12Resource> _cubemap;
    ComPtr<ID3D12Resource> _upload_buffer;
    ComPtr<ID3D12DescriptorHeap> _srv_heap;
};
