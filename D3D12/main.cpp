#include "Renderer.h"

#include <cstdlib>

int main()
{
    RENDERER renderer;

    if (!renderer.initialize(GetModuleHandleW(nullptr), SW_SHOWDEFAULT))
    {
        return EXIT_FAILURE;
    }

    return renderer.run();
}
