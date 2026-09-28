#include "../../Common/stdafx.h"
#include "SkyboxRenderPass.h"
#include "../Shader/Shader.h"

bool SKYBOX_RENDER_PASS::initialize(ID3D12Device* device)
{
    if (device == nullptr)
    {
        return false;
    }

    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;
    if (!SHADER::get_instance().compile_shader(
        L"Skybox.hlsl", "VS_Skybox", "vs_5_0", vertex_shader) ||
        !SHADER::get_instance().compile_shader(
            L"Skybox.hlsl", "PS_Skybox", "ps_5_0", pixel_shader))
    {
        return false;
    }

    D3D12_DESCRIPTOR_RANGE texture_range{};
    texture_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texture_range.NumDescriptors = 1;
    texture_range.BaseShaderRegister = 0;
    texture_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER root_parameter{};
    root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameter.DescriptorTable.NumDescriptorRanges = 1;
    root_parameter.DescriptorTable.pDescriptorRanges = &texture_range;
    root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    const D3D12_ROOT_SIGNATURE_DESC root_description
    {
        1, &root_parameter, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE
    };
    ComPtr<ID3DBlob> serialized_root_signature;
    ComPtr<ID3DBlob> root_signature_error;
    if (FAILED(D3D12SerializeRootSignature(
        &root_description, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized_root_signature, &root_signature_error)) ||
        FAILED(device->CreateRootSignature(
            0, serialized_root_signature->GetBufferPointer(),
            serialized_root_signature->GetBufferSize(),
            IID_PPV_ARGS(&_root_signature))))
    {
        if (root_signature_error != nullptr)
        {
            OutputDebugStringA(static_cast<const char*>(root_signature_error->GetBufferPointer()));
        }
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.pRootSignature = _root_signature.Get();
    description.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
    description.PS = { pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };
    description.RasterizerState = CD3DX12_RASTERIZER_DESC();
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.BlendState = CD3DX12_BLEND_DESC();
    description.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC();
    description.DepthStencilState.DepthEnable = FALSE;
    description.SampleMask = UINT_MAX;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    return SUCCEEDED(device->CreateGraphicsPipelineState(
        &description, IID_PPV_ARGS(&_pipeline)));
}

bool SKYBOX_RENDER_PASS::load_cubemap(
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

    const UINT height = *reinterpret_cast<const UINT*>(file_data.data() + 12);
    const UINT width = *reinterpret_cast<const UINT*>(file_data.data() + 16);
    const UINT dxgi_format = *reinterpret_cast<const UINT*>(file_data.data() + 128);
    const UINT resource_dimension = *reinterpret_cast<const UINT*>(file_data.data() + 132);
    const UINT misc_flags = *reinterpret_cast<const UINT*>(file_data.data() + 136);
    const UINT array_size = *reinterpret_cast<const UINT*>(file_data.data() + 140);
    constexpr UINT texture_cube_flag = 0x4;
    constexpr UINT face_count = 6;
    constexpr UINT bytes_per_pixel = 8;
    const size_t face_size = static_cast<size_t>(width) * height * bytes_per_pixel;
    if (width == 0 || height == 0 ||
        dxgi_format != DXGI_FORMAT_R16G16B16A16_FLOAT ||
        resource_dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        (misc_flags & texture_cube_flag) == 0 ||
        array_size == 0 || file_data.size() < 148 + face_size * face_count)
    {
        return false;
    }

    D3D12_RESOURCE_DESC texture_description{};
    texture_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_description.Width = width;
    texture_description.Height = height;
    texture_description.DepthOrArraySize = static_cast<UINT16>(face_count);
    texture_description.MipLevels = 1;
    texture_description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texture_description.SampleDesc.Count = 1;
    texture_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_description.Flags = D3D12_RESOURCE_FLAG_NONE;

    const CD3DX12_HEAP_PROPERTIES default_heap(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(
        &default_heap, D3D12_HEAP_FLAG_NONE, &texture_description,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_cubemap))))
    {
        return false;
    }

    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, face_count> footprints{};
    std::array<UINT, face_count> row_counts{};
    std::array<UINT64, face_count> row_sizes{};
    UINT64 upload_size = 0;
    device->GetCopyableFootprints(
        &texture_description, 0, face_count, 0, footprints.data(),
        row_counts.data(), row_sizes.data(), &upload_size);

    const CD3DX12_HEAP_PROPERTIES upload_heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto upload_description = CD3DX12_RESOURCE_DESC::buffer(upload_size);
    if (FAILED(device->CreateCommittedResource(
        &upload_heap, D3D12_HEAP_FLAG_NONE, &upload_description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_upload_buffer))))
    {
        return false;
    }

    uint8_t* mapped_data = nullptr;
    if (FAILED(_upload_buffer->Map(
        0, nullptr, reinterpret_cast<void**>(&mapped_data))))
    {
        return false;
    }
    for (UINT face = 0; face < face_count; ++face)
    {
        const uint8_t* source = file_data.data() + 148 + face_size * face;
        for (UINT row = 0; row < height; ++row)
        {
            std::memcpy(
                mapped_data + footprints[face].Offset +
                    static_cast<size_t>(row) * footprints[face].Footprint.RowPitch,
                source + static_cast<size_t>(row) * width * bytes_per_pixel,
                width * bytes_per_pixel);
        }

        D3D12_TEXTURE_COPY_LOCATION source_location{};
        source_location.pResource = _upload_buffer.Get();
        source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source_location.PlacedFootprint = footprints[face];
        D3D12_TEXTURE_COPY_LOCATION destination_location{};
        destination_location.pResource = _cubemap.Get();
        destination_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination_location.SubresourceIndex = face;
        command_list->CopyTextureRegion(
            &destination_location, 0, 0, 0, &source_location, nullptr);
    }
    _upload_buffer->Unmap(0, nullptr);

    const auto resource_barrier = CD3DX12_RESOURCE_BARRIER::transition(
        _cubemap.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command_list->ResourceBarrier(1, &resource_barrier);

    D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
    heap_description.NumDescriptors = 1;
    heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(
        &heap_description, IID_PPV_ARGS(&_srv_heap))))
    {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_description{};
    srv_description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv_description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srv_description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_description.TextureCube.MostDetailedMip = 0;
    srv_description.TextureCube.MipLevels = 1;
    srv_description.TextureCube.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(
        _cubemap.Get(), &srv_description, _srv_heap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

ID3D12Resource* SKYBOX_RENDER_PASS::get_cubemap() const
{
    return _cubemap.Get();
}

void SKYBOX_RENDER_PASS::render(ID3D12GraphicsCommandList* command_list) const
{
    command_list->SetPipelineState(_pipeline.Get());
    command_list->SetGraphicsRootSignature(_root_signature.Get());
    ID3D12DescriptorHeap* descriptor_heaps[] = { _srv_heap.Get() };
    command_list->SetDescriptorHeaps(1, descriptor_heaps);
    command_list->SetGraphicsRootDescriptorTable(
        0, _srv_heap->GetGPUDescriptorHandleForHeapStart());
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list->DrawInstanced(3, 1, 0, 0);
}
