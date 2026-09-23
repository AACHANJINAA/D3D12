#pragma once

#include "../Common/stdafx.h"

class CUBE_PSO
{
public:
    bool initialize(ID3D12Device* device);
    ID3D12RootSignature* get_root_signature() const;
    ID3D12PipelineState* get_pipeline() const;
    ID3D12PipelineState* get_wireframe_pipeline() const;

private:
    ComPtr<ID3D12RootSignature> _root_signature;
    ComPtr<ID3D12PipelineState> _pipeline;
    ComPtr<ID3D12PipelineState> _wireframe_pipeline;
};

class PIPELINE
{
public:
    bool initialize(ID3D12Device* device);
    ID3D12RootSignature* get_root_signature() const;
    ID3D12PipelineState* get_pipeline() const;
    ID3D12PipelineState* get_wireframe_pipeline() const;

private:
    CUBE_PSO _cube_pso;
};
