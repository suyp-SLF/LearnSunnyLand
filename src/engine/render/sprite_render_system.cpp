#include "sprite_render_system.h"
#include "../component/sprite_component.h"
#include "../component/transform_component.h"
#include "../core/context.h"
#include "renderer.h"
#include "camera.h"

namespace engine::render
{
    void SpriteRenderSystem::renderAll(engine::core::Context& ctx)
    {
        // 1. 获取渲染后端和相机的引用（通过你刚才定义的抽象 Renderer）
        auto& renderer = ctx.getRenderer();
        auto& camera = ctx.getCamera();

        // 2. 批量遍历精灵（线性内存访问）
        for (auto* comp : _sprites)
        {
            // 3. 基础状态过滤
            if (comp->isHidden()) continue;

            auto* transform = comp->getTransformComp();
            if (!transform) continue;

            // 4. 计算变换后的位置（考虑偏移）
            const glm::vec2& pos = transform->getPosition() + comp->getOffset();
            const glm::vec2& scale = transform->getScale();
            double rotation = static_cast<double>(transform->getRotation());

            // 5. 调用抽象接口进行绘制
            // 不管底层是 SDL 还是 Vulkan，这里只需要调用这一行
            renderer.drawSprite(
                camera, 
                comp->getSprite(), 
                pos, 
                scale, 
                rotation
            );
        }
    }
}