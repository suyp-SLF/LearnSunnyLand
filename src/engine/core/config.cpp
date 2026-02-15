#include "config.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace engine::core
{
    Config::Config(const std::string &json_path)
    {
        loadFromFile(json_path);
    }

    bool Config::loadFromFile(const std::string &json_path)
    {
        std::ifstream file(json_path);
        if (!file.is_open())
        {
            spdlog::warn("配置文件 {} 打开失败，使用默认配置", json_path);
            if (!saveToFile(json_path))
            {
                spdlog::error("无法创建默认配置文件 {} ", json_path);
                return false;
            }
            return false;
        }
        try
        {
            nlohmann::json j;
            file >> j;
            fromJson(j);
            spdlog::info("配置文件 {} 加载成功", json_path);
            return true;
        }
        catch (const std::exception &e)
        {
            spdlog::error("配置文件 {} 解析失败: {}", json_path, e.what());
        }
        return false;
    }

    bool Config::saveToFile(const std::string &json_path) const
    {
        std::ofstream file(json_path);
        if (!file.is_open())
        {
            spdlog::error("配置文件 {} 打开失败", json_path);
            return false;
        }
        try
        {
            nlohmann::json j = toJson();
            file << j.dump(4);
            spdlog::info("配置文件 {} 保存成功", json_path);
            return true;
        }
        catch (const std::exception &e)
        {
            spdlog::error("配置文件 {} 保存失败: {}", json_path, e.what());
        }
        return false;
    }

    void Config::fromJson(const nlohmann::json &json)
    {
        if (json.contains("window"))
        {
            const auto &window_config = json["window"];
            _window_title = window_config.value("title", _window_title);
            _window_width = window_config.value("width", _window_width);
            _window_height = window_config.value("height", _window_height);
            _logical_width = window_config.value("logical_width", _logical_width);
            _logical_height = window_config.value("logical_height", _logical_height);
            _camera_width = window_config.value("camera_width", _camera_width);
            _camera_height = window_config.value("camera_height", _camera_height);
            _window_resizable = window_config.value("resizable", _window_resizable);
        }
        if (json.contains("graphics"))
        {
            const auto &graphics_config = json["graphics"];
            _render_type = graphics_config.value("render_type", _render_type);
            _vsync_enabled = graphics_config.value("vsync", _vsync_enabled);
        }
        if (json.contains("performance"))
        {
            const auto &performance_config = json["performance"];
            _target_fps = performance_config.value("target_fps", _target_fps);
            if (_target_fps <= 0)
            {
                spdlog::warn("目标帧率必须大于0，使用默认值 60");
                _target_fps = 60;
            }
        }
        if (json.contains("audio"))
        {
            const auto &audio_config = json["audio"];
            _music_volume = audio_config.value("music_volume", _music_volume);
            _sfx_volume = audio_config.value("sfx_volume", _sfx_volume);
        }

        if (json.contains("input_mapping") && json["input_mapping"].is_object())
        {
            const auto &mapping_json = json["input_mapping"];
            try
            {
                auto input_mappings = mapping_json.get<std::unordered_map<std::string, std::vector<std::string>>>();
                _input_mappings = std::move(input_mappings);
                spdlog::trace("输入映射加载成功");
            }
            catch (const std::exception &e)
            {
                spdlog::warn("输入映射加载失败, 使用默认映射: {}", e.what());
            }
        }
        else
        {
            spdlog::warn("输入映射加载失败, 未找到输入映射对象, 使用默认映射");
        }
    }

    nlohmann::ordered_json Config::toJson() const
    {
        return nlohmann::ordered_json{
            {"window", {{"title", _window_title}, {"width", _window_width}, {"height", _window_height}, {"logical_width", _logical_width}, {"logical_height", _logical_height}, {"camera_width", _camera_width}, {"camera_height", _camera_height}, {"resizable", _window_resizable}}},
            {"graphics", {{"vsync", _vsync_enabled}, {"render_type", _render_type}}},
            {"performance", {{"target_fps", _target_fps}}},
            {"audio", {{"music_volume", _music_volume}, {"sfx_volume", _sfx_volume}}},
            {"input_mapping", _input_mappings}};
    }
}; // namespace engine::core::
