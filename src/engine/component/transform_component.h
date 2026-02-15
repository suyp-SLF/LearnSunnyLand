#pragma once
#include "./component.h"
#include <glm/vec2.hpp>
#include <cstdint> // 使用 uint32_t

namespace engine::component
{
    class TransformComponent : public engine::component::Component
    {
        friend class engine::object::GameObject;

    public:
        glm::vec2 _position = {0.0f, 0.0f};
        glm::vec2 _scale = {1.0f, 1.0f};
        float _rotation = 0.0f;
        uint32_t _version = 0; // 核心：版本号，用于检测变换是否发生

       TransformComponent(glm::vec2 position = {0.0f, 0.0f}, glm::vec2 scale = {1.0f, 1.0f}, float rotation = 0.0f)
            : _position(position), _scale(scale), _rotation(rotation), _version(0) {};
        
        // 禁止拷贝及移动
        TransformComponent(const TransformComponent&) = delete;
        TransformComponent& operator=(const TransformComponent&) = delete;
        TransformComponent(TransformComponent&&) = delete;
        TransformComponent& operator=(TransformComponent&&) = delete;

        // GETTER
        const glm::vec2 &getPosition() const { return _position; }
        const glm::vec2 &getScale() const { return _scale; }
        float getRotation() const { return _rotation; }
        uint32_t getVersion() const { return _version; }
        // SETTER
        void setPosition(glm::vec2 &position);
        void setScale(glm::vec2 &scale);
        void setRotation(float rotation);
        void translate(glm::vec2 &translation);

        private:
        void update(float delta_time) override {}
        virtual void render() override {}
    };
}