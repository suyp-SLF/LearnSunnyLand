#include "sprite_component.h"
#include "./transform_component.h"
#include "../resource/resource_manager.h"
#include "../utils/alignment.h"
#include "../object/game_object.h"
#include "../core/context.h"
#include "../render/renderer.h"
#include "../render/sprite_render_system.h" // 必须包含，用于注册逻辑
#include <spdlog/spdlog.h>

namespace engine::component
{
    SpriteComponent::SpriteComponent(const std::string &texture_id,
                                     engine::utils::Alignment alignment,
                                     std::optional<engine::utils::FRect> source_rect_opt,
                                     bool is_flipped)
        : _sprite(texture_id, source_rect_opt, is_flipped),
          _alignment(alignment)
    {
        spdlog::trace("创建SpriteComponent，纹理ID: {}", texture_id);
    }

    // ⚡️ 必须实现析构，否则 System 会尝试访问已销毁的组件指针导致崩溃
    SpriteComponent::~SpriteComponent()
    {
        if (_context)
        {
            _context->getSpriteRenderSystem().unregisterComponent(this);
            spdlog::trace("SpriteComponent 已从渲染系统中注销");
        }
    }

    void SpriteComponent::init()
    {
        // _owner 和 _context 通常在 GameObject::addComponent 时通过 attach 注入
        if (!_owner || !_context)
        {
            spdlog::error("SpriteComponent 初始化失败：_owner 或 _context 未绑定");
            return;
        }

        // 1. 获取（或自动添加）Transform 组件并缓存指针
        _transform_comp = _owner->getComponent<TransformComponent>();
        if (!_transform_comp)
        {
            _transform_comp = _owner->addComponent<TransformComponent>();
        }

        // 2. ⚡️ 向 Context 里的渲染系统注册自己
        _context->getSpriteRenderSystem().registerComponent(this);

        // 3. 初始化数据
        updateSpriteSize();
        updateOffset();
    }

    void SpriteComponent::update(float delta_time)
    {
        if (_transform_comp && _transform_comp->getVersion() != _last_transform_version) {
        updateOffset();
        _last_transform_version = _transform_comp->getVersion();
    }
    }

    void SpriteComponent::setAlignment(engine::utils::Alignment archor)
    {
        _alignment = archor;
        updateOffset();
    }

    void SpriteComponent::updateOffset()
    {
        // 如果 Transform 不存在或尺寸非法，不进行计算
        if (!_transform_comp || _sprite_size.x <= 0 || _sprite_size.y <= 0)
        {
            _offset = {0.0f, 0.0f};
            return;
        }

        const glm::vec2& scale = _transform_comp->getScale();
        
        // 基于锚点(Alignment)计算偏移量
        // 原理：将渲染位置根据对齐方式偏移到正确的位置
        switch (_alignment)
        {
        case engine::utils::Alignment::CENTER:
            _offset = glm::vec2(-_sprite_size.x * scale.x / 2.0f, -_sprite_size.y * scale.y / 2.0f);
            break;
        case engine::utils::Alignment::TOP_LEFT:
            _offset = glm::vec2(0.0f, 0.0f);
            break;
        case engine::utils::Alignment::TOP_RIGHT:
            _offset = glm::vec2(-_sprite_size.x * scale.x, 0.0f);
            break;
        case engine::utils::Alignment::BOTTOM_LEFT:
            _offset = glm::vec2(0.0f, -_sprite_size.y * scale.y);
            break;
        case engine::utils::Alignment::BOTTOM_RIGHT:
            _offset = glm::vec2(-_sprite_size.x * scale.x, -_sprite_size.y * scale.y);
            break;
        case engine::utils::Alignment::TOP_CENTER:
            _offset = glm::vec2(-_sprite_size.x * scale.x / 2.0f, 0.0f);
            break;
        case engine::utils::Alignment::BOTTOM_CENTER:
            _offset = glm::vec2(-_sprite_size.x * scale.x / 2.0f, -_sprite_size.y * scale.y);
            break;
        case engine::utils::Alignment::CENTER_LEFT:
            _offset = glm::vec2(0.0f, -_sprite_size.y * scale.y / 2.0f);
            break;
        case engine::utils::Alignment::CENTER_RIGHT:
            _offset = glm::vec2(-_sprite_size.x * scale.x, -_sprite_size.y * scale.y / 2.0f);
            break;
        case engine::utils::Alignment::NONE:
        default:
            _offset = {0.0f, 0.0f};
            break;
        }
    }

    void SpriteComponent::setSpriteById(const std::string &texture_id, std::optional<engine::utils::FRect> source_rect_opt)
    {
        _sprite.setTextureId(texture_id);
        _sprite.setSourceRect(source_rect_opt);

        updateSpriteSize();
        updateOffset();
    }

    void SpriteComponent::setSourceRect(const std::optional<engine::utils::FRect> &source_rect_opt)
    {
        _sprite.setSourceRect(source_rect_opt);
        updateSpriteSize();
        updateOffset(); // 区域变了，偏移量必须重新计算
    }

    void SpriteComponent::updateSpriteSize()
    {
        auto source_rect_opt = _sprite.getSourceRect();
        if (source_rect_opt.has_value())
        {
            _sprite_size = source_rect_opt->size;
        }
        else
        {
            // ✅ 使用成员变量 _context 访问资源管理器
            if (_context) {
                _sprite_size = _context->getResourceManager().getTextureSize(_sprite.getTextureId());
            }
        }
    }
}