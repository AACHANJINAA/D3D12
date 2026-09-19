#include "../Common/stdafx.h"
#include "Command.h"

COMMAND::~COMMAND()
{
    wait_for_gpu();
    if (_fence_event != nullptr) CloseHandle(_fence_event);
}

bool COMMAND::initialize(ID3D12Device* device)
{
    const CD3DX12_COMMAND_QUEUE_DESC description(D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (FAILED(device->CreateCommandQueue(&description, IID_PPV_ARGS(&_queue))) ||
        FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_allocator))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _allocator.Get(), nullptr,
            IID_PPV_ARGS(&_list))) || FAILED(_list->Close()) ||
        FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)))) return false;
    _fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return _fence_event != nullptr;
}

void COMMAND::begin()
{
    _allocator->Reset();
    _list->Reset(_allocator.Get(), nullptr);
}

void COMMAND::execute()
{
    _list->Close();
    ID3D12CommandList* lists[] = { _list.Get() };
    _queue->ExecuteCommandLists(1, lists);
}

void COMMAND::wait_for_gpu()
{
    if (_queue == nullptr || _fence == nullptr || _fence_event == nullptr) return;
    const UINT64 value = ++_fence_value;
    _queue->Signal(_fence.Get(), value);
    _fence->SetEventOnCompletion(value, _fence_event);
    WaitForSingleObject(_fence_event, INFINITE);
}

ID3D12CommandQueue* COMMAND::get_queue() const { return _queue.Get(); }
ID3D12GraphicsCommandList* COMMAND::get_list() const { return _list.Get(); }
