### Task 3: Tracker 层读取新设置（一致性）

**Files:**
- Modify: `src/tracker/window_tracker.cpp:220-234`

**Interfaces:**
- Consumes: `m_db->getSetting("min_record_threshold", "40")`
- Produces: 无（tracker 不直接使用此值，仅保持 reloadSettings 与设置项同步）

- [ ] **Step 1: 在 `reloadSettings()` 中读取新设置**

在 `window_tracker.cpp` 第 226 行（`m_minTrackingSeconds` 读取之后）添加：

```cpp
m_db->getSetting("min_record_threshold", "40");
```

- [ ] **Step 2: 构建验证**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

- [ ] **Step 3: 提交**

```bash
git add src/tracker/window_tracker.cpp
git commit -m "chore: read min_record_threshold in tracker reloadSettings"
```

