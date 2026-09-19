#pragma once

#include <d3d12.h>

struct CD3DX12_COMMAND_QUEUE_DESC : public D3D12_COMMAND_QUEUE_DESC
{
    explicit CD3DX12_COMMAND_QUEUE_DESC(D3D12_COMMAND_LIST_TYPE type)
    {
        Type = type;
        Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        NodeMask = 0;
    }
};

struct CD3DX12_DESCRIPTOR_HEAP_DESC : public D3D12_DESCRIPTOR_HEAP_DESC
{
    CD3DX12_DESCRIPTOR_HEAP_DESC(UINT descriptor_count, D3D12_DESCRIPTOR_HEAP_TYPE type)
    {
        NumDescriptors = descriptor_count;
        Type = type;
        Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        NodeMask = 0;
    }
};

struct CD3DX12_ROOT_PARAMETER : public D3D12_ROOT_PARAMETER
{
    CD3DX12_ROOT_PARAMETER(
        D3D12_ROOT_PARAMETER_TYPE type,
        UINT shader_register,
        D3D12_SHADER_VISIBILITY visibility)
    {
        ParameterType = type;
        Descriptor.ShaderRegister = shader_register;
        Descriptor.RegisterSpace = 0;
        ShaderVisibility = visibility;
    }
};

struct CD3DX12_ROOT_SIGNATURE_DESC : public D3D12_ROOT_SIGNATURE_DESC
{
    CD3DX12_ROOT_SIGNATURE_DESC(
        UINT parameter_count,
        const D3D12_ROOT_PARAMETER* parameters,
        D3D12_ROOT_SIGNATURE_FLAGS flags)
    {
        NumParameters = parameter_count;
        pParameters = parameters;
        NumStaticSamplers = 0;
        pStaticSamplers = nullptr;
        Flags = flags;
    }
};

struct CD3DX12_HEAP_PROPERTIES : public D3D12_HEAP_PROPERTIES
{
    explicit CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE type)
    {
        Type = type;
        CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        CreationNodeMask = 1;
        VisibleNodeMask = 1;
    }
};

struct CD3DX12_RESOURCE_DESC : public D3D12_RESOURCE_DESC
{
    static CD3DX12_RESOURCE_DESC buffer(UINT64 size)
    {
        CD3DX12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        description.Width = size;
        description.Height = 1;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        return description;
    }

    static CD3DX12_RESOURCE_DESC texture2d(
        DXGI_FORMAT format,
        UINT64 width,
        UINT height,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
    {
        CD3DX12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = width;
        description.Height = height;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = format;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        description.Flags = flags;
        return description;
    }
};

struct CD3DX12_RESOURCE_BARRIER : public D3D12_RESOURCE_BARRIER
{
    static CD3DX12_RESOURCE_BARRIER transition(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES state_before,
        D3D12_RESOURCE_STATES state_after)
    {
        CD3DX12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = state_before;
        barrier.Transition.StateAfter = state_after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        return barrier;
    }
};

struct CD3DX12_RASTERIZER_DESC : public D3D12_RASTERIZER_DESC
{
    CD3DX12_RASTERIZER_DESC()
    {
        FillMode = D3D12_FILL_MODE_SOLID;
        CullMode = D3D12_CULL_MODE_BACK;
        FrontCounterClockwise = FALSE;
        DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        DepthClipEnable = TRUE;
        MultisampleEnable = FALSE;
        AntialiasedLineEnable = FALSE;
        ForcedSampleCount = 0;
        ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }
};

struct CD3DX12_BLEND_DESC : public D3D12_BLEND_DESC
{
    CD3DX12_BLEND_DESC()
    {
        AlphaToCoverageEnable = FALSE;
        IndependentBlendEnable = FALSE;
        for (auto& render_target : RenderTarget)
        {
            render_target.BlendEnable = FALSE;
            render_target.LogicOpEnable = FALSE;
            render_target.SrcBlend = D3D12_BLEND_ONE;
            render_target.DestBlend = D3D12_BLEND_ZERO;
            render_target.BlendOp = D3D12_BLEND_OP_ADD;
            render_target.SrcBlendAlpha = D3D12_BLEND_ONE;
            render_target.DestBlendAlpha = D3D12_BLEND_ZERO;
            render_target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            render_target.LogicOp = D3D12_LOGIC_OP_NOOP;
            render_target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
    }
};

struct CD3DX12_DEPTH_STENCIL_DESC : public D3D12_DEPTH_STENCIL_DESC
{
    CD3DX12_DEPTH_STENCIL_DESC()
    {
        DepthEnable = TRUE;
        DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        StencilEnable = FALSE;
        StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
            D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        BackFace = FrontFace;
    }
};

struct CD3DX12_CLEAR_VALUE : public D3D12_CLEAR_VALUE
{
    explicit CD3DX12_CLEAR_VALUE(DXGI_FORMAT format, float depth)
    {
        Format = format;
        DepthStencil.Depth = depth;
        DepthStencil.Stencil = 0;
    }
};

struct CD3DX12_GRAPHICS_PIPELINE_STATE_DESC : public D3D12_GRAPHICS_PIPELINE_STATE_DESC
{
    CD3DX12_GRAPHICS_PIPELINE_STATE_DESC()
        : D3D12_GRAPHICS_PIPELINE_STATE_DESC{}
    {
        RasterizerState = CD3DX12_RASTERIZER_DESC();
        BlendState = CD3DX12_BLEND_DESC();
        DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC();
        SampleMask = UINT_MAX;
        SampleDesc.Count = 1;
    }
};
