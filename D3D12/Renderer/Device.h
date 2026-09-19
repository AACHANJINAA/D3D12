#pragma once

#include "../Common/stdafx.h"

class DEVICE
{
public:
    bool initialize();
    ID3D12Device* get_device() const;
    IDXGIFactory4* get_factory() const;

private:
    ComPtr<IDXGIFactory4> _factory;
    ComPtr<ID3D12Device> _device;
};
