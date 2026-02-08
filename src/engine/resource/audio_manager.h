#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <SDL3_mixer/SDL_mixer.h>

namespace engine::resource
{
    class AudioManager
    {
        friend class ResourceManager;

    private:
        struct SDLMixTrackDeleter
        {
            // 确保这里是 MIX_Track
            void operator()(MIX_Track *track) const
            {
                if (track)
                    MIX_DestroyTrack(track);
            }
        };

        struct SDLMixAudioDeleter
        {
            // 确保这里是 MIX_Audio
            void operator()(MIX_Audio *audio) const
            {
                if (audio)
                    MIX_DestroyAudio(audio);
            }
        };
        std::unordered_map<std::string, std::unique_ptr<MIX_Track, SDLMixTrackDeleter>> _tracks;
        std::unordered_map<std::string, std::unique_ptr<MIX_Audio, SDLMixAudioDeleter>> _audios;
        MIX_Mixer *_mixer = nullptr;

    public:
        AudioManager();
        ~AudioManager();
        AudioManager(const AudioManager &) = delete;
        AudioManager &operator=(const AudioManager &) = delete;
        AudioManager(AudioManager &&) = delete;
        AudioManager &operator=(AudioManager &&) = delete;

    private:
        MIX_Audio *loadAudio(const std::string &path);
        MIX_Audio *getAudio(const std::string &path);
        void unloadAudio(const std::string &path);
        void clearAudios();
    };
};
