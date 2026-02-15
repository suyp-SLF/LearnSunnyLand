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
        engine::core::Context *_context = nullptr; // ⚡️ 新增：缓存上下文

    public:
    // ⚡️ 由 GameObject 调用，完成依赖注入
        void attach(engine::object::GameObject* owner, engine::core::Context* ctx) {
            _owner = owner;
            _context = ctx;
            init(); // 确保在拿到 context 后才初始化
        }
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
        virtual void handleInput() {};
        virtual void update(float delta_time) = 0;
        virtual void render() = 0;
        virtual void clean() {};
    };
}; // namespace engine::component