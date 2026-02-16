#pragma once
#include <vector>
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

    public:
        ParallaxRenderSystem() = default;
        ~ParallaxRenderSystem() = default;
        // 禁止拷贝
        ParallaxRenderSystem(const ParallaxRenderSystem &) = delete;
        ParallaxRenderSystem &operator=(const ParallaxRenderSystem &) = delete;
        // 注册与注销逻辑
        void registerComponent(engine::component::ParallaxComponent *parallax)
        {
            _parallaxs.push_back(parallax);
        }

        void unregisterComponent(engine::component::ParallaxComponent *parallax)
        {
            _parallaxs.erase(std::remove(_parallaxs.begin(), _parallaxs.end(), parallax), _parallaxs.end());
        }
        // 高性能批量渲染函数
        void renderAll(engine::core::Context &ctx);
    };
} // namespace engine::render
