#include "sprite_component.h"
#include "./transform_component.h"
#include "../resource/resource_manager.h"
#include "../utils/alignment.h"
#include "../object/game_object.h"
#include "../core/context.h"
#include "../render/renderer.h"
#include <spdlog/spdlog.h>

namespace engine::component
{
    // SpriteComponent.cpp
    SpriteComponent::SpriteComponent(const std::string &texture_id,
                                     engine::resource::ResourceManager &resource_manager,
                                     engine::utils::Alignment alignment, // 删掉下划线前缀，避免混淆
                                     std::optional<engine::utils::FRect> source_rect_opt,
                                     bool is_flipped)
        : _resource_manager(&resource_manager), // 必须取地址，因为成员是指针
          _sprite(texture_id, source_rect_opt, is_flipped),
          _alignment(alignment)
    {
        if (!_resource_manager)
        {
            spdlog::critical("创建SpriteComponent时，_resource_manager为空指针");
        }
        spdlog::trace("创建SpriteComponent，纹理ID: {}", texture_id);
    }

    void SpriteComponent::init()
    {
        if (!_owner)
        {
            spdlog::error("SpriteComponent的_owner为空指针");
            return;
        }
        _transform_comp = _owner->addComponent<TransformComponent>();
        if (!_transform_comp)
        {
            spdlog::warn("GameObject{}上的SpriteComponent需要一个TransformComponent，但未找到", _owner->getName());
            return;
        }

        // 更新大小以及偏移量
        updateSpriteSize();
        updateOffset();
    }

    void SpriteComponent::render(engine::core::Context &context)
    {
        if (_is_hidden || !_transform_comp || !_resource_manager)
        {
            return;
        }

        // 检查 Transform 是否变动过
        if (_transform_comp->getVersion() != _last_transform_version)
        {
            updateOffset();
            _last_transform_version = _transform_comp->getVersion();
            spdlog::trace("Transform 版本变更，Sprite 更新偏移量");
        }
        
        // 获得变换信息（考虑偏移量）
        const glm::vec2 &pos = _transform_comp->getPosition() + _offset;
        const glm::vec2 &scale = _transform_comp->getScale();
        float roatation_degrees = _transform_comp->getRotation();

        // 渲染精灵
        context.getRenderer().drawSprite(context.getCamera(), _sprite, pos, scale, roatation_degrees);
    }

    void SpriteComponent::setAlignment(engine::utils::Alignment archor)
    {
        _alignment = archor;
        // 更新锚点的时候更新偏移量
        updateOffset();
    }
    void SpriteComponent::updateOffset()
    {
        // 如果尺寸大小为0，则不更新偏移量
        if (!_transform_comp || _sprite_size.x <= 0 || _sprite_size.y <= 0)
        {
            _offset = {0.0f, 0.0f};
            return;
        }
        auto scale = _transform_comp->getScale();
        // 计算偏移量
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
            break;
        }
    }
    void SpriteComponent::setSpriteById(const std::string &texture_id, std::optional<engine::utils::FRect> _source_rect_opt)
    {
        _sprite.setTextureId(texture_id);
        _sprite.setSourceRect(_source_rect_opt);

        updateSpriteSize();
        updateOffset();
    }

    void SpriteComponent::setSourceRect(const std::optional<engine::utils::FRect> &source_rect_opt)
    {
        _sprite.setSourceRect(source_rect_opt);
        updateSpriteSize();
    }
    void SpriteComponent::updateSpriteSize()
    {
        if (!_resource_manager)
        {
            spdlog::error("SpriteComponent的_resource_manager为空指针");
            return;
        }
        auto source_rect_opt = _sprite.getSourceRect();
        if (source_rect_opt.has_value())
        {
            _sprite_size = source_rect_opt->size;
        }
        else
        {
            _sprite_size = _resource_manager->getTextureSize(_sprite.getTextureId());
        }
    }
}