#pragma once

#include "../Common/stdafx.h"

class TEXTURE_SET
{
public:
    static constexpr size_t texture_count = 5;
    static constexpr UINT ibl_texture_index = static_cast<UINT>(texture_count);
    static constexpr UINT brdf_texture_index = ibl_texture_index + 1;
    static constexpr UINT specular_texture_index = brdf_texture_index + 1;

    bool initialize(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        const std::array<std::filesystem::path, texture_count>& file_paths);
    bool append_cubemap(ID3D12Device* device, ID3D12Resource* cubemap);
    bool append_brdf_lut(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        const std::filesystem::path& file_path);
    bool append_specular_cubemap(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        const std::filesystem::path& file_path);

    ID3D12DescriptorHeap* get_srv_heap() const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle() const;

private:
    std::array<ComPtr<ID3D12Resource>, texture_count> _textures;
    std::array<ComPtr<ID3D12Resource>, texture_count> _upload_buffers;
    ComPtr<ID3D12Resource> _brdf_lut;
    ComPtr<ID3D12Resource> _brdf_upload_buffer;
    ComPtr<ID3D12Resource> _specular_cubemap;
    ComPtr<ID3D12Resource> _specular_upload_buffer;
    ComPtr<ID3D12DescriptorHeap> _srv_heap;
};
