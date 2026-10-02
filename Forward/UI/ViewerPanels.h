#pragma once

#include "../Renderer/ViewerSettings.h"
#include "../Scene/Scene.h"

enum class VIEWER_ACTION { none, open, add, erase, focus };
struct VIEWER_REQUEST
{
    VIEWER_ACTION action = VIEWER_ACTION::none;
    UINT64 object_id = 0;
    std::filesystem::path path;
    MATH::VECTOR3 position{};
    MATH::VECTOR3 rotation{};
    MATH::VECTOR3 scale{ 1, 1, 1 };

    bool has_valid_transform() const
    {
        const auto isfinite = [](const MATH::VECTOR3& value, float limit)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
                std::abs(value.x) <= limit && std::abs(value.y) <= limit && std::abs(value.z) <= limit;
        };
        return isfinite(position, 1000000) && isfinite(rotation, 360000) && isfinite(scale, 10000) &&
            scale.x >= 0.0001f && scale.y >= 0.0001f && scale.z >= 0.0001f;
    }
};

struct VIEWER_STATS
{
    size_t model_count = 0;
    size_t texture_count = 0;
};

class VIEWER_PANELS
{
public:
    void draw(VIEWER_SETTINGS& settings, SCENE& scene, const VIEWER_STATS& stats,
        const std::array<UINT64, 5>& textures);
    VIEWER_REQUEST take_request() { auto request = std::move(_request); _request = {}; return request; }

private:
    VIEWER_REQUEST _request;
    bool _is_scene = true;
    bool _is_inspector = true;
    bool _is_display = true;
    bool _is_light = true;
    bool _is_assets = true;
    bool _is_status = true;
    bool _is_hidden = false;
    int _transform_tab = 0;
    bool _is_material_tab = false;
    bool _is_texture_tab = false;
    int _texture_index = 0;
};
