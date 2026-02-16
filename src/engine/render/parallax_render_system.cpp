#include "parallax_render_system.h"
#include "../component/parallax_component.h"
#include "../component/transform_component.h"
#include "../core/context.h"
#include "renderer.h"
#include "camera.h"

namespace engine::render
{
    /**
     * @brief 渲染所有可见的视差背景组件
     *
     * 该方法负责批量渲染场景中的所有视差背景组件，包括：
     * 1. 获取渲染后端和相机引用
     * 2. 遍历所有视差组件进行渲染
     * 3. 对每个组件进行可见性检查和变换计算
     * 4. 通过抽象渲染接口执行实际绘制，支持滚动因子与重复平铺
     *
     * @param ctx 引擎上下文对象，提供渲染器和相机访问
     *
     * @note 该方法会跳过隐藏的视差组件和没有变换组件的实体
     * @note scroll_factor 控制背景相对于相机移动的速度比例（0=固定，1=随相机同步移动）
     * @note repeat 控制纹理在 X/Y 方向上是否无限平铺
     * @note 通过抽象渲染接口实现跨平台渲染（SDL/Vulkan等）
     */
    void ParallaxRenderSystem::renderAll(engine::core::Context &ctx)
    {
        auto &renderer = ctx.getRenderer();
        auto &camera = ctx.getCamera();

        for (auto *comp : _parallaxs)
        {
            if (comp->isHidden()) continue;

            auto *transform = comp->getTransformComp();
            if (!transform) continue;

            const glm::vec2 &pos = transform->getPosition();
            const glm::vec2 &scale = transform->getScale();
            double rotation = static_cast<double>(transform->getRotation());

            renderer.drawParallax(
                camera,
                comp->getSprite(),
                pos,
                comp->getScrollFactor(),
                comp->getRepeat(),
                scale,
                rotation
            );
        }
    }
}
