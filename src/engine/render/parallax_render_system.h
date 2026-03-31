#pragma once
#include <vector>
#include <unordered_map>
namespace engine::component
{
    class ParallaxComponent; // 声明 ParallaxComponent
}
namespace engine::core
{
    class Context; // 声明 Context
}
namespace engine::render
{
    class ParallaxRenderSystem
    {
    private:
        // 核心：存储所有存活的精灵组件指针
        // 在内存中连续排列，对 CPU 缓存极其友好
        std::vector<engine::component::ParallaxComponent *> _parallaxs;
        std::unordered_map<engine::component::ParallaxComponent *, size_t> _parallaxIndex;

    public:
        ParallaxRenderSystem()
        {
            _parallaxs.reserve(128);
            _parallaxIndex.reserve(128);
        }
        ~ParallaxRenderSystem() = default;
        // 禁止拷贝
        ParallaxRenderSystem(const ParallaxRenderSystem &) = delete;
        ParallaxRenderSystem &operator=(const ParallaxRenderSystem &) = delete;
        // 注册与注销逻辑
        void registerComponent(engine::component::ParallaxComponent *parallax)
        {
            if (!parallax)
                return;
            if (_parallaxIndex.find(parallax) != _parallaxIndex.end())
                return;
            _parallaxIndex[parallax] = _parallaxs.size();
            _parallaxs.push_back(parallax);
        }

        void unregisterComponent(engine::component::ParallaxComponent *parallax)
        {
            if (!parallax)
                return;

            auto it = _parallaxIndex.find(parallax);
            if (it == _parallaxIndex.end())
                return;

            const size_t idx = it->second;
            const size_t lastIdx = _parallaxs.size() - 1;
            engine::component::ParallaxComponent *lastParallax = _parallaxs[lastIdx];
            if (idx != lastIdx)
            {
                _parallaxs[idx] = lastParallax;
                _parallaxIndex[lastParallax] = idx;
            }
            _parallaxs.pop_back();
            _parallaxIndex.erase(it);
        }
        // 高性能批量渲染函数
        void renderAll(engine::core::Context &ctx);
    };
} // namespace engine::render
