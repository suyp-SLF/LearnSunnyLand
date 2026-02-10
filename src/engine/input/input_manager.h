#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <SDL3/SDL_render.h>
#include <glm/vec2.hpp>
#include <variant>

namespace engine::core
{
    class Config;
}

namespace engine::input
{
    enum class ActionState
    {
        INACTIVE,
        PRESSED_THIS_FRAME,
        HELD_DOWN,
        RELEASED_THIS_FRAME
    };

    class InputManager
    {
    private:
        SDL_Renderer *_sdl_renderer;
        std::unordered_map<std::string, std::vector<std::string>> _actions_to_keyname_map;
        std::unordered_map<std::variant<SDL_Scancode, Uint32>, std::vector<std::string>> _input_to_action_map;

        std::unordered_map<std::string, ActionState> _action_states;

        bool _should_quit = false;
        glm::vec2 _mouse_position;

    public:
        InputManager(SDL_Renderer *sdl_renderer, const engine::core::Config *config);

        void update();

        // 动作状态检查
        bool isActionDown(const std::string &action_name) const;
        bool isActionPressed(const std::string &action_name) const;
        bool isActionReleased(const std::string &action_name) const;

        // 获取是否退出
        bool shouldQuit() const;
        void setShouldQuit(bool should_quit);

        // 获取鼠标位置
        glm::vec2 getMousePosition() const;
        glm::vec2 getLogicalMousePosition() const;

    private:
        void processEvent(const SDL_Event &event);
        void initializeMappings(const engine::core::Config *config);
        void updateActionState(const std::string &action_name, bool is_input_active, bool is_repeat_event);
        SDL_Scancode scancodeFromString(const std::string &key_name) const;
        Uint8 MouseButtonUint8FromString(const std::string &button_name) const;
    };
}; // namespace engin::input