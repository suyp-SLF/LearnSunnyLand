#pragma once

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

        // 关键循环函数，由引擎调用
        virtual void handleInput() {};
        virtual void update(float) {};
        virtual void render() {};
        virtual void clean() {};

    protected:
        virtual void init() {};
    };
}; // namespace engine::component