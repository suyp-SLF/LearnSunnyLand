#pragma once
namespace engine::core
{
    class Context;
}

namespace engine::object
{
    class GameObject;
}

namespace engine::component
{
    class Component
    {
        friend class engine::object::GameObject;

    protected:
        engine::object::GameObject *_owner = nullptr;

    public:
        Component() = default;
        virtual ~Component() = default;

        // 禁止拷贝和移动，一般不移动
        Component(const Component &) = delete;
        Component &operator=(const Component &) = delete;
        Component(Component &&) = delete;
        Component &operator=(Component &&) = delete;

        void setOwner(engine::object::GameObject *owner) { _owner = owner; };
        engine::object::GameObject *getOwner() const { return _owner; };

    protected:
        virtual void init() {};
        virtual void handleInput(engine::core::Context&) {};
        virtual void update(float, engine::core::Context&) = 0;
        virtual void render(engine::core::Context&) = 0;
        virtual void clean() {};
    };
}; // namespace engine::component