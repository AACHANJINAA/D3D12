#pragma once

#include "../Renderer/ViewerSettings.h"

struct VIEWER_STATS
{
    UINT vertex_count = 0;
    UINT triangle_count = 0;
};

class VIEWER_PANELS
{
public:
    void draw(VIEWER_SETTINGS& settings, const VIEWER_STATS& stats,
        const std::array<UINT64, 5>& textures);

private:
    bool _is_scene = true;
    bool _is_inspector = true;
    bool _is_display = true;
    bool _is_light = true;
    bool _is_assets = true;
    bool _is_status = true;
    bool _is_hidden = false;
    bool _is_selected = true;
    int _transform_tab = 0;
    bool _is_material_tab = false;
    bool _is_texture_tab = false;
    int _texture_index = 0;
};
