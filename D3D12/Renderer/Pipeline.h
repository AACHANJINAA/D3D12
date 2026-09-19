#pragma once

#include "../Common/stdafx.h"

class PIPELINE
{
public:
    bool initialize(ID3D12Device* device);
    ID3D12RootSignature* get_root_signature() const;
    ID3D12PipelineState* get_grid_pipeline() const;
    ID3D12PipelineState* get_cube_pipeline() const;

private:
    ComPtr<ID3D12RootSignature> _root_signature;
    ComPtr<ID3D12PipelineState> _grid_pipeline;
    ComPtr<ID3D12PipelineState> _cube_pipeline;
};
