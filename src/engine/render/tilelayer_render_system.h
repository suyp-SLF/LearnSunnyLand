#pragma once
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace engine::core { class Context; }
namespace engine::component { class TilelayerComponent; }

namespace engine::render
{
    class TilelayerRenderSystem
    {
    private:
        // 核心：存储所有存活的精灵组件指针
        // 在内存中连续排列，对 CPU 缓存极其友好
        std::vector<engine::component::TilelayerComponent*> _tilelayers;
        std::unordered_map<engine::component::TilelayerComponent*, size_t> _tilelayerIndex;

    public:
        TilelayerRenderSystem()
        {
            _tilelayers.reserve(64);
            _tilelayerIndex.reserve(64);
        }
        ~TilelayerRenderSystem() = default;

        // 禁止拷贝
        TilelayerRenderSystem(const TilelayerRenderSystem&) = delete;
        TilelayerRenderSystem& operator=(const TilelayerRenderSystem&) = delete;

        // 注册与注销逻辑
        void registerComponent(engine::component::TilelayerComponent* tilelayer) {
            if (!tilelayer)
                return;
            if (_tilelayerIndex.find(tilelayer) != _tilelayerIndex.end())
                return;
            _tilelayerIndex[tilelayer] = _tilelayers.size();
            _tilelayers.push_back(tilelayer);
        }

        void unregisterComponent(engine::component::TilelayerComponent* tilelayer) {
            if (!tilelayer)
                return;

            auto it = _tilelayerIndex.find(tilelayer);
            if (it == _tilelayerIndex.end())
                return;

            const size_t idx = it->second;
            const size_t lastIdx = _tilelayers.size() - 1;
            engine::component::TilelayerComponent* lastTilelayer = _tilelayers[lastIdx];
            if (idx != lastIdx)
            {
                _tilelayers[idx] = lastTilelayer;
                _tilelayerIndex[lastTilelayer] = idx;
            }
            _tilelayers.pop_back();
            _tilelayerIndex.erase(it);
        }

        // 高性能批量渲染函数
        void renderAll(engine::core::Context& ctx);
    };
}