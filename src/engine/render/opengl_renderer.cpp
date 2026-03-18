#include "opengl_renderer.h"
#include "camera.h"
#include "sprite.h"
#include "../resource/resource_manager.h"
#include <spdlog/spdlog.h>

// OpenGL headers
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

namespace engine::render
{
    OpenGLRenderer::OpenGLRenderer(SDL_Window* window)
        : m_window(window)
    {
        if (!initOpenGL())
        {
            spdlog::error("Failed to initialize OpenGL");
        }
    }

    OpenGLRenderer::~OpenGLRenderer()
    {
        clean();
    }

    bool OpenGLRenderer::initOpenGL()
    {
        // Create OpenGL context
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext)
        {
            spdlog::error("Failed to create OpenGL context: {}", SDL_GetError());
            return false;
        }

        SDL_GL_MakeCurrent(m_window, m_glContext);
        SDL_GL_SetSwapInterval(1); // Enable vsync

        spdlog::info("OpenGL initialized successfully");
        spdlog::info("OpenGL Version: {}", (const char*)glGetString(GL_VERSION));

        createSpriteShader();
        createSpriteBuffers();

        return true;
    }

    void OpenGLRenderer::drawTexture(SDL_GPUTexture* texture, float x, float y, float w, float h)
    {
        // OpenGLRenderer doesn't support GPU textures
    }

    void OpenGLRenderer::clean()
    {
        if (m_spriteVAO) glDeleteVertexArrays(1, &m_spriteVAO);
        if (m_spriteVBO) glDeleteBuffers(1, &m_spriteVBO);
        if (m_spriteShader) glDeleteProgram(m_spriteShader);

        for (auto& [path, tex] : m_textures)
        {
            glDeleteTextures(1, &tex);
        }
        m_textures.clear();

        if (m_glContext)
        {
            SDL_GL_DestroyContext(m_glContext);
            m_glContext = nullptr;
        }
    }

    void OpenGLRenderer::createSpriteShader()
    {
        const char* vertexShaderSource = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;

            out vec2 TexCoord;

            uniform mat4 projection;
            uniform mat4 model;

            void main()
            {
                gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 330 core
            out vec4 FragColor;
            in vec2 TexCoord;

            uniform sampler2D texture1;

            void main()
            {
                FragColor = texture(texture1, TexCoord);
            }
        )";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);

        m_spriteShader = glCreateProgram();
        glAttachShader(m_spriteShader, vertexShader);
        glAttachShader(m_spriteShader, fragmentShader);
        glLinkProgram(m_spriteShader);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void OpenGLRenderer::createSpriteBuffers()
    {
        float vertices[] = {
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,

            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f
        };

        glGenVertexArrays(1, &m_spriteVAO);
        glGenBuffers(1, &m_spriteVBO);

        glBindVertexArray(m_spriteVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void OpenGLRenderer::clearScreen()
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void OpenGLRenderer::present()
    {
        SDL_GL_SwapWindow(m_window);
    }

    void OpenGLRenderer::setDrawColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    glm::vec2 OpenGLRenderer::windowToLogical(float window_x, float window_y) const
    {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return glm::vec2(window_x, window_y);
    }

    void OpenGLRenderer::drawSprite(const Camera &camera,
                                    const Sprite &sprite,
                                    const glm::vec2 &position,
                                    const glm::vec2 &scale,
                                    double angle,
                                    const glm::vec4 &uv_rect)
    {
        // TODO: Implement OpenGL sprite rendering
        // This is a placeholder - full implementation needed
    }

    void OpenGLRenderer::drawParallax(const Camera &camera, const Sprite &sprite,
                                      const glm::vec2 &position,
                                      const glm::vec2 &scroll_factor,
                                      const glm::bvec2 &repeat,
                                      const glm::vec2 &scale, double angle)
    {
        // TODO: Implement OpenGL parallax rendering
    }

    void OpenGLRenderer::drawChunkVertices(const Camera &camera,
                                          const std::unordered_map<SDL_GPUTexture *, std::vector<GPUVertex>> &verticesPerTexture,
                                          const glm::vec2 &worldOffset)
    {
        // TODO: Implement OpenGL chunk rendering
        // Note: Will need to convert SDL_GPUTexture to OpenGL textures
    }

    void OpenGLRenderer::drawChunkBatches(const Camera &camera,
                                         const std::unordered_map<SDL_GPUTexture *, engine::world::TextureBatch> &batches,
                                         const glm::vec2 &worldOffset)
    {
        // TODO: Implement OpenGL batch rendering
    }
}
