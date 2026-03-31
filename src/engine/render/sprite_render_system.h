#pragma once
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace engine::core { class Context; }
namespace engine::component { class SpriteComponent; }

namespace engine::render
{
    class SpriteRenderSystem
    {
    private:
        // 核心：存储所有存活的精灵组件指针
        // 在内存中连续排列，对 CPU 缓存极其友好
        std::vector<engine::component::SpriteComponent*> _sprites;
        std::unordered_map<engine::component::SpriteComponent*, size_t> _spriteIndex;

    public:
        SpriteRenderSystem()
        {
            _sprites.reserve(256);
            _spriteIndex.reserve(256);
        }
        ~SpriteRenderSystem() = default;

        // 禁止拷贝
        SpriteRenderSystem(const SpriteRenderSystem&) = delete;
        SpriteRenderSystem& operator=(const SpriteRenderSystem&) = delete;

        // 注册与注销逻辑
        void registerComponent(engine::component::SpriteComponent* sprite) {
            if (!sprite)
                return;
            if (_spriteIndex.find(sprite) != _spriteIndex.end())
                return;
            _spriteIndex[sprite] = _sprites.size();
            _sprites.push_back(sprite);
        }

        void unregisterComponent(engine::component::SpriteComponent* sprite) {
            if (!sprite)
                return;

            auto it = _spriteIndex.find(sprite);
            if (it == _spriteIndex.end())
                return;

            const size_t idx = it->second;
            const size_t lastIdx = _sprites.size() - 1;
            engine::component::SpriteComponent* lastSprite = _sprites[lastIdx];
            if (idx != lastIdx)
            {
                _sprites[idx] = lastSprite;
                _spriteIndex[lastSprite] = idx;
            }
            _sprites.pop_back();
            _spriteIndex.erase(it);
        }

        // 高性能批量渲染函数
        void renderAll(engine::core::Context& ctx);
    };
}