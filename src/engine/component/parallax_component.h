#pragma once
#include "component.h"
#include "../render/sprite.h"
#include <string>
#include <glm/vec2.hpp>

namespace engine::component
{
    class TransformComponent;

    class ParallaxComponent : public Component
    {
        friend class engine::object::GameObject;

    private:
        TransformComponent *_transform = nullptr;
        engine::render::Sprite *_sprite = nullptr;
        glm::vec2 _scoll_factor = glm::vec2(1.0f, 1.0f);
        glm::bvec2 _repeat = glm::bvec2(false, false);
        bool _is_hidden = false;

    public:
        ParallaxComponent(const std::string &texture_id,
                          const glm::vec2 &scoll_factor,
                          glm::bvec2 repeat);
        // GETTER
        engine::render::Sprite *getSprite() const { return _sprite; };
        glm::vec2 getScollFactor() const { return _scoll_factor; };
        glm::bvec2 getRepeat() const { return _repeat; };
        bool isHidden() const { return _is_hidden; };
        // SETTER
        void setSprite(engine::render::Sprite *sprite) { _sprite = sprite; };
        void setScollFactor(const glm::vec2 &scoll_factor) { _scoll_factor = scoll_factor; };
        void setRepeat(const glm::bvec2 &repeat) { _repeat = repeat; };
        void setHidden(bool is_hidden) { _is_hidden = is_hidden; };

    protected:
        // 核心循环函数
        void update(float delta_time) override;
        void init() override;
        void render() override;
    };
} // namespace engine::component