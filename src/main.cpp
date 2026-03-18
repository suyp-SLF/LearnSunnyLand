#include <iostream>
#include "engine/core/game_app.h"
#include <spdlog/spdlog.h>

int main(int, char**){
    spdlog::set_level(spdlog::level::trace);

    engine::core::GameApp app;
    app.run();
    return 0;
}