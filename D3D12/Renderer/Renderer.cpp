#include "../Common/stdafx.h"
#include "Renderer.h"

namespace
{
    bool show_initialization_failure(const wchar_t* step)
    {
        MessageBoxW(nullptr, step, L"D3D12 initialization failed", MB_OK | MB_ICONERROR);
        return false;
    }
}

bool RENDERER::initialize(HINSTANCE instance, int show_command)
{
    if (!_window.initialize(instance, show_command)) return show_initialization_failure(L"WINDOW");
    if (!_device.initialize()) return show_initialization_failure(L"DEVICE");
    if (!_command.initialize(_device.get_device())) return show_initialization_failure(L"COMMAND");
    if (!_swap_chain.initialize(_device.get_factory(), _device.get_device(),
        _command.get_queue(), _window.get_window())) return show_initialization_failure(L"SWAP_CHAIN");
    if (!_pipeline.initialize(_device.get_device())) return show_initialization_failure(L"PIPELINE");
    if (!_resource.initialize(_device.get_device())) return show_initialization_failure(L"RESOURCE");
    if (!_resource.create_depth_stencil_view(_device.get_device())) return show_initialization_failure(L"DEPTH_STENCIL");
    return true;
}

int RENDERER::run()
{
    while (_window.process_messages())
    {
        render_frame();
    }
    return 0;
}

void RENDERER::render_frame()
{
    _command.begin();
    ID3D12GraphicsCommandList* command_list = _command.get_list();
    const UINT frame_index = _swap_chain.get_frame_index();
    const auto to_render_target = CD3DX12_RESOURCE_BARRIER::transition(
        _swap_chain.get_render_target_resource(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    command_list->ResourceBarrier(1, &to_render_target);

    const auto render_target = _swap_chain.get_render_target();
    const auto depth_stencil = _resource.get_depth_stencil();
    command_list->OMSetRenderTargets(1, &render_target, FALSE, &depth_stencil);
    constexpr float clear_color[] = { 0.05f, 0.05f, 0.08f, 1.0f };
    command_list->ClearRenderTargetView(render_target, clear_color, 0, nullptr);
    command_list->ClearDepthStencilView(depth_stencil, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT scissor{ 0, 0, 1280, 720 };
    command_list->RSSetViewports(1, &viewport);
    command_list->RSSetScissorRects(1, &scissor);
    command_list->SetGraphicsRootSignature(_pipeline.get_root_signature());

    const MATH::MATRIX4X4 view = MATH::matrix_look_at_left_handed(
        { 8.0f, 7.0f, -8.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
    const MATH::MATRIX4X4 projection = MATH::matrix_perspective_left_handed(
        MATH::pi / 3.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
    const MATH::MATRIX4X4 view_projection = MATH::multiply(view, projection);
    _resource.update_transforms(
        view_projection,
        MATH::multiply(MATH::matrix_translation({ 0.0f, 1.0f, 0.0f }), view_projection));

    command_list->SetPipelineState(_pipeline.get_grid_pipeline());
    command_list->SetGraphicsRootConstantBufferView(0, _resource.get_constant_address(0));
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    const auto grid_view = _resource.get_grid_vertex_view();
    command_list->IASetVertexBuffers(0, 1, &grid_view);
    command_list->DrawInstanced(grid_view.SizeInBytes / grid_view.StrideInBytes, 1, 0, 0);

    command_list->SetPipelineState(_pipeline.get_cube_pipeline());
    command_list->SetGraphicsRootConstantBufferView(0, _resource.get_constant_address(1));
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const auto cube_view = _resource.get_cube_vertex_view();
    const auto index_view = _resource.get_cube_index_view();
    command_list->IASetVertexBuffers(0, 1, &cube_view);
    command_list->IASetIndexBuffer(&index_view);
    command_list->DrawIndexedInstanced(36, 1, 0, 0, 0);

    const auto to_present = CD3DX12_RESOURCE_BARRIER::transition(
        _swap_chain.get_render_target_resource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    command_list->ResourceBarrier(1, &to_present);
    _command.execute();
    _swap_chain.present();
    _command.wait_for_gpu();
}
