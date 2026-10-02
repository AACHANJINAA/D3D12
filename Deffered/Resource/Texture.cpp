#include "../Common/stdafx.h"
#include "Texture.h"

namespace
{
    bool load_rgba_image(const std::filesystem::path& file_path,
        std::vector<uint8_t>& pixels, UINT& width, UINT& height)
    {
        ComPtr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;

        ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoderFromFilename(file_path.c_str(), nullptr,
            GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) return false;

        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)))
            return false;

        ComPtr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateFormatConverter(&converter)) ||
            FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
            return false;

        if (!width || !height || width > 16384 || height > 16384) return false;
        pixels.resize(static_cast<size_t>(width) * height * 4);
        return SUCCEEDED(converter->CopyPixels(nullptr, width * 4,
            static_cast<UINT>(pixels.size()), pixels.data()));
    }
}

bool IMAGE_TEXTURE::load(ID3D12Device* device, ID3D12GraphicsCommandList* list,
    const std::filesystem::path& path, bool isnormal)
{
    std::vector<uint8_t> pixels;
    UINT width = 0;
    UINT height = 0;
    if (path.empty())
    {
        width = height = 1;
        pixels = isnormal ? std::vector<uint8_t>{ 128, 128, 255, 255 }
            : std::vector<uint8_t>{ 255, 255, 255, 255 };
    }
    else if (!load_rgba_image(path, pixels, width, height)) return false;

    D3D12_RESOURCE_DESC texture_description{};
    texture_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_description.Width = width;
    texture_description.Height = height;
    texture_description.DepthOrArraySize = 1;
    texture_description.MipLevels = 1;
    texture_description.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    texture_description.SampleDesc.Count = 1;
    texture_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    const CD3DX12_HEAP_PROPERTIES default_heap(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE,
        &texture_description, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&_resource)))) return false;

    UINT64 upload_size = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT row_count = 0;
    UINT64 row_size = 0;
    device->GetCopyableFootprints(&texture_description, 0, 1, 0, &footprint,
        &row_count, &row_size, &upload_size);

    const CD3DX12_HEAP_PROPERTIES upload_heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto upload_description = CD3DX12_RESOURCE_DESC::buffer(upload_size);
    if (FAILED(device->CreateCommittedResource(&upload_heap, D3D12_HEAP_FLAG_NONE,
        &upload_description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&_upload)))) return false;

    uint8_t* mapped_data = nullptr;
    if (FAILED(_upload->Map(
        0, nullptr, reinterpret_cast<void**>(&mapped_data)))) return false;
    for (UINT row = 0; row < height; ++row)
    {
        std::memcpy(mapped_data + footprint.Offset + row * footprint.Footprint.RowPitch,
            pixels.data() + static_cast<size_t>(row) * width * 4, width * 4);
    }
    _upload->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = _upload.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = _resource.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

    const auto shader_resource_barrier = CD3DX12_RESOURCE_BARRIER::transition(
        _resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list->ResourceBarrier(1, &shader_resource_barrier);

    return true;
}

void TEXTURE_SET::release_uploads()
{
    for (auto& texture : _textures) if (texture) texture->release_upload();
    _brdf_upload_buffer.Reset();
    _specular_upload_buffer.Reset();
}

bool TEXTURE_SET::initialize(ID3D12Device* device,
    ID3D12GraphicsCommandList* command_list,
    const std::array<std::filesystem::path, texture_count>& file_paths,
    RESOURCE_CACHE<IMAGE_TEXTURE>* cache)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
    heap_description.NumDescriptors = specular_texture_index + 1;
    heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&_srv_heap))))
        return false;

    const UINT descriptor_size = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto descriptor_handle = _srv_heap->GetCPUDescriptorHandleForHeapStart();

    for (size_t texture_index = 0; texture_index < texture_count; ++texture_index)
    {
        const auto key = file_paths[texture_index].empty()
            ? std::wstring(texture_index == 1 ? L"@normal" : L"@white")
            : resource_key(file_paths[texture_index]);
        auto image = cache ? cache->find(key) : nullptr;
        if (!image)
        {
            image = std::make_shared<IMAGE_TEXTURE>();
            if (!image->load(device, command_list, file_paths[texture_index], texture_index == 1)) return false;
            if (cache) cache->insert(key, image);
        }
        _textures[texture_index] = std::move(image);

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_description{};
        const bool is_srgb_texture = texture_index == 0 || texture_index == 4;
        srv_description.Format = is_srgb_texture
            ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            : DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_description.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(_textures[texture_index]->get_resource(),
            &srv_description, descriptor_handle);
        descriptor_handle.ptr += descriptor_size;
    }
    return true;
}

