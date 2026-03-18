#pragma once
#include "renderer.h"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <string>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

namespace engine::render
{
    class OpenGLRenderer : public Renderer
    {
    private:
        SDL_Window* m_window = nullptr;
        SDL_GLContext m_glContext = nullptr;

        GLuint m_spriteVAO = 0;
        GLuint m_spriteVBO = 0;
        GLuint m_spriteShader = 0;

        std::unordered_map<std::string, GLuint> m_textures;

    public:
        OpenGLRenderer(SDL_Window* window);
        ~OpenGLRenderer() override;

        glm::vec2 windowToLogical(float window_x, float window_y) const override;

        void drawSprite(const Camera &camera,
                       const Sprite &sprite,
                       const glm::vec2 &position,
                       const glm::vec2 &scale,
                       double angle,
                       const glm::vec4 &uv_rect) override;

        void drawParallax(const Camera &camera, const Sprite &sprite,
                         const glm::vec2 &position,
                         const glm::vec2 &scroll_factor,
                         const glm::bvec2 &repeat,
                         const glm::vec2 &scale, double angle) override;

        void drawChunkVertices(const Camera &camera,
                              const std::unordered_map<SDL_GPUTexture *, std::vector<GPUVertex>> &verticesPerTexture,
                              const glm::vec2 &worldOffset) override;

        void drawChunkBatches(const Camera &camera,
                             const std::unordered_map<SDL_GPUTexture *, engine::world::TextureBatch> &batches,
                             const glm::vec2 &worldOffset) override;

        void clearScreen() override;
        void present() override;
        void setDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
        void drawTexture(SDL_GPUTexture* texture, float x, float y, float w, float h) override;
        void clean() override;

    private:
        bool initOpenGL();
        void createSpriteShader();
        void createSpriteBuffers();
        GLuint loadTexture(const std::string& path);
    };
}
