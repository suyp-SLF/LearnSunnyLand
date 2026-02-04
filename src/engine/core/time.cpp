#include "time.h"
#include <SDL3/SDL_timer.h>
#include <spdlog/spdlog.h>

namespace engine::core
{
    /**
     * Time类的默认构造函数
     * 初始化_last_time和_frame_start_time为当前系统时间
     */
    Time::Time()
    {
        // 获取当前系统时间（纳秒级）并赋值给_last_time和_frame_start_time
        _last_time = SDL_GetTicksNS();
        _frame_start_time = _last_time;

        // 使用spdlog记录trace级别的日志，输出初始化时的当前时间
        spdlog::trace("Time 初始化，当前时间：{}", _last_time);
    }

    /**
     * 更新时间状态，计算帧时间差并控制帧率
     * 该方法在每一帧开始时调用，用于更新游戏的时间相关数据
     */
    void Time::update()
    {
        // 获取当前帧的开始时间（纳秒级精度）
        _frame_start_time = SDL_GetTicksNS();
        // 计算当前帧与上一帧的时间差（转换为秒）
        auto current_delta_timer = static_cast<double>(_frame_start_time - _last_time) / 1000000000.0;
        // 如果设置了目标帧率时间（大于0），则进行帧率限制
        if (_target_frame_time > 0)
        {
            // 调用帧率限制函数，确保帧率不超过目标值
            limitFrameRate(current_delta_timer);
        }
        else
        {
            _delta_time = current_delta_timer;
        }

        _last_time = SDL_GetTicksNS();
    }

    /**
     * 获取经过时间缩放后的增量时间
     * @return 返回经过时间缩放系数调整后的增量时间值
     */
    float Time::getDeltaTime() const
    {
        return _delta_time * _timer_scale; // 返回原始增量时间与时间缩放系数的乘积
    }

    /**
     * 获取未缩放的时间增量（delta time）
     * @return 返回自上一帧以来的时间增量（单位：秒），不受时间缩放影响
     */
    float Time::getUnscaleDeltaTime() const
    {
        return _delta_time;
    }

    /**
     * 获取计时器的时间缩放比例
     * @return 返回计时器的时间缩放比例值
     */
    float Time::getTimerScale() const
    {
        return _timer_scale; // 返回成员变量_timer_scale的值
    }

    /**
     * 设置时间缩放比例
     * @param scale 时间缩放比例值，用于调整游戏内时间的流逝速度
     */
    void Time::setTimerScale(float scale)
    {                         // 设置时间缩放比例的函数
        _timer_scale = scale; // 将传入的缩放比例值赋给成员变量_timer_scale
    }

    /**
     * 设置目标帧率
     * @param fps 目标帧率值，单位为帧每秒(FPS)
     */
    void Time::setTargetFPS(int fps)
    {
        _target_fps = fps; // 将传入的帧率值赋给成员变量_target_fps
    }

    /**
     * 获取目标帧率的方法
     * @return 返回目标帧率值_target_fps
     */
    int Time::getTargetFPS() const
    {
        return _target_fps; // 返回成员变量_target_fps的值
    }
    /**
     * 限制帧率函数，确保渲染帧率不超过目标帧率
     * @param current_delta_time 当前帧的增量时间（秒）
     */
    void Time::limitFrameRate(float current_delta_time)
    {
        // 如果当前帧时间大于目标帧时间，说明渲染过快，需要延迟
        if (current_delta_time > _target_frame_time)
        {
            // 计算需要等待的时间（秒）
            double timer_to_wait = _target_frame_time - current_delta_time;
            // 将等待时间转换为纳秒
            Uint64 ns_to_wait = static_cast<Uint64>(timer_to_wait * 1000000000.0);
            // 使用SDL_Delay函数进行延迟
            SDL_DelayNS(ns_to_wait);
            // 更新delta_time为实际经过的时间
            _delta_time = static_cast<float>(SDL_GetTicksNS() - _last_time) / 1000000000.0;
        }
    }
}
