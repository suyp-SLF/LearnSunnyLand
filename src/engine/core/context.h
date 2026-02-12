#pragma once

namespace engine::input
{
    class InputManager;
}
namespace engine::render
{
    class Renderer;
    class Camera;
}
namespace engine::resource
{
    class ResourceManager;
}

namespace engine::core
{
    class Context final
    {
    private:
        engine::input::InputManager &_input_manager;
        engine::render::Renderer &_renderer;
        engine::render::Camera &_camera;
        engine::resource::ResourceManager &_resource_manager;

    public:
        Context(engine::input::InputManager &input_manager, 
            engine::render::Renderer &renderer, 
            engine::render::Camera &camera, 
            engine::resource::ResourceManager &resource_manager);
        // 禁止拷贝和移动
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;
        Context(Context &&) = delete;
        Context &operator=(Context &&) = delete;

        // GETTER
        engine::resource::ResourceManager &getResourceManager() const {return _resource_manager;};
        engine::render::Renderer &getRenderer() const {return _renderer;};
        engine::render::Camera &getCamera() const {return _camera;};
        engine::input::InputManager &getInputManager() const {return _input_manager;};
    };
} // namespace engine::core