#pragma once
#include <vector>
#include <algorithm>

namespace engine::core { class Context; }
namespace engine::component { class SpriteComponent; }

namespace engine::render
{
    class SpriteRenderSystem
    {
    private:
        // ⚡️ 核心：存储所有存活的精灵组件指针
        // 在内存中连续排列，对 CPU 缓存极其友好
        std::vector<engine::component::SpriteComponent*> _sprites;

    public:
        SpriteRenderSystem() = default;
        ~SpriteRenderSystem() = default;

        // 禁止拷贝
        SpriteRenderSystem(const SpriteRenderSystem&) = delete;
        SpriteRenderSystem& operator=(const SpriteRenderSystem&) = delete;

        // 注册与注销逻辑
        void registerComponent(engine::component::SpriteComponent* sprite) {
            _sprites.push_back(sprite);
        }

        void unregisterComponent(engine::component::SpriteComponent* sprite) {
            _sprites.erase(std::remove(_sprites.begin(), _sprites.end(), sprite), _sprites.end());
        }

        // ⚡️ 高性能批量渲染函数
        void renderAll(engine::core::Context& ctx);
    };
}