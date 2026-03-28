/**
 * state_controller.h  —  数据驱动状态控制器
 *
 * 职责：实时决策。不关心如何画动画，只关心：
 *   - 当前按了什么键
 *   - 配置表里说现在能去哪
 *
 * 使用方式：
 *   1. init(data, "IDLE")
 *   2. 每帧：push input → update(dt, activeInputs) → 读取 UpdateResult
 *   3. 根据 result.currentState 播放动画
 *   4. 根据 result.rootMotionDx/Dy 施加冲量
 *   5. 根据 result.firedEvents 触发音效/特效
 */
#pragma once

#include "sm_types.h"
#include "input_buffer.h"
#include <functional>
#include <string>
#include <vector>

namespace game::statemachine {

class StateController {
public:
    StateController() = default;

    /** 绑定状态机数据并跳到初始状态 */
    void init(const StateMachineData* data, const std::string& initialState = "");

    /** 重置到初始状态 */
    void reset();

    /**
     *  主更新函数，每帧调用。
     *  @param dt           帧时间（秒）
     *  @param activeInputs 本帧激活的指令列表（已按下 or 持续）
     *  @param time         游戏总时间（用于 InputBuffer 时间戳）
     */
    UpdateResult update(float dt, const std::vector<std::string>& activeInputs, float time);

    /** 将一次按键压入指令缓冲池（通常在 isActionPressed 时调用）*/
    void pushInput(const std::string& action, float time);

    // ── 查询 ──────────────────────────────────────────────────────────────
    const std::string& getCurrentState() const { return m_currentStateName; }
    int                getCurrentFrame() const { return m_currentFrame; }
    float              getStateTime()    const { return m_stateTime; }
    bool               isValid()         const { return m_data != nullptr && m_currentState != nullptr; }

    /** 返回当前帧所在的区间类型（Locked/Combo/Cancelable），-1 = 不在任何区间 */
    int currentWindowType() const;

    /** 直接强制跳转到指定状态（用于外部事件，如被击；谨慎使用）*/
    void forceTransition(const std::string& stateName);

    /** 设置每帧时长（秒）——用于将 dt 折算成帧索引 */
    void setFrameDuration(float secondsPerFrame) { m_frameDuration = secondsPerFrame; }

private:
    const StateMachineData* m_data             = nullptr;
    const StateNode*        m_currentState     = nullptr;
    std::string             m_currentStateName;
    float                   m_stateTime        = 0.0f;  // 在当前状态已经过的秒数
    int                     m_currentFrame     = 0;
    int                     m_lastEventFrame   = -1;    // 已触发事件的最后帧（防重复）
    float                   m_frameDuration    = 0.1f;  // 100ms / 帧（可被动画组件覆盖）
    InputBuffer             m_inputBuffer;

    void doTransition(const std::string& stateName, UpdateResult& result);
    bool tryTransitions(const std::vector<std::string>& activeInputs,
                        bool inComboOrCancel, float time, UpdateResult& result);
    void collectFrameEvents(int prevFrame, int newFrame, UpdateResult& result);
    void collectRootMotion(int prevFrame, int newFrame, UpdateResult& result);
};

} // namespace game::statemachine