bool TEXTURE_SET::append_cubemap(ID3D12Device* device, ID3D12Resource* cubemap)
{
    if (device == nullptr || cubemap == nullptr || _srv_heap == nullptr)
    {
        return false;
    }

    const UINT descriptor_size = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto descriptor_handle = _srv_heap->GetCPUDescriptorHandleForHeapStart();
    descriptor_handle.ptr += static_cast<SIZE_T>(ibl_texture_index) * descriptor_size;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_description{};
    srv_description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv_description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srv_description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_description.TextureCube.MostDetailedMip = 0;
    srv_description.TextureCube.MipLevels = 1;
    srv_description.TextureCube.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(cubemap, &srv_description, descriptor_handle);
    return true;
}

bool TEXTURE_SET::append_brdf_lut(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* command_list,
    const std::filesystem::path& file_path)
{
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return false;
    }
    const std::streamsize file_size = file.tellg();
    if (file_size < 148)
    {
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> file_data(static_cast<size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(file_data.data()), file_size))
    {
        return false;
    }

    const UINT width = *reinterpret_cast<const UINT*>(file_data.data() + 16);
    const UINT height = *reinterpret_cast<const UINT*>(file_data.data() + 12);
    constexpr UINT bytes_per_pixel = 8;
    const size_t pixel_size = static_cast<size_t>(width) * height * bytes_per_pixel;
    if (width == 0 || height == 0 || file_data.size() < 148 + pixel_size)
    {
        return false;
    }

    D3D12_RESOURCE_DESC texture_description{};
    texture_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_description.Width = width;
    texture_description.Height = height;
    texture_description.DepthOrArraySize = 1;
    texture_description.MipLevels = 1;
    texture_description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texture_description.SampleDesc.Count = 1;
    texture_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    const CD3DX12_HEAP_PROPERTIES default_heap(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(
        &default_heap, D3D12_HEAP_FLAG_NONE, &texture_description,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_brdf_lut))))
    {
        return false;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT row_count = 0;
    UINT64 row_size = 0;
    UINT64 upload_size = 0;
    device->GetCopyableFootprints(
        &texture_description, 0, 1, 0, &footprint,
        &row_count, &row_size, &upload_size);
    const CD3DX12_HEAP_PROPERTIES upload_heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto upload_description = CD3DX12_RESOURCE_DESC::buffer(upload_size);
    if (FAILED(device->CreateCommittedResource(
        &upload_heap, D3D12_HEAP_FLAG_NONE, &upload_description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&_brdf_upload_buffer))))
    {
        return false;
    }

    uint8_t* mapped_data = nullptr;
    if (FAILED(_brdf_upload_buffer->Map(
        0, nullptr, reinterpret_cast<void**>(&mapped_data))))
    {
        return false;
    }
    for (UINT row = 0; row < height; ++row)
    {
        std::memcpy(
            mapped_data + footprint.Offset + row * footprint.Footprint.RowPitch,
            file_data.data() + 148 + static_cast<size_t>(row) * width * bytes_per_pixel,
            width * bytes_per_pixel);
    }
    _brdf_upload_buffer->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = _brdf_upload_buffer.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = _brdf_lut.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    command_list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    const auto resource_barrier = CD3DX12_RESOURCE_BARRIER::transition(
        _brdf_lut.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command_list->ResourceBarrier(1, &resource_barrier);

    const UINT descriptor_size = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto descriptor_handle = _srv_heap->GetCPUDescriptorHandleForHeapStart();
    descriptor_handle.ptr += static_cast<SIZE_T>(brdf_texture_index) * descriptor_size;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_description{};
    srv_description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv_description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_description.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(_brdf_lut.Get(), &srv_description, descriptor_handle);
    return true;
}

