#include "sprite_render_system.h"
#include "../component/sprite_component.h"
#include "../component/transform_component.h"
#include "../core/context.h"
#include "renderer.h"
#include "camera.h"

namespace engine::render
{
    /**
     * @brief 渲染所有可见的精灵组件
     *
     * 该方法负责批量渲染场景中的所有精灵组件，包括：
     * 1. 获取渲染后端和相机引用
     * 2. 遍历所有精灵组件进行渲染
     * 3. 对每个精灵进行可见性检查和变换计算
     * 4. 通过抽象渲染接口执行实际绘制
     *
     * @param ctx 引擎上下文对象，提供渲染器和相机访问
     *
     * @note 该方法会跳过隐藏的精灵组件和没有变换组件的精灵
     * @note 渲染时考虑了精灵的偏移、缩放和旋转变换
     * @note 通过抽象渲染接口实现跨平台渲染（SDL/Vulkan等）
     */
    void SpriteRenderSystem::renderAll(engine::core::Context &ctx)
    {
        // 1. 获取渲染后端和相机的引用（通过你刚才定义的抽象 Renderer）
        auto &renderer = ctx.getRenderer();
        auto &camera = ctx.getCamera();

        // ⚡️ 优化 A：渲染前排序 (Depth Sorting)
        // 如果不排序，后生成的对象永远在最前面，无法处理遮挡关系
        // std::sort(_sprites.begin(), _sprites.end(), [](auto* a, auto* b) {
        //     // 假设你在 SpriteComponent 里有个 getZIndex()
        //     return a->getZIndex() < b->getZIndex();
        // });
        // 2. 批量遍历精灵（线性内存访问）
        for (auto *comp : _sprites)
        {
            // 1. 基础状态过滤
            if (!comp || comp->isHidden())
                continue;

            // 2. ⚡️ 核心：按需更新（把原本 update 里的逻辑搬到这）
            // 内部会检查 version 和 dirty_flags
            comp->ensureResourcesReady();

            auto *transform = comp->getTransformComp();
            if (!transform)
                continue;

            // 3. ⚡️ 视口剔除 (Frustum Culling) - 可选
            // 如果精灵在相机范围外，直接跳过 draw 调用，节省 GPU/CPU 开销
            if (!camera.isBoxInView(transform->getPosition(), comp->getSpriteSize())) continue;

            // 4. 调用组件自身的 draw（或者直接在这里调用 renderer）
            // 建议统一调用 comp->draw(ctx)，因为组件最清楚自己的 offset 怎么加
            comp->draw(ctx);
        }
    }
}