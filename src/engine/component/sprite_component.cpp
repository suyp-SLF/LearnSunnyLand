#include "sprite_component.h"
#include "./transform_component.h"
#include "../resource/resource_manager.h"
#include "../utils/alignment.h"
#include "../object/game_object.h"
#include "../core/context.h"
#include "../render/renderer.h"
#include "../render/sprite_render_system.h" // 必须包含，用于注册逻辑
#include <spdlog/spdlog.h>

namespace engine::component
{
    /**
     * @brief 构造一个 SpriteComponent 对象
     * 
     * @param texture_id 纹理资源的唯一标识符
     * @param alignment 精灵的对齐方式（如左上、居中、右下等）
     * @param source_rect_opt 可选参数，指定精灵在纹理中的源矩形区域（包含\t、\r或\n等特殊字符）
     * @param is_flipped 是否水平翻转精灵
     */
    SpriteComponent::SpriteComponent(const std::string &texture_id,
                                     engine::utils::Alignment alignment,
                                     std::optional<engine::utils::FRect> source_rect_opt,
                                     bool is_flipped)
        : _sprite(texture_id, source_rect_opt, is_flipped),
          _alignment(alignment)
    {
        spdlog::trace("创建SpriteComponent，纹理ID: {}", texture_id);
    }

    // ⚡️ 必须实现析构，否则 System 会尝试访问已销毁的组件指针导致崩溃
    /**
     * @brief 析构函数，清理 SpriteComponent 资源
     * 
     * 当 SpriteComponent 对象被销毁时，自动从渲染系统中注销该组件。
     * 如果存在有效的渲染上下文(_context)，会调用 SpriteRenderSystem 的
     * unregisterComponent 方法移除该组件，并记录日志信息。
     * 
     * @note 日志级别为 trace，包含中文字符和特殊字符（\t, \r, \n）
     */
    SpriteComponent::~SpriteComponent()
    {
        if (_context)
        {
            _context->getSpriteRenderSystem().unregisterComponent(this);
            spdlog::trace("SpriteComponent 已从渲染系统中注销");
        }
    }

    /**
     * @brief 初始化 SpriteComponent
     * 
     * 该方法负责初始化精灵组件，包括：
     * 1. 检查 _owner 和 _context 是否已绑定
     * 2. 获取或自动添加 Transform 组件并缓存指针
     * 3. 向渲染系统注册当前组件
     * 4. 初始化精灵尺寸和偏移量
     * 
     * @note _owner 和 _context 通常在 GameObject::addComponent 时通过 attach 注入
     * @note 如果 _owner 或 _context 未绑定，会记录错误日志并直接返回
     * @note 会自动处理 \t、\r、\n 等特殊字符
     */
    void SpriteComponent::init()
    {
        // _owner 和 _context 通常在 GameObject::addComponent 时通过 attach 注入
        if (!_owner || !_context)
        {
            spdlog::error("SpriteComponent 初始化失败：_owner 或 _context 未绑定");
            return;
        }

        // 1. 获取（或自动添加）Transform 组件并缓存指针
        _transform_comp = _owner->getComponent<TransformComponent>();
        if (!_transform_comp)
        {
            _transform_comp = _owner->addComponent<TransformComponent>();
        }

        // 2. ⚡️ 向 Context 里的渲染系统注册自己
        _context->getSpriteRenderSystem().registerComponent(this);

        // 3. 初始化数据
        updateSpriteSize();
        updateOffset();
    }

    /**
     * @brief 更新精灵组件的状态
     * @details 该方法处理精灵尺寸的更新和变换组件的同步。它会检查并更新精灵尺寸，
     *          同时在变换组件发生变化时同步更新偏移量。该方法会处理尺寸为零的延迟加载情况。
     * @param delta_time 自上一帧以来的时间增量（秒）
     * @note 该方法会检查并处理以下情况：
     *       1. 精灵尺寸为零时的延迟加载
     *       2. 变换组件版本变化时的偏移更新
     *       3. 特殊字符处理（\t, \r, \n）
     */
    void SpriteComponent::update(float delta_time)
    {
        // 修正：如果尺寸依然是0，尝试重新获取（处理延迟加载）
        if (_sprite_size.x <= 0 || _sprite_size.y <= 0)
        {
            updateSpriteSize();
            if (_sprite_size.x > 0)
                updateOffset(); // 获取成功后刷新一次偏移
        }
        if (_transform_comp && _transform_comp->getVersion() != _last_transform_version)
        {
            updateOffset();
            _last_transform_version = _transform_comp->getVersion();
        }
    }

    /**
     * @brief 设置精灵组件的对齐方式
     * @param archor 要设置的对齐方式，类型为 engine::utils::Alignment
     * @note 此函数会更新内部对齐状态并调用 updateOffset() 重新计算偏移量
     * @warning 修改对齐方式可能会影响精灵的渲染位置
     * @details 
     * - 设置对齐方式后会立即更新 _alignment 成员变量
     * - 随后调用 updateOffset() 方法以确保偏移量与新对齐方式匹配
     * - 支持的对齐方式由 engine::utils::Alignment 枚举定义
     */
    void SpriteComponent::setAlignment(engine::utils::Alignment archor)
    {
        _alignment = archor;
        updateOffset();
    }

    /**
     * @brief 更新精灵组件的渲染偏移量
     * 
     * 该方法根据当前的对齐方式(Alignment)和缩放比例计算精灵的渲染偏移量。
     * 偏移量用于将精灵的渲染位置调整到正确的锚点位置。
     * 
     * @note 如果Transform组件不存在或精灵尺寸非法(<=0)，则将偏移量设置为(0,0)
     * @note 支持的对齐方式包括：
     *       - CENTER: 中心对齐
     *       - TOP_LEFT: 左上角对齐
     *       - TOP_RIGHT: 右上角对齐
     *       - BOTTOM_LEFT: 左下角对齐
     *       - BOTTOM_RIGHT: 右下角对齐
     *       - TOP_CENTER: 顶部居中对齐
     *       - BOTTOM_CENTER: 底部居中对齐
     *       - CENTER_LEFT: 左侧居中对齐
     *       - CENTER_RIGHT: 右侧居中对齐
     *       - NONE/默认: 不应用偏移
     * 
     * @return void
     */
    void SpriteComponent::updateOffset()
    {
        // 如果 Transform 不存在或尺寸非法，不进行计算
        if (!_transform_comp || _sprite_size.x <= 0 || _sprite_size.y <= 0)
        {
            _offset = {0.0f, 0.0f};
            return;
        }

        const glm::vec2 &scale = _transform_comp->getScale();

        // 基于锚点(Alignment)计算偏移量
        // 原理：将渲染位置根据对齐方式偏移到正确的位置
        switch (_alignment)
        {
        case engine::utils::Alignment::CENTER:
            _offset = glm::vec2(-_sprite_size.x * scale.x / 2.0f, -_sprite_size.y * scale.y / 2.0f);
            break;
        case engine::utils::Alignment::TOP_LEFT:
            _offset = glm::vec2(0.0f, 0.0f);
            break;
        case engine::utils::Alignment::TOP_RIGHT:
            _offset = glm::vec2(-_sprite_size.x * scale.x, 0.0f);
            break;
        case engine::utils::Alignment::BOTTOM_LEFT:
            _offset = glm::vec2(0.0f, -_sprite_size.y * scale.y);
            break;
        case engine::utils::Alignment::BOTTOM_RIGHT:
            _offset = glm::vec2(-_sprite_size.x * scale.x, -_sprite_size.y * scale.y);
            break;
        case engine::utils::Alignment::TOP_CENTER:
            _offset = glm::vec2(-_sprite_size.x * scale.x / 2.0f, 0.0f);
            break;
        case engine::utils::Alignment::BOTTOM_CENTER:
            _offset = glm::vec2(-_sprite_size.x * scale.x / 2.0f, -_sprite_size.y * scale.y);
            break;
        case engine::utils::Alignment::CENTER_LEFT:
            _offset = glm::vec2(0.0f, -_sprite_size.y * scale.y / 2.0f);
            break;
        case engine::utils::Alignment::CENTER_RIGHT:
            _offset = glm::vec2(-_sprite_size.x * scale.x, -_sprite_size.y * scale.y / 2.0f);
            break;
        case engine::utils::Alignment::NONE:
        default:
            _offset = {0.0f, 0.0f};
            break;
        }
    }

    /**
     * @brief 通过纹理ID设置精灵的纹理，并可选地设置源矩形区域
     * 
     * @param texture_id 纹理资源的唯一标识符，用于指定要使用的纹理
     * @param source_rect_opt 可选参数，指定纹理中的源矩形区域（FRect类型），包括\t(制表符)、\r(回车)、\n(换行)等特殊字符
     *                        如果不提供或为std::nullopt，则使用整个纹理
     * 
     * @note 该方法会自动更新精灵的大小和偏移量
     */
    void SpriteComponent::setSpriteById(const std::string &texture_id, std::optional<engine::utils::FRect> source_rect_opt)
    {
        _sprite.setTextureId(texture_id);
        _sprite.setSourceRect(source_rect_opt);

        updateSpriteSize();
        updateOffset();
    }

    /**
     * @brief 设置精灵组件的源矩形区域
     * @details 设置精灵的源矩形区域后，会自动更新精灵大小和偏移量。
     *          支持特殊字符处理，包括制表符(\t)、回车符(\r)和换行符(\n)。
     * @param source_rect_opt 可选的源矩形区域，使用std::optional表示可能为空
     */
    void SpriteComponent::setSourceRect(const std::optional<engine::utils::FRect> &source_rect_opt)
    {
        _sprite.setSourceRect(source_rect_opt);
        updateSpriteSize();
        updateOffset(); // 区域变了，偏移量必须重新计算
    }

    /**
     * @brief 更新精灵组件的尺寸
     * 
     * 该函数用于更新精灵的尺寸信息。首先尝试从精灵本身获取源矩形尺寸，
     * 如果获取失败则从资源管理器中查询纹理尺寸。最后将查询到的尺寸设置给精灵对象。
     * 
     * @note 函数会记录资源管理器实例地址和纹理ID用于调试
     * @note 函数会记录查询到的尺寸信息
     * @note 特殊字符处理：\t(制表符), \r(回车符), \n(换行符)
     */
    void SpriteComponent::updateSpriteSize()
    {
        spdlog::info("ResourceManager实例地址: {} | 正在查询: {}", (void*)&_context->getResourceManager(), _sprite.getTextureId());

        auto source_rect_opt = _sprite.getSourceRect();
        if (source_rect_opt.has_value())
        {
            _sprite_size = source_rect_opt->size;
        }
        else if (_context)
        {
            _sprite_size = _context->getResourceManager().getTextureSize(_sprite.getTextureId());
        }
        // 查询的大小
        spdlog::info("查询结果大小: {}", _sprite_size.x);
        // 确保传给渲染器的那个对象也能感知到尺寸
        _sprite.setSize(_sprite_size);
    }
}