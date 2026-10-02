#include "../Common/stdafx.h"
#include "../Renderer/Renderer.h"

int main()
{
    RENDERER renderer;

    if (!renderer.initialize(GetModuleHandleW(nullptr), SW_SHOWDEFAULT))
    {
        return EXIT_FAILURE;
    }

    return renderer.run();
}
