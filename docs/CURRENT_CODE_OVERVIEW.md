# 当前代码简述（Character Editor 相关）

更新日期：2026-04-02

## 主要改动

1. 角色编辑器改为三栏布局
- 左侧：角色列表
- 中间：角色参数与编辑页签
- 右侧：实时预览（碰撞盒、影子、锚点、机甲高度辅助线）

2. 页签结构调整
- 合并“基础”和“资源”为“基础 & 资源”
- 新增“帧动画编辑器”页签（内联）
- 新增“状态机”页签（内联）

3. 路径绑定能力
- 帧动画编辑器与状态机编辑器可直接读取基础参数中的 JSON 路径
- 在页签中自动绑定并加载，减少重复手动打开文件

4. 图片资源选择
- 图片选择器固定扫描目录：assets/textures/Actors/
- 在“基础 & 资源”页签内通过按钮弹窗选择图片

5. 帧编辑器工具优化
- 内联工具栏增加“画格子切帧”开关
- 支持在编辑过程中快速切换网格切割模式

6. 配置体检提示
- 在基础参数页增加动画配置检查：
  - 帧动画 JSON 是否存在
  - 状态机 JSON 是否存在
  - 贴图路径是否存在

## 关键文件

- src/game/scene/character_editor_scene.cpp
- src/game/scene/character_editor_scene.h
- src/game/scene/frame_editor.cpp
- src/game/scene/frame_editor.h
- src/game/scene/state_machine_editor.cpp
- src/game/scene/state_machine_editor.h

## 构建状态

- 本地编译通过：cmake --build build

## 备注

该文档用于快速同步当前代码状态，方便提交与协作 review。
