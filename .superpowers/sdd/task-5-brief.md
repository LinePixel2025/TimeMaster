### Task 5: 构建并验证

**Files:**
- 无代码变更，仅构建测试

- [ ] **Step 1: 设置环境并构建**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
taskkill /f /im TimeMaster.exe 2>$null
cmake --preset mingw
cmake --build build
```

- [ ] **Step 2: 验证构建通过**

确认输出中无 `error:` 字样，最终显示 `[100%] Built target TimeMaster`。

- [ ] **Step 3: 运行应用验证 UI**

```powershell
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
# 临时在 main.cpp 中添加 window.show()，启动应用后打开设置验证 UI
```

验证项目：
1. 对话框大小 700×480，左右分栏显示
2. 已知应用列表显示图标 + 友好名称（别名优先）
3. 已屏蔽应用在左侧灰显（`#9CA3AF`）
4. 搜索框实时过滤两个列表
5. `→` 按钮添加屏蔽后两侧同步刷新
6. `←` 按钮/Del 键移除屏蔽后两侧同步刷新
7. Ctrl+A 全选已知列表可用

- [ ] **Step 4: 运行测试确保无回归**

```powershell
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
.\build\tests\test_exporter.exe
```

两个测试均应返回退出代码 0。

- [ ] **Step 5: 恢复 main.cpp（移除 window.show()）并提交**

```powershell
git add src/ui/settings_dialog.h src/ui/settings_dialog.cpp
git diff --cached
git commit -m "feat: 优化设置中已知应用和已屏蔽应用列表展示"
```
