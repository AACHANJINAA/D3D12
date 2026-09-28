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

        pixels.resize(static_cast<size_t>(width) * height * 4);
        return SUCCEEDED(converter->CopyPixels(nullptr, width * 4,
            static_cast<UINT>(pixels.size()), pixels.data()));
    }
}

bool TEXTURE_SET::initialize(ID3D12Device* device,
    ID3D12GraphicsCommandList* command_list,
    const std::array<std::filesystem::path, texture_count>& file_paths)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
    heap_description.NumDescriptors = static_cast<UINT>(texture_count);
    heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&_srv_heap))))
        return false;

    const UINT descriptor_size = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto descriptor_handle = _srv_heap->GetCPUDescriptorHandleForHeapStart();

    for (size_t texture_index = 0; texture_index < texture_count; ++texture_index)
    {
        std::vector<uint8_t> pixels;
        UINT width = 0;
        UINT height = 0;
        if (!load_rgba_image(file_paths[texture_index], pixels, width, height)) return false;

        D3D12_RESOURCE_DESC texture_description{};
        texture_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture_description.Width = width;
        texture_description.Height = height;
        texture_description.DepthOrArraySize = 1;
        texture_description.MipLevels = 1;
        texture_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_description.SampleDesc.Count = 1;
        texture_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        const CD3DX12_HEAP_PROPERTIES default_heap(D3D12_HEAP_TYPE_DEFAULT);
        if (FAILED(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE,
            &texture_description, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&_textures[texture_index])))) return false;

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
            IID_PPV_ARGS(&_upload_buffers[texture_index])))) return false;

        uint8_t* mapped_data = nullptr;
        if (FAILED(_upload_buffers[texture_index]->Map(
            0, nullptr, reinterpret_cast<void**>(&mapped_data)))) return false;
        for (UINT row = 0; row < height; ++row)
        {
            std::memcpy(mapped_data + footprint.Offset + row * footprint.Footprint.RowPitch,
                pixels.data() + static_cast<size_t>(row) * width * 4, width * 4);
        }
        _upload_buffers[texture_index]->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = _upload_buffers[texture_index].Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = _textures[texture_index].Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        command_list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

        const auto shader_resource_barrier = CD3DX12_RESOURCE_BARRIER::transition(
            _textures[texture_index].Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list->ResourceBarrier(1, &shader_resource_barrier);

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_description{};
        srv_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_description.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(_textures[texture_index].Get(),
            &srv_description, descriptor_handle);
        descriptor_handle.ptr += descriptor_size;
    }
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
