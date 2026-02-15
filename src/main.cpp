#include <iostream>
#include "engine/core/game_app.h"
#include <spdlog/spdlog.h>

int main(int, char**){
    spdlog::set_level(spdlog::level::trace); // Set global log level to debug

    engine::core::GameApp app;
    app.run();
    return 0;
}
/** 文件路径
src/
└── engine/
    ├── core/
    │   ├── context.h / .cpp           # 持有渲染器指针和系统
    ├── component/
    │   ├── component.h                # 基类：增加 _context 缓存
    │   ├── transform_component.h
    │   └── sprite_component.h         # 只存数据，不负责具体渲染
    ├── render/
    │   ├── renderer.h                 # ⚡️ 接口类：定义 drawSprite 等抽象行为
    │   ├── sdl_renderer.h / .cpp      # 具体的 SDL 实现
    │   ├── vulkan_renderer.h / .cpp   # 具体的 Vulkan 实现
    │   ├── sprite_render_system.h     # ⚡️ 逻辑类：遍历组件并调用 renderer
    │   └── sprite.h                   # 数据类：贴图ID、Rect等
    └── object/
        └── game_object.h              # 负责组件的自动注册
 */
