#pragma once

#include <glm/vec2.hpp>
#include <cstdint>
#include "./component.h"

namespace engine::component
{
    /**
     * @brief 变换组件
     * 核心机制：使用版本号（Version）实现脏标记追踪，支持高性能按需更新。
     */
    class TransformComponent final : public engine::component::Component
    {
        // 允许 GameObject 直接访问私有构造/成员以进行生命周期管理
        friend class engine::object::GameObject;

    public:
        // --- 构造与析构 ---
        TransformComponent(glm::vec2 position = {0.0f, 0.0f}, 
                           glm::vec2 scale    = {1.0f, 1.0f}, 
                           float rotation     = 0.0f)
            : _position(position), _scale(scale), _rotation(rotation), _version(0) {}

        // 显式禁止拷贝与移动，确保组件在内存中的唯一性与安全性
        TransformComponent(const TransformComponent&) = delete;
        TransformComponent& operator=(const TransformComponent&) = delete;
        TransformComponent(TransformComponent&&) = delete;
        TransformComponent& operator=(TransformComponent&&) = delete;

        virtual ~TransformComponent() = default;

        // --- Getter (只读访问) ---
        const glm::vec2& getPosition() const { return _position; }
        const glm::vec2& getScale()    const { return _scale; }
        float            getRotation() const { return _rotation; }
        uint32_t         getVersion()  const { return _version; }

        // --- Setter (状态修改，将触发版本号更新) ---
        void setPosition(const glm::vec2& position);
        void setScale(const glm::vec2& scale);
        void setRotation(float rotation);
        void translate(const glm::vec2& translation);

    private:
        // --- 核心变换数据 ---
        glm::vec2 _position;
        glm::vec2 _scale;
        float     _rotation;

        /** * @brief 数据版本号
         * 只要变换数据发生改变，该值即自增。
         * 渲染系统或其他系统通过比对旧版本号来实现 Lazy Evaluation（延迟计算）。
         */
        uint32_t  _version;

        // --- 基类接口实现 ---
        
        /** * @brief 逻辑更新
         * 变换组件作为纯数据源，通常不执行每帧主动更新逻辑。
         */
        void update(float delta_time) override {}

        /**
         * @brief 渲染接口
         * 变换本身不可见，由 SpriteRenderSystem 结合此数据进行批量渲染。
         */
        void render() override {}
    };
}