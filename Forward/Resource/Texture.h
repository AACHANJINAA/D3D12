#pragma once

#include "../Common/stdafx.h"
#include "ResourceCache.h"

class IMAGE_TEXTURE
{
public:
    bool load(ID3D12Device* device, ID3D12GraphicsCommandList* list,
        const std::filesystem::path& path, bool isnormal);
    ID3D12Resource* get_resource() const { return _resource.Get(); }
    void release_upload() { _upload.Reset(); }
private:
    ComPtr<ID3D12Resource> _resource;
    ComPtr<ID3D12Resource> _upload;
};

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
        const std::array<std::filesystem::path, texture_count>& file_paths,
        RESOURCE_CACHE<IMAGE_TEXTURE>* cache = nullptr);
    void release_uploads();
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
    ID3D12Resource* get_texture(size_t index) const { return _textures.at(index)->get_resource(); }
    ID3D12Resource* get_brdf_lut() const { return _brdf_lut.Get(); }
    ID3D12Resource* get_specular_cubemap() const { return _specular_cubemap.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle() const;

private:
    std::array<std::shared_ptr<IMAGE_TEXTURE>, texture_count> _textures;
    ComPtr<ID3D12Resource> _brdf_lut;
    ComPtr<ID3D12Resource> _brdf_upload_buffer;
    ComPtr<ID3D12Resource> _specular_cubemap;
    ComPtr<ID3D12Resource> _specular_upload_buffer;
    ComPtr<ID3D12DescriptorHeap> _srv_heap;
};