bool TEXTURE_SET::append_specular_cubemap(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* command_list,
    const std::filesystem::path& file_path)
{
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return false;
    }
    const std::streamsize file_size = file.tellg();
    if (file_size < 128)
    {
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> file_data(static_cast<size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(file_data.data()), file_size))
    {
        return false;
    }

    const UINT width = *reinterpret_cast<const UINT*>(file_data.data() + 16);
    const UINT height = *reinterpret_cast<const UINT*>(file_data.data() + 12);
    const UINT mip_count = *reinterpret_cast<const UINT*>(file_data.data() + 28);
    constexpr UINT face_count = 6;
    constexpr UINT bytes_per_pixel = 8;
    if (width == 0 || height == 0 || mip_count == 0 || mip_count > 10)
    {
        return false;
    }

    size_t data_size = 0;
    for (UINT face = 0; face < face_count; ++face)
    {
        for (UINT mip = 0; mip < mip_count; ++mip)
        {
            const UINT mip_width = (std::max)(1u, width >> mip);
            const UINT mip_height = (std::max)(1u, height >> mip);
            data_size += static_cast<size_t>(mip_width) * mip_height * bytes_per_pixel;
        }
    }
    if (file_data.size() < 128 + data_size)
    {
        return false;
    }

    D3D12_RESOURCE_DESC texture_description{};
    texture_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_description.Width = width;
    texture_description.Height = height;
    texture_description.DepthOrArraySize = static_cast<UINT16>(face_count);
    texture_description.MipLevels = static_cast<UINT16>(mip_count);
    texture_description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texture_description.SampleDesc.Count = 1;
    texture_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    const CD3DX12_HEAP_PROPERTIES default_heap(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(
        &default_heap, D3D12_HEAP_FLAG_NONE, &texture_description,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&_specular_cubemap))))
    {
        return false;
    }

    constexpr UINT subresource_count = face_count * 10;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, subresource_count> footprints{};
    std::array<UINT, subresource_count> row_counts{};
    std::array<UINT64, subresource_count> row_sizes{};
    UINT64 upload_size = 0;
    const UINT actual_subresource_count = face_count * mip_count;
    device->GetCopyableFootprints(
        &texture_description, 0, actual_subresource_count, 0,
        footprints.data(), row_counts.data(), row_sizes.data(), &upload_size);

    const CD3DX12_HEAP_PROPERTIES upload_heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto upload_description = CD3DX12_RESOURCE_DESC::buffer(upload_size);
    if (FAILED(device->CreateCommittedResource(
        &upload_heap, D3D12_HEAP_FLAG_NONE, &upload_description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&_specular_upload_buffer))))
    {
        return false;
    }

    uint8_t* mapped_data = nullptr;
    if (FAILED(_specular_upload_buffer->Map(
        0, nullptr, reinterpret_cast<void**>(&mapped_data))))
    {
        return false;
    }

    size_t source_offset = 128;
    for (UINT face = 0; face < face_count; ++face)
    {
        for (UINT mip = 0; mip < mip_count; ++mip)
        {
            const UINT subresource = face * mip_count + mip;
            const UINT mip_width = (std::max)(1u, width >> mip);
            const UINT mip_height = (std::max)(1u, height >> mip);
            const size_t mip_row_size = static_cast<size_t>(mip_width) * bytes_per_pixel;
            for (UINT row = 0; row < mip_height; ++row)
            {
                std::memcpy(
                    mapped_data + footprints[subresource].Offset +
                        static_cast<size_t>(row) * footprints[subresource].Footprint.RowPitch,
                    file_data.data() + source_offset + row * mip_row_size,
                    mip_row_size);
            }
            source_offset += mip_row_size * mip_height;

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = _specular_upload_buffer.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = footprints[subresource];
            D3D12_TEXTURE_COPY_LOCATION destination{};
            destination.pResource = _specular_cubemap.Get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.SubresourceIndex = subresource;
            command_list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        }
    }
    _specular_upload_buffer->Unmap(0, nullptr);

    const auto resource_barrier = CD3DX12_RESOURCE_BARRIER::transition(
        _specular_cubemap.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command_list->ResourceBarrier(1, &resource_barrier);

    const UINT descriptor_size = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto descriptor_handle = _srv_heap->GetCPUDescriptorHandleForHeapStart();
    descriptor_handle.ptr += static_cast<SIZE_T>(specular_texture_index) * descriptor_size;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_description{};
    srv_description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv_description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srv_description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_description.TextureCube.MipLevels = mip_count;
    device->CreateShaderResourceView(
        _specular_cubemap.Get(), &srv_description, descriptor_handle);
    return true;
}

ID3D12DescriptorHeap* TEXTURE_SET::get_srv_heap() const
{
    return _srv_heap.Get();
}

D3D12_GPU_DESCRIPTOR_HANDLE TEXTURE_SET::get_gpu_handle() const
{
    return _srv_heap->GetGPUDescriptorHandleForHeapStart();
}
