#pragma once
#include "./component.h"
#include "../utils/alignment.h"
#include "../render/sprite.h"
#include "../utils/math.h"
#include <glm/vec2.hpp>
#include <string>
#include <optional>

namespace engine::core
{
    class Context;
}
namespace engine::render
{
    class Sprite;
}

namespace engine::resource
{
    class ResourceManager;
}

namespace engine::component
{
    class TransformComponent; // 前向声明
    class SpriteComponent final
     : public engine::component::Component
    {
        friend class engine::object::GameObject;

    private:
        // 用于监测变换组件的版本号，如果版本号发生变化，则更新sprite
        uint32_t _last_transform_version = 0xFFFFFFFF; // 初始设为最大值，确保第一次会更新

        TransformComponent *_transform_comp = nullptr;

        engine::render::Sprite _sprite;
        engine::utils::Alignment _alignment = engine::utils::Alignment::NONE;
        glm::vec2 _sprite_size = {0.0f, 0.0f};
        glm::vec2 _offset = {0.0f, 0.0f};
        bool _is_hidden = false;

    public:
        SpriteComponent(const std::string &texture_id,
                engine::utils::Alignment alignment = engine::utils::Alignment::NONE,
                std::optional<engine::utils::FRect> source_rect_opt = std::nullopt,
                bool is_flipped = false); // 这里的默认值必须存在
        ~SpriteComponent() override;

        // 禁止拷贝和移动
        SpriteComponent(const SpriteComponent &) = delete;
        SpriteComponent &operator=(const SpriteComponent &) = delete;
        SpriteComponent(SpriteComponent &&) = delete;
        SpriteComponent &operator=(SpriteComponent &&) = delete;

        void updateOffset();

        // GETTER
        const engine::render::Sprite &getSprite() const { return _sprite; }
        const std::string &getTextureId() const { return _sprite.getTextureId(); }
        const glm::vec2 getSpriteSize() const { return _sprite_size; }
        const glm::vec2 getOffset() const { return _offset; }
        engine::utils::Alignment getAlignment() const { return _alignment; }
        bool isFlipped() const { return _sprite.isFlipped(); }
        bool isHidden() const { return _is_hidden; }
        // 提供给 System 使用的 Transform 指针
        TransformComponent* getTransformComp() const { return _transform_comp; }

        // SETTER
        void setSpriteById(const std::string &texture_id, std::optional<engine::utils::FRect> _source_rect_opt = std::nullopt);
        void setFlipped(bool flipped) { _sprite.setFlipped(flipped); };
        void setHidden(bool hidden) { _is_hidden = hidden; };
        void setSourceRect(const std::optional<engine::utils::FRect> &source_rect_opt);
        void setAlignment(engine::utils::Alignment archor);

    private:
        void updateSpriteSize(); // 更新精灵大小

        // Component
        virtual void init() override;
        virtual void update(float delta_time) override;
        // ⚡️ render 逻辑现在由 SpriteRenderSystem 统一管理，组件内不再执行
        virtual void render() override {}
    };
}