#pragma once

#include "../Common/stdafx.h"

class TEXTURE
{
public:
    bool initialize(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        const std::filesystem::path& file_path);

    ID3D12DescriptorHeap* get_srv_heap() const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle() const;

private:
    ComPtr<ID3D12Resource> _texture;
    ComPtr<ID3D12Resource> _upload_buffer;
    ComPtr<ID3D12DescriptorHeap> _srv_heap;
};
