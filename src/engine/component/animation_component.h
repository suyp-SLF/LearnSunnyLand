#pragma once

#include "component.h"
#include "../utils/math.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::component
{
    class SpriteComponent;

    struct AnimationFrame
    {
        engine::utils::FRect sourceRect{{0.0f, 0.0f}, {0.0f, 0.0f}};
        float duration = 0.1f;
        bool  flipX = false;
    };

    /**
     * @brief 描述一段动画片段
     * row/col_start 以精灵表的格子坐标为单位（非像素）。
     */
    struct AnimationClip
    {
        int   row;            // 精灵表行索引（y = row * frame_h）
        int   col_start;      // 起始列索引
        int   frame_count;    // 帧数
        float frame_duration; // 每帧持续时间（秒）
        bool  loop = true;    // true=循环播放，false=抵达最后一帧后保持

        // ── 可变帧尺寸扩展（供非等高精灵表使用，如 gundom_sheet）── 
        float y_origin          = -1.0f;  // >=0 时直接用此像素 y 替代 row*frame_h
        float frame_h_override  =  0.0f;  // >0 时替代 AnimationComponent 的 m_frame_h

        // 显式帧序列：当非空时，优先使用这里的 sourceRect / duration / flipX。
        std::vector<AnimationFrame> frames;
    };

    /**
     * @brief 动画管理组件
     *
     * 使用方式：
     *   //  注册片段（一般在 createActor 阶段）
     *   anim->addClip("idle",  {0, 0, 8, 0.30f, true});
     *   anim->addClip("run",   {1, 0, 8, 0.09f, true});
     *
     *   //  切换状态（每帧在逻辑层调用即可，重复 play 同名片段无额外开销）
     *   anim->play("run");
     *
     * 组件的 update() 会在每帧自动前进时间轴并将 UV 写入 SpriteComponent。
     */
    class AnimationComponent final : public Component
    {
    public:
        /**
         * @param frame_w  单帧像素宽度（精灵表列宽）
         * @param frame_h  单帧像素高度（精灵表行高）
         */
        AnimationComponent(float frame_w, float frame_h);
        ~AnimationComponent() override = default;

        AnimationComponent(const AnimationComponent&)            = delete;
        AnimationComponent& operator=(const AnimationComponent&) = delete;
        AnimationComponent(AnimationComponent&&)                  = delete;
        AnimationComponent& operator=(AnimationComponent&&)       = delete;

        // --- 片段管理 ---

        /** 注册（或覆盖）一条动画片段 */
        void addClip(const std::string& name, AnimationClip clip);

        /** 切换到指定片段；若已在播该片段则无操作 */
        void play(const std::string& name);

        /** 强制从头播放（即使同名也重置）；用于连招/被打动画 */
        void forcePlay(const std::string& name);

        /** 当前非循环动画是否已播完 */
        bool isFinished() const;

        // --- 查询 ---
        const std::string& currentClip()  const { return m_current; }
        int                currentFrame()  const { return m_frame;   }
        float              currentTimer()  const { return m_timer;   }

        // --- 编辑器接口 ---
        float getFrameWidth()  const { return m_frame_w; }
        float getFrameHeight() const { return m_frame_h; }
        void  setFrameSize(float w, float h) { m_frame_w = w; m_frame_h = h; }
        const std::unordered_map<std::string, AnimationClip>& getClips() const { return m_clips; }
        void removeClip(const std::string& name)
        {
            m_clips.erase(name);
            if (m_current == name) { m_current.clear(); m_frame = 0; m_timer = 0.0f; }
        }
        void clearClips()
        {
            m_clips.clear();
            m_current.clear();
            m_frame = 0;
            m_timer = 0.0f;
        }

    protected:
        void init()   override;
        void update(float dt) override;
        void render() override {}

    private:
        float m_frame_w;
        float m_frame_h;

        std::unordered_map<std::string, AnimationClip> m_clips;
        std::string m_current;
        int   m_frame = 0;
        float m_timer = 0.0f;

        SpriteComponent* m_sprite = nullptr;

        void applyFrame();
    };
} // namespace engine::component
