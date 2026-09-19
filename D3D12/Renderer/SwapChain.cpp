#include "../Common/stdafx.h"
#include "SwapChain.h"

bool SWAP_CHAIN::initialize(IDXGIFactory4* factory, ID3D12Device* device,
    ID3D12CommandQueue* queue, HWND window)
{
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.BufferCount = frame_count;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    description.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swap_chain;
    if (FAILED(factory->CreateSwapChainForHwnd(queue, window, &description,
        nullptr, nullptr, &swap_chain)) ||
        FAILED(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER)) ||
        FAILED(swap_chain.As(&_swap_chain))) return false;

    const CD3DX12_DESCRIPTOR_HEAP_DESC heap_description(frame_count, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (FAILED(device->CreateDescriptorHeap(&heap_description, IID_PPV_ARGS(&_render_target_heap)))) return false;
    _descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    auto handle = _render_target_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT index = 0; index < frame_count; ++index)
    {
        if (FAILED(_swap_chain->GetBuffer(index, IID_PPV_ARGS(&_render_targets[index])))) return false;
        device->CreateRenderTargetView(_render_targets[index].Get(), nullptr, handle);
        handle.ptr += _descriptor_size;
    }
    _frame_index = _swap_chain->GetCurrentBackBufferIndex();
    return true;
}

bool SWAP_CHAIN::present()
{
    if (FAILED(_swap_chain->Present(1, 0))) return false;
    _frame_index = _swap_chain->GetCurrentBackBufferIndex();
    return true;
}

UINT SWAP_CHAIN::get_frame_index() const { return _frame_index; }

D3D12_CPU_DESCRIPTOR_HANDLE SWAP_CHAIN::get_render_target() const
{
    auto handle = _render_target_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(_frame_index) * _descriptor_size;
    return handle;
}

ID3D12Resource* SWAP_CHAIN::get_render_target_resource() const
{
    return _render_targets[_frame_index].Get();
}

UINT SWAP_CHAIN::get_descriptor_size() const { return _descriptor_size; }
