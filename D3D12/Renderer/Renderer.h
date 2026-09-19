#pragma once

#include "../Common/stdafx.h"
#include "Command.h"
#include "Device.h"
#include "Pipeline.h"
#include "Resource/Resource.h"
#include "SwapChain.h"
#include "Window.h"

class RENDERER
{
public:
    RENDERER() = default;
    ~RENDERER() = default;
    RENDERER(const RENDERER&) = delete;
    RENDERER& operator=(const RENDERER&) = delete;

    bool initialize(HINSTANCE instance, int show_command);
    int run();

private:
    void render_frame();

    WINDOW _window;
    DEVICE _device;
    COMMAND _command;
    SWAP_CHAIN _swap_chain;
    PIPELINE _pipeline;
    RESOURCE _resource;
};
