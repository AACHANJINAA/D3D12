#pragma once

#include "../Common/stdafx.h"

class COMMAND
{
public:
    ~COMMAND();
    COMMAND() = default;
    COMMAND(const COMMAND&) = delete;
    COMMAND& operator=(const COMMAND&) = delete;
    bool initialize(ID3D12Device* device);
    void begin();
    void execute();
    void wait_for_gpu();
    ID3D12CommandQueue* get_queue() const;
    ID3D12GraphicsCommandList* get_list() const;

private:
    ComPtr<ID3D12CommandQueue> _queue;
    ComPtr<ID3D12CommandAllocator> _allocator;
    ComPtr<ID3D12GraphicsCommandList> _list;
    ComPtr<ID3D12Fence> _fence;
    UINT64 _fence_value = 0;
    HANDLE _fence_event = nullptr;
};
