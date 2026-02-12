#include "context.h"
#include "../input/input_manager.h"
#include "../render/camera.h"
#include "../render/renderer.h"
#include "../resource/resource_manager.h"
#include <spdlog/spdlog.h>

namespace engine::core
{
    Context::Context(engine::input::InputManager &input_manager,
                     engine::render::Renderer &renderer,
                     engine::render::Camera &camera,
                     engine::resource::ResourceManager &resource_manager)
                     : _input_manager(input_manager),
                       _renderer(renderer),
                       _camera(camera),
                       _resource_manager(resource_manager)
    {
        spdlog::trace("上下文初始化完成，当前上下文指针：{}", static_cast<const void*>(this));
    }
}