// G-buffer depth testing leaves only the visible selected surfaces in normal.a.
bool is_selection_outline(int2 pixel)
{
    if (gbuffer_normal.Load(int3(pixel, 0)).a < 0.5f) return false;
    uint width, height;
    gbuffer_normal.GetDimensions(width, height);
    const int2 offsets[8] = {
        int2(-2, 0), int2(2, 0), int2(0, -2), int2(0, 2),
        int2(-1, -1), int2(1, -1), int2(-1, 1), int2(1, 1)
    };
    [unroll]
    for (int index = 0; index < 8; ++index)
    {
        int2 neighbor = pixel + offsets[index];
        if (any(neighbor < 0) || any(neighbor >= int2(width, height))) return true;
        if (gbuffer_normal.Load(int3(neighbor, 0)).a < 0.5f) return true;
    }
    return false;
}
