#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <imgui.h>

namespace engine::resource { class ResourceManager; }

namespace game::scene {

// ─────────────────────────────────────────────────────────────────────────────
//  数据结构
// ─────────────────────────────────────────────────────────────────────────────

enum class BoxType { Hurtbox = 0, Hitbox = 1, Bodybox = 2 };

struct FrameBox {
    BoxType type  = BoxType::Hurtbox;
    float   x = 0, y = 0, w = 32, h = 32; // 相对锚点偏移（像素）
};

struct FrameEvent {
    char name[64] = {};
};

struct FrameData {
    int sx = 0, sy = 0, sw = 64, sh = 64; // 源矩形（精灵表像素坐标）
    int anchor_x = 32, anchor_y = 56;     // 锚点（帧内像素坐标）
    int duration_ms = 100;
    bool flip_x = false;                  // 单帧水平反向
    std::vector<FrameBox>   boxes;
    std::vector<FrameEvent> events;
};

struct ActionData {
    char name[64]            = {};
    std::vector<FrameData>   frames;
    bool is_loop             = true;
    ActionData() { std::strncpy(name, "New_Action", 63); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  FrameEditor 主类
// ─────────────────────────────────────────────────────────────────────────────
class FrameEditor {
public:
    FrameEditor()  = default;
    ~FrameEditor() = default;

    /** 打开编辑器窗口 */
    void open()          { if (!m_open) { m_open = true; m_firstOpen = true; m_showLauncher = true; scanJsonFiles(); } }
    void toggle()        { if (m_open) { m_open = false; } else { m_open = true; m_firstOpen = true; m_showLauncher = true; scanJsonFiles(); } }
    bool isOpen()  const { return m_open; }

    /** 每帧在 ImGui NewFrame/Render 之间调用 */
    void render(engine::resource::ResourceManager &resMgr);

    /** 打开并加载指定帧 JSON，可选覆盖纹理路径；suggestedSavePath 用于路径为空时设定合理默认保存名 */
    void openWithJson(const std::string &jsonPath, const std::string &texturePath = "",
                      const std::string &suggestedSavePath = "");

    /** 在当前 ImGui 上下文（Tab/ChildWindow）内联渲染，无独立浮动窗口 */
    void renderInline(engine::resource::ResourceManager& resMgr);

    /** 外部预览面板所需的当前帧信息 */
    struct FrameViewInfo {
        unsigned int glTex = 0;
        float texW = 0.0f, texH = 0.0f;
        int sx = 0, sy = 0, sw = 64, sh = 64;
        int anchorX = 32, anchorY = 56;
        bool valid = false;
    };
    FrameViewInfo peekCurrentFrame() const;

    /** 外部查询：用户是否点击了"状态机编辑器"按钮 */
    bool wantsSmEditor() const { return m_wantsSmEditor; }
    void clearSmEditorRequest()    { m_wantsSmEditor = false; }

    /** 当前保存路径 */
    std::string getSavePath() const { return std::string(m_savePath); }

    /** 若上次 renderInline 期间用户执行了保存，返回 true 并清除标志（一次性消费） */
    bool popJustSaved() { bool v = m_justSaved; m_justSaved = false; return v; }

private:
    // ── 窗口状态 ──────────────────────────────────────────────────────────
    bool         m_open        = false;
    bool         m_firstOpen   = true;   // 首次打开时定位到屏幕中央
    bool         m_wantsSmEditor = false; // 用户请求打开状态机编辑器
    bool         m_justSaved   = false;  // 本帧执行了保存

    // ── 当前项目数据 ──────────────────────────────────────────────────────
    char         m_texturePath[512] = "assets/textures/Characters/gundom.png";
    std::vector<ActionData> m_actions;
    int          m_selAction   = -1;    // 当前选中动作索引
    int          m_selFrame    = -1;    // 当前选中帧索引

    // ── 纹理缓存 ──────────────────────────────────────────────────────────
    unsigned int m_glTex       = 0;
    float        m_texW        = 0.0f;
    float        m_texH        = 0.0f;

    // CPU 端像素缓存（用于魔棒 BFS）
    std::vector<uint8_t> m_pixels; // RGBA8

    // ── 画布视图变换 ──────────────────────────────────────────────────────
    ImVec2       m_origin      = {0.0f, 0.0f}; // 画布左上角对应的纹理像素坐标
    float        m_zoom        = 3.0f;
    bool         m_panning     = false;
    ImVec2       m_panAnchor   = {0.0f, 0.0f}; // 开始平移时鼠标屏幕坐标
    ImVec2       m_origOnPan   = {0.0f, 0.0f}; // 开始平移时的 m_origin

    // ── 帧拖拽选区 ────────────────────────────────────────────────────────
    bool         m_dragging    = false;
    ImVec2       m_dragStartTex = {0.0f, 0.0f};
    ImVec2       m_dragEndTex   = {0.0f, 0.0f};

    // ── 锚点编辑模式 ──────────────────────────────────────────────────────
    bool         m_anchorMode  = false;

    // ── 判定盒绘制 ────────────────────────────────────────────────────────
    BoxType      m_activeBoxType = BoxType::Hurtbox;
    bool         m_boxDragging  = false;
    ImVec2       m_boxDragStart = {0.0f, 0.0f};   // 帧内像素坐标（相对锚点）
    int          m_selBoxIdx    = -1;

    // ── 动画预览 ──────────────────────────────────────────────────────────
    bool         m_previewPlay  = true;
    float        m_previewTimer = 0.0f;
    int          m_previewFrame = 0;

    // ── 洋葱皮 ────────────────────────────────────────────────────────────
    bool         m_onionSkin    = true;
    float        m_onionAlpha   = 0.30f;

    // ── 精灵网格辅助 ──────────────────────────────────────────────────────
    bool         m_showGrid     = true;
    int          m_gridW        = 32;
    int          m_gridH        = 32;
    bool         m_snapToGrid   = false;

    // ── 网格切割模式（设置帧宽/高/步长 → 点击网格单元提取帧）────────────
    bool         m_gridCutMode  = false;  // 切换网格切割模式
    int          m_gcFrameW     = 102;    // 帧宽
    int          m_gcFrameH     = 104;    // 帧高
    int          m_gcSpacingX   = 0;     // 水平间距
    int          m_gcSpacingY   = 0;     // 垂直间距
    int          m_gcOffsetX    = 0;     // 起始偏移 X
    int          m_gcOffsetY    = 0;     // 起始偏移 Y
    bool         m_gcHovering   = false;  // 鼠标正悬停在某个格子
    int          m_gcHoverCol   = -1;
    int          m_gcHoverRow   = -1;

    // ── 动作内联重命名 ────────────────────────────────────────────────────
    int          m_renamingAction = -1;   // -1=不在重命名，>=0=正在重命名的动作索引
    char         m_renameBuffer[64] = {};

    // ── 时间轴帧拖拽重排 ──────────────────────────────────────────────────
    bool  m_tlDragging   = false;    // 时间轴帧正在被拖拽
    int   m_tlDragSrcIdx = -1;       // 拖拽源帧索引
    int   m_tlInsertIdx  = -1;       // 当前插入位置 (0=最前,N=最后)

    // ── 开屏启动页 ────────────────────────────────────────────────────────
    bool         m_showLauncher  = true;   // true=显示启动/选择页
    struct JsonEntry {
        std::string path;         // 相对路径
        std::string displayName;  // 文件名（不含扩展名）
    };
    std::vector<JsonEntry> m_jsonFiles;    // 扫描到的 JSON 文件列表

    // ── 文件路径缓冲 ──────────────────────────────────────────────────────
    char         m_savePath[512] = "assets/textures/Characters/frame_data.json";
    char         m_statusMsg[128] = {};  // 底部状态文字

    // ── 私有方法 ──────────────────────────────────────────────────────────
    void renderMenuBar(engine::resource::ResourceManager &resMgr);
    void renderLeftPanel();    // 动作列表 + 帧列表
    void renderCanvas();       // 主画布（居中大区域）
    void renderRightPanel();   // 帧属性 + 判定盒 + 事件
    void renderPreview();      // 底部时间轴 + 预览
    void renderStatusBar();

    // 纹理加载与像素拉取
    void loadTexture(engine::resource::ResourceManager &resMgr);
    void fetchPixels();        // glGetTexImage → m_pixels

    // 网格切割模式
    void renderGridCutOverlay();  // 在画布上绘制网格切割预览

    // 魔棒 BFS 自动扣帧
    void magicWandAt(int texPx, int texPy);

    // 帧操作
    void addFrameFromRect(int sx, int sy, int sw, int sh);
    void confirmDragFrame();   // 将 dragStartTex/dragEndTex 提交为新帧

    // 坐标转换（均以 canvasViewPos = 当前帧子窗口左上角屏幕坐标为基准）
    // 存储在 render 调用中更新
    ImVec2       m_cvPos       = {0.0f, 0.0f};  // 画布子窗口左上角
    ImVec2       m_cvSize      = {0.0f, 0.0f};  // 画布子窗口尺寸

    ImVec2 texToScreen(float tx, float ty) const;
    ImVec2 screenToTex(float sx, float sy) const;
    ImVec2 framePixToScreen(int fx, int fy) const; // 帧内→屏幕（需 selFrame 有效）

    // JSON 存储
    void saveJSON();
    void loadJSON();
    void loadJSONFrom(const std::string &path);  // 从指定路径加载
    void ensureDefaultActions();                  // 确保 IDLE/MOVE 存在且不可删
    void scanJsonFiles();                         // 扫描 Characters 目录

    // 启动选择页
    void renderLauncher(engine::resource::ResourceManager &resMgr);

    // 工具
    ActionData*  selAction() { return (m_selAction >= 0 && m_selAction < (int)m_actions.size()) ? &m_actions[m_selAction] : nullptr; }
    FrameData*   selFrame()  {
        auto* a = selAction();
        return (a && m_selFrame >= 0 && m_selFrame < (int)a->frames.size()) ? &a->frames[m_selFrame] : nullptr;
    }
    // 判断动作是否受保护（不可删除）
    bool isProtectedAction(int idx) const {
        if (idx < 0 || idx >= (int)m_actions.size()) return false;
        const char* n = m_actions[idx].name;
        return (std::strcmp(n, "IDLE") == 0 || std::strcmp(n, "MOVE") == 0);
    }
};

} // namespace game::scene
