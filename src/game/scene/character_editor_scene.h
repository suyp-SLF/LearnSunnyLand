#pragma once

#include "../../engine/scene/scene.h"
#include "frame_editor.h"
#include "state_machine_editor.h"
#include <SDL3/SDL.h>
#include <array>
#include <string>
#include <vector>

namespace game::scene
{
    class CharacterEditorScene : public engine::scene::Scene
    {
    public:
        CharacterEditorScene(const std::string& name,
                             engine::core::Context& context,
                             engine::scene::SceneManager& sceneManager);

        void init() override;
        void update(float delta_time) override;
        void render() override;
        void handleInput() override;
        void clean() override;

    private:
        struct CharacterProfile
        {
            std::string id;
            std::string displayName;
            std::string frameJsonPath;
            std::string stateMachineJsonPath;
            std::string texturePath;
            std::string mapRole;
            float collisionHalfW = 12.0f;
            float collisionHalfD = 3.5f;
            float mechHeightPx = 0.0f;
            float moveSpeed = 220.0f;
            float jumpVelocity = -420.0f;
            int maxHp = 100;
            int maxEnergy = 100;
            std::string filePath;
        };

        SDL_GLContext m_glContext = nullptr;
        bool m_showSettings = false;
        bool m_showCreatePopup = false;
        bool m_hasUnsavedChanges = false;
        std::vector<CharacterProfile> m_profiles;
        int m_selectedProfile = -1;
        int m_editingProfile = -1;

        std::array<char, 64> m_newCharacterIdBuf{"new_character"};
        std::array<char, 96> m_newCharacterNameBuf{"新角色"};

        std::array<char, 64> m_editIdBuf{};
        std::array<char, 96> m_editNameBuf{};
        std::array<char, 256> m_editFrameJsonBuf{};
        std::array<char, 256> m_editSmJsonBuf{};
        std::array<char, 256> m_editTextureBuf{};
        std::array<char, 32> m_editMapRoleBuf{};

        struct PreviewFrame
        {
            int sx = 0;
            int sy = 0;
            int sw = 64;
            int sh = 64;
            int durationMs = 100;
            int anchorX = 32;
            int anchorY = 56;
        };

        struct PreviewAction
        {
            std::string name;
            bool isLoop = true;
            std::vector<PreviewFrame> frames;
        };

        std::vector<PreviewAction> m_previewActions;
        int m_previewActionIndex = 0;
        int m_previewFrameIndex = 0;
        float m_previewTimerSec = 0.0f;
        std::string m_previewLoadedFrameJson;
        std::string m_previewTexturePath;
        std::string m_previewError;

        std::vector<std::string> m_textureAssets;
        int m_selectedTextureAsset = -1;

        FrameEditor m_frameEditor;
        StateMachineEditor m_smEditor;
        int m_activeTabIndex = 0;
        int m_requestedTab   = -1;

        // 用于将“基础参数中的路径”直接绑定到内联编辑器，避免每帧重复重载
        std::string m_boundFrameJsonPath;
        std::string m_boundTexturePath;
        std::string m_boundSmJsonPath;

        void renderToolbar();
        void renderProfilePanel();
        void renderSettings();
        void renderPerformanceOverlay() const;
        void renderPreviewPanel(CharacterProfile& p);
        void renderActorImagePicker(CharacterProfile& p);
        void syncEditBuffersFromSelection();
        void applyEditBuffersToSelection();
        void createNewCharacterFile();
        void loadPreviewFromProfile(const CharacterProfile& p);
        void scanTextureAssets();

        void loadProfiles();
        void saveCurrentProfile();
    };
} // namespace game::scene
