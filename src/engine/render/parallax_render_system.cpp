#include "parallax_render_system.h"
#include "../component/parallax_component.h"
#include "../component/transform_component.h"
#include "../core/context.h"
#include "renderer.h"
#include "camera.h"

namespace engine::render
{
    void ParallaxRenderSystem::renderAll(engine::core::Context &ctx)
    {
        auto &camera = ctx.getCamera();

        // 💡 提示：如果背景层级很多，可以考虑在这里按 Transform 的 Z 轴或手动定义的 Layer 排序
        // std::sort(_parallaxs.begin(), _parallaxs.end(), ...);

        for (auto *comp : _parallaxs)
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

            // 3. ⚡️ 视口剔除 (Frustum Culling) - 仅对非重复层生效
            // repeat.x/y 为 true 时，精灵在世界坐标出界后仍通过视差公式平铺整屏，不能用
            // 世界空间 AABB 来判断可见性——否则一旦相机向右移动超过精灵宽度就会被错误剔除。
            const glm::bvec2 repeat = comp->getRepeat();
            if (!repeat.x && !repeat.y)
            {
                if (!camera.isBoxInView(transform->getPosition(), comp->getSprite().getSize())) continue;
            }

            // 4. 调用组件自身的 draw（或者直接在这里调用 renderer）
            // 建议统一调用 comp->draw(ctx)，因为组件最清楚自己的 offset 怎么加
            comp->draw(ctx);
        }
    }
}