#include "renderer.h"
#include "camera.h"
#include "../resource/resource_manager.h"
#include <SDL3_image/SDL_image.h>
#include <spdlog/spdlog.h>

namespace engine::render
{
    /**
     * @brief 构造函数，初始化渲染器对象
     * @param renderer SDL渲染器指针，用于实际的渲染操作
     * @param resource_manager 资源管理器指针，用于管理渲染所需的资源
     * @throws std::runtime_error 当renderer或resource_manager为空时抛出异常
     * @note 构造函数会设置默认绘制颜色为黑色(0,0,0,255)
     * @note 构造过程会记录trace和error级别的日志
     */
    Renderer::Renderer(SDL_Renderer *renderer, engine::resource::ResourceManager *resource_manager)
        : _renderer(renderer),
          _resource_manager(resource_manager)
    {
        spdlog::trace("构造 Renderer");
        if (!_renderer)
        {
            spdlog::error("无法创建渲染器");
            throw std::runtime_error("无法创建渲染器");
        }
        if (!_resource_manager)
        {
            spdlog::error("无法创建渲染器，资源管理器为空");
            throw std::runtime_error("无法创建渲染器，资源管理器为空");
        }
        setDrawColor(0, 0, 0, 255);
        spdlog::trace("构造 Renderer 完成");
    }

    /**
     * @brief 绘制精灵到屏幕
     * 
     * @param camera 相机对象，用于世界坐标到屏幕坐标的转换
     * @param sprite 要绘制的精灵对象
     * @param position 精灵在世界坐标系中的位置
     * @param scale 精灵的缩放比例，x和y方向可以不同
     * @param angle 精灵的旋转角度（度）
     * 
     * @note 该函数会执行以下操作：
     *       1. 获取精灵对应的纹理资源
     *       2. 计算精灵的源矩形和目标矩形
     *       3. 应用相机变换将世界坐标转换为屏幕坐标
     *       4. 检查精灵是否在可视区域内
     *       5. 使用SDL渲染旋转后的精灵
     * 
     * @warning 如果纹理获取失败或源矩形无效，函数会记录错误并返回
     * 
     * @details 特殊字符处理：
     *          - \t: 制表符
     *          - \r: 回车符
     *          - \n: 换行符
     */
    void Renderer::drawSprite(const Camera &camera, const Sprite &sprite, const glm::vec2 &position, const glm::vec2 &scale, double angle)
    {
        auto texture = _resource_manager->getTexture(sprite.getTextureId());
        if (!texture)
        {
            spdlog::error("无法为ID：{}的纹理获取纹理", sprite.getTextureId());
            return;
        }
        auto src_rect = getSpriteRect(sprite);
        if (!src_rect.has_value())
        {
            spdlog::error("无法获取精灵的源矩形，ID：{}", sprite.getTextureId());
            return;
        }
        // 应用相机变换
        glm::vec2 position_screen = camera.worldToScreen(position);
        // 计算目标矩形
        float scaled_width = src_rect.value().w * scale.x;
        float scaled_height = src_rect.value().h * scale.y;
        SDL_FRect dst_rect = {position_screen.x,
                              position_screen.y,
                              scaled_width,
                              scaled_height};
        // 不在屏幕内不绘制
        if (isRectInViewport(camera, dst_rect))
            return;

        // 执行绘制
        if (!SDL_RenderTextureRotated(_renderer, texture, &src_rect.value(), &dst_rect, angle, nullptr, sprite.isFlipped() ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE))
        {
            spdlog::error("渲染旋转纹理失败，ID：{}：{}", sprite.getTextureId(), SDL_GetError());
        }
    }
    void Renderer::drawParallax(const Camera &camera, const Sprite &sprite, const glm::vec2 &position, const glm::vec2 &scroll_factor, const glm::bvec2 &repeat, const glm::vec2 &scale, double angle)
    {
        auto texture = _resource_manager->getTexture(sprite.getTextureId());
        if (!texture)
        {
            spdlog::error("无法为ID：{}的纹理获取纹理", sprite.getTextureId());
            return;
        }
        auto src_rect = getSpriteRect(sprite);
        if (!src_rect.has_value())
        {
            spdlog::error("无法获取精灵的源矩形，ID：{}", sprite.getTextureId());
            return;
        }

        // 应用相机变换
        glm::vec2 position_screen = camera.worldToScreenWithParallax(position, scroll_factor);
        // 计算缩放后的纹理尺寸
        float scaled_width = src_rect.value().w * scale.x;
        float scaled_height = src_rect.value().h * scale.y;
        glm::vec2 start, stop;
        glm::vec2 viewport_size = camera.getViewportSize();

        if (repeat.x)
        {
            start.x = glm::mod(position_screen.x, scaled_width) - scaled_width;
            stop.x = viewport_size.x;
        }
        else
        {
            start.x = position_screen.x;
            stop.x = glm::mod(position_screen.x + scaled_width, viewport_size.x);
        }
        if (repeat.y)
        {
            start.y = glm::mod(position_screen.y, scaled_height) - scaled_height;
            stop.y = viewport_size.y;
        }
        else
        {
            start.y = position_screen.y;
            stop.y = glm::mod(position_screen.y + scaled_height, viewport_size.y);
        }
        for (float x = start.x; x < stop.x; x += scaled_width)
        {
            for (float y = start.y; y < stop.y; y += scaled_height)
            {
                SDL_FRect dst_rect = {x, y, scaled_width, scaled_height};
                if (!SDL_RenderTexture(_renderer, texture, nullptr, &dst_rect))
                {
                    spdlog::error("渲染纹理失败，ID：{}：{}", sprite.getTextureId(), SDL_GetError());
                    return;
                }
            }
        }
    }
    /**
     * @brief 在UI上绘制一个精灵
     * 
     * @param sprite 要绘制的精灵对象，包含纹理ID、翻转状态等信息
     * @param position 绘制位置的坐标(x, y)
     * @param size 可选参数，指定绘制的尺寸(width, height)。如果不提供，则使用精灵的原始尺寸
     * 
     * @note 该函数会处理以下特殊情况：
     *       - 纹理获取失败时记录错误并返回
     *       - 精灵源矩形获取失败时记录错误并返回
     *       - 支持水平翻转绘制
     *       - 支持自定义绘制尺寸
     * 
     * @warning 函数内部使用SDL_RenderTextureRotated进行绘制，如果绘制失败会记录SDL错误信息
     */
    void Renderer::drawUISprite(const Sprite &sprite, const glm::vec2 &position, const std::optional<glm::vec2> &size)
    {
        auto texture = _resource_manager->getTexture(sprite.getTextureId());
        if (!texture)
        {
            spdlog::error("无法为ID：{}的纹理获取纹理", sprite.getTextureId());
            return;
        }
        auto src_rect = getSpriteRect(sprite);
        if (!src_rect.has_value())
        {
            spdlog::error("无法获取精灵的源矩形，ID：{}", sprite.getTextureId());
            return;
        }
        // 计算目标矩形
        SDL_FRect dst_rect = {position.x, position.y, 0, 0};
        if (size.has_value())
        {
            dst_rect.w = size.value().x;
            dst_rect.h = size.value().y;
        }
        else
        {
            dst_rect.w = src_rect.value().w;
            dst_rect.h = src_rect.value().h;
        }
        // 执行绘制
        if (!SDL_RenderTextureRotated(_renderer, texture, &src_rect.value(), &dst_rect, 0.0f, nullptr, sprite.isFlipped() ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE))
        {
            spdlog::error("渲染旋转纹理失败，ID：{}：{}", sprite.getTextureId(), SDL_GetError());
        }
    }

    /**
     * @brief 将渲染缓冲区的内容呈现到屏幕上
     * 
     * 该函数调用SDL的渲染呈现功能，将所有渲染操作的结果显示在屏幕上。
     * 如果渲染失败，会记录错误日志。
     * 
     * @note 该函数不处理\t、\r或\n等特殊字符，仅处理渲染缓冲区内容
     */
    void Renderer::present()
    {
        if (!SDL_RenderPresent(_renderer))
        {
            spdlog::error("渲染失败：{}", SDL_GetError());
        }
    }
    /**
     * @brief 清除屏幕渲染缓冲区
     * 
     * 该函数使用SDL的渲染清除功能来清除当前渲染目标的内容。
     * 清除操作可能失败，如果失败会记录错误日志。
     * 
     * @note 该函数会处理SDL_RenderClear可能返回的错误情况
     * @note 特殊字符处理：\t(制表符), \r(回车符), \n(换行符)
     */
    void Renderer::clearScreen()
    {
        if (!SDL_RenderClear(_renderer))
        {
            spdlog::error("清屏失败：{}", SDL_GetError());
        }
    }
    /**
     * @brief 设置渲染器的绘制颜色
     * @details 设置用于后续绘图操作的颜色（RGBA格式），包括特殊字符处理（\t, \r, \n）
     * @param r 红色分量值（0-255）
     * @param g 绿色分量值（0-255）
     * @param b 蓝色分量值（0-255）
     * @param a 透明度分量值（0-255）
     * @note 如果设置失败，会记录错误日志
     */
    void Renderer::setDrawColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        if (!SDL_SetRenderDrawColor(_renderer, r, g, b, a))
        {
            spdlog::error("设置绘制颜色失败：{}", SDL_GetError());
        }
    }
    /**
     * @brief 设置渲染器的绘制颜色（浮点数格式）
     * 
     * @param r 红色分量，范围 [0.0, 1.0]
     * @param g 绿色分量，范围 [0.0, 1.0]
     * @param b 蓝色分量，范围 [0.0, 1.0]
     * @param a 透明度分量，范围 [0.0, 1.0]
     * 
     * @note 该方法会调用 SDL_SetRenderDrawColorFloat 设置颜色
     * @note 如果设置失败，会记录错误日志
     * @note 错误信息包含 SDL 返回的错误字符串
     * 
     * @warning 颜色值超出范围可能导致未定义行为
     * 
     * @see SDL_SetRenderDrawColorFloat
     */
    void Renderer::setDrawColorFloat(float r, float g, float b, float a)
    {
        if (!SDL_SetRenderDrawColorFloat(_renderer, r, g, b, a))
        {
            spdlog::error("设置绘制颜色失败：{}", SDL_GetError());
        }
    }
    /**
     * @brief 获取精灵的矩形区域
     * @details 根据精灵的源矩形配置或纹理尺寸计算并返回精灵的矩形区域。
     *          如果精灵配置了源矩形，则直接返回源矩形（需验证尺寸有效性）。
     *          如果未配置源矩形，则从纹理获取完整尺寸作为矩形区域。
     * @param sprite 要获取矩形区域的精灵对象
     * @return std::optional<SDL_FRect> 成功时返回有效的矩形区域，失败时返回std::nullopt
     * @note 可能返回std::nullopt的情况：
     *       - 纹理获取失败
     *       - 源矩形尺寸无效
     *       - 无法获取纹理尺寸
     */
    std::optional<SDL_FRect> Renderer::getSpriteRect(const Sprite &sprite)
    {
        SDL_Texture *texture = _resource_manager->getTexture(sprite.getTextureId());
        if (!texture)
        {
            spdlog::error("无法为ID：{}的纹理获取纹理", sprite.getTextureId());
            return std::nullopt;
        }
        auto src_rect = sprite.getSourceRect();
        if (src_rect.has_value())
        {
            if (src_rect.value().w <= 0 || src_rect.value().h <= 0)
            {
                spdlog::error("源矩形尺寸无效，ID：{}", sprite.getTextureId());
                return std::nullopt;
            }
            return src_rect;
        }
        else
        {
            SDL_FRect result = {0, 0, 0, 0};
            if (!SDL_GetTextureSize(texture, &result.w, &result.h))
            {
                spdlog::error("无法获取纹理尺寸，ID：{}", sprite.getTextureId());
                return std::nullopt;
            }
            return result;
        }
    }
    /**
     * @brief 检查矩形是否完全位于相机视口内
     * 
     * @param camera 相机对象，用于获取视口尺寸
     * @param rect 要检查的矩形，包含位置和尺寸信息
     * @return true 如果矩形完全位于视口内
     * @return false 如果矩形部分或完全位于视口外
     * 
     * @note 该方法通过比较矩形的四个边界与视口边界的关系来判断
     */
    bool Renderer::isRectInViewport(const Camera &camera, const SDL_FRect &rect)
    {
        glm::vec2 viewport_size = camera.getViewportSize();
        return rect.x > -rect.w && rect.x + rect.w < viewport_size.x &&
               rect.y > -rect.h && rect.y + rect.h < viewport_size.y;
    }
}
