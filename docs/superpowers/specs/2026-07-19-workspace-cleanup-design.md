# 工作区整理方案

## 背景

经过多轮 SDD 开发后，工作区积累了大量临时文件、构建日志和未提交的变更。需要系统性地清理和提交。

## 提交拆分

按功能领域拆分 7 个 commit：

### 1. `chore: cleanup`

- 删除临时文件：`build_*.txt`、`err*.txt`、`test_compile.*`、`test_*.txt`、`package_installer.ps1`
- 更新 `.gitignore`：添加 `build_*.txt`、`err*.txt`、`test_compile.*`、`.omo/`、`build-test/`、`*.obj` 等模式
- 提交 `.gitignore` 变更

### 2. `feat(dashboard): modular card system`

仪表盘全面模块化重写：
- 新文件：`chart_card`、`hero_card`、`compare_card`、`rank_card`、`topapp_card`、`insight_card`、`dashboard_card`、`dashboard_layout`、`grid_editor`、`svg_icon`、`design_tokens`、`wallpaper_helper`
- 修改文件：`main_window`（改用卡片系统）、`stats_widget`（精简）、`app_rank_widget`（精简）
- 构建系统：`src/CMakeLists.txt`、`CMakeLists.txt`
- 资源文件：`resources/resources.qrc`、`icon.svg`

### 3. `feat(ai): AI advisor module`

AI 分析功能：
- `src/ai/` 目录（AiAdvisor）
- `src/ui/ai_report_dialog`（AI 报告弹窗）
- `insight_card`（已在 commit 2 中提交，但 AI 模块本身分离）
- 测试文件

### 4. `refactor(database): simplify API`

- `database_manager.cpp` 精简（-262 行）
- `database_manager.h` 对应修改

### 5. `refactor(tracker): simplify polling`

- `window_tracker.cpp` 精简（-108 行）
- `window_tracker.h` 对应修改

### 6. `feat(push): LineWeb enhancement`

- `lineweb_pusher.cpp/.h` 增强
- 相关测试
- `main.cpp` 集成

### 7. `docs: update AGENTS.md`

- 更新 `AGENTS.md`
- 将设计文档纳入版本控制

## 范围边界

- 不移除仍在使用的旧文件（如 `stats_widget` 虽然大幅精简但仍存在）
- 不改变目录结构
- 每个 commit 保证可构建（`cmake --build build` 通过）

## 实施步骤

1. 按顺序执行 7 个 commit
2. 每个 commit 后运行 `cmake --build build` 验证
3. 最终推送到 GitHub
