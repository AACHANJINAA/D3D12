#pragma once

#include "../Common/stdafx.h"

class SWAP_CHAIN
{
public:
    static constexpr UINT frame_count = 2;

    bool initialize(IDXGIFactory4* factory, ID3D12Device* device,
        ID3D12CommandQueue* queue, HWND window);
    bool present();
    UINT get_frame_index() const;
    D3D12_CPU_DESCRIPTOR_HANDLE get_render_target() const;
    ID3D12Resource* get_render_target_resource() const;
    UINT get_descriptor_size() const;

private:
    ComPtr<IDXGISwapChain3> _swap_chain;
    ComPtr<ID3D12DescriptorHeap> _render_target_heap;
    std::array<ComPtr<ID3D12Resource>, frame_count> _render_targets;
    UINT _descriptor_size = 0;
    UINT _frame_index = 0;
};
