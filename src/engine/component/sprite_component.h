#pragma once
#include "./component.h"
#include "./transform_component.h"
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
    class SpriteComponent final
     : public engine::component::Component
    {
        friend class engine::object::GameObject;

    private:
        engine::resource::ResourceManager *_resource_manager = nullptr;
        TransformComponent *_transform_comp = nullptr;

        engine::render::Sprite _sprite;
        engine::utils::Alignment _alignment = engine::utils::Alignment::NONE;
        glm::vec2 _sprite_size = {0.0f, 0.0f};
        glm::vec2 _offset = {0.0f, 0.0f};
        bool _is_hidden = false;

    public:
        SpriteComponent(const std::string &texture_id,
                engine::resource::ResourceManager &resource_manager,
                engine::utils::Alignment alignment = engine::utils::Alignment::NONE,
                std::optional<engine::utils::FRect> source_rect_opt = std::nullopt,
                bool is_flipped = false); // 这里的默认值必须存在
        ~SpriteComponent() override = default;

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
        virtual void update(float, engine::core::Context &) override {};
        virtual void render(engine::core::Context &) override;
    };
}