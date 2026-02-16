#include "parallax_component.h"
#include "transform_component.h"
#include "../object/game_object.h"
#include "../render/sprite.h"
#include <spdlog/spdlog.h>

namespace engine::component
{
    /**
     * @brief 构造函数
     * @param texture_id 纹理资源 ID
     * @param scroll_factor 滚动因子（0=静止，1=随相机同步，<1=远景，>1=近景）
     * @param repeat 是否在 X 或 Y 轴上进行无限重复填充
     */
    ParallaxComponent::ParallaxComponent(const std::string &texture_id, 
                                         const glm::vec2 &scroll_factor, 
                                         glm::bvec2 repeat)
        : _sprite(texture_id), 
          _scroll_factor(scroll_factor), 
          _repeat(repeat)
    {
        spdlog::trace("创建 ParallaxComponent，纹理: {}", texture_id);
    }

    /**
     * @brief 初始化组件
     * 职责：确保关联到 Transform，并验证宿主对象状态。
     */
    void ParallaxComponent::init()
    {
        if (!_owner)
        {
            spdlog::error("ParallaxComponent 初始化失败：未绑定 GameObject");
            return;
        }

        // 自动获取 Transform 组件引用
        _transform_comp = _owner->getComponent<TransformComponent>();
        if (!_transform_comp)
        {
            // 对于视差背景，没有 Transform 意味着无法确定基础坐标
            spdlog::error("ParallaxComponent 警告：实体 [{}] 缺少 TransformComponent", _owner->getName());
        }
    }

    /**
     * @brief 每一帧的逻辑更新
     * @note 视差偏移通常依赖于 Camera 位置，该逻辑建议放在专用的 ParallaxRenderSystem 中。
     * 这里的 update 保持为空，符合“去逻辑化”的高性能设计。
     */
    void ParallaxComponent::update(float /*delta_time*/)
    {
        // 预留：如果将来需要实现“自发性滚动”（如自动流动的云），可以在此计算累加偏移。
    }
} // namespace engine::component