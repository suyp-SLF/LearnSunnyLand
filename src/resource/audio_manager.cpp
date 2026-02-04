#include "audio_manager.h"
#include <spdlog/spdlog.h>

namespace engine::resource
{
    AudioManager::AudioManager()
    {
        if (!MIX_Init())
        {
            throw std::runtime_error("AudioManager 初始化失败");
        }
        spdlog::trace("TextureManager 构造成功");
    }

    AudioManager::~AudioManager()
    {
        clearAudios();
        MIX_Quit();
        spdlog::trace("TextureManager 析构成功");
    }

    MIX_Audio *AudioManager::loadAudio(const std::string &path)
    {
        auto it = _audios.find(path);
        if (it != _audios.end())
        {
            return it->second.get();
        }

        spdlog::debug("加载音频文件: {}", path);
        MIX_Audio *raw_audio = MIX_LoadAudio(_mixer, path.c_str(), false);
        if (!raw_audio)
        {
            spdlog::error("加载音频失败:'{}' {}", path, SDL_GetError());
            return nullptr;
        }
        _audios.emplace(path, std::unique_ptr<MIX_Audio, SDLMixAudioDeleter>(raw_audio));
        spdlog::debug("加载音频成功:'{}'", path);
        return raw_audio;
    }

    MIX_Audio *AudioManager::getAudio(const std::string &path)
    {
        auto it = _audios.find(path);
        if (it != _audios.end())
        {
            return it->second.get();
        }
        spdlog::warn("音频 '{}' 未找到缓存，尝试加载。", path);
        return loadAudio(path);
    }

    void AudioManager::unloadAudio(const std::string &path)
    {
        auto it = _audios.find(path);
        if (it != _audios.end())
        {
            spdlog::debug("卸载音频文件: {}", path);
            _audios.erase(it);
        }
        else
        {
            spdlog::warn("尝试卸载不存在的音频：{}", path);
        }
    }

    void AudioManager::clearAudios()
    {
        if (!_audios.empty())
        {
            spdlog::debug("正在清除 {} 个缓存的音频", _audios.size());
            _audios.clear();
        }
    }

};
