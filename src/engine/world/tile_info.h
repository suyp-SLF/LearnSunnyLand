// tile_info.h
#pragma once

#include <string>
#include <glm/glm.hpp>

namespace engine::world
{
    /**
     * @brief 定义瓦片类型
     */
    enum class TileType : uint8_t
    {
        Air = 0,
        Stone = 1,
        Dirt = 2,
        Grass = 3,
    };

    /**
     * @brief 单个瓦片的渲染和逻辑信息
     */
    struct TileData
    {
        glm::vec4 uv_rect;      // 纹理坐标
        TileType type;          // 瓦片类型
        std::string texture_id; // 纹理ID（多图片模式）

        // 默认构造函数
        TileData() = default;

        // 新增：接受 TileType 的构造函数
        explicit TileData(TileType t)
            : type(t)
        {
            // 根据瓦片类型设置 UV 坐标（像素坐标）
            switch (t)
            {
            case TileType::Stone:
                uv_rect = glm::vec4(0.0f, 0.0f, 16.0f, 16.0f);
                break;
            case TileType::Dirt:
                uv_rect = glm::vec4(16.0f, 0.0f, 16.0f, 16.0f);
                break;
            case TileType::Grass:
                uv_rect = glm::vec4(32.0f, 0.0f, 16.0f, 16.0f);
                break;
            default:
                uv_rect = glm::vec4(0.0f, 0.0f, 16.0f, 16.0f);
                break;
            }
        }

        // 原有的三参数构造函数
        TileData(glm::vec4 uv_rect, TileType type, std::string texture_id)
            : uv_rect(uv_rect), type(type), texture_id(std::move(texture_id)) {}
    };

} // namespace engine::world