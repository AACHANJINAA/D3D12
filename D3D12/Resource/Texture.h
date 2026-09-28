#pragma once

#include "../Common/stdafx.h"

class TEXTURE_SET
{
public:
    static constexpr size_t texture_count = 5;

    bool initialize(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        const std::array<std::filesystem::path, texture_count>& file_paths);

    ID3D12DescriptorHeap* get_srv_heap() const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle() const;

private:
    std::array<ComPtr<ID3D12Resource>, texture_count> _textures;
    std::array<ComPtr<ID3D12Resource>, texture_count> _upload_buffers;
    ComPtr<ID3D12DescriptorHeap> _srv_heap;
};
