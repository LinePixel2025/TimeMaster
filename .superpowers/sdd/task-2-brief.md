### Task 2: 追踪设置页添加最低记录阈值 SpinBox

**Files:**
- Modify: `src/ui/settings_dialog.h:48-50`（添加成员变量声明）
- Modify: `src/ui/settings_dialog.cpp:184-194`（添加 SpinBox UI）
- Modify: `src/ui/settings_dialog.cpp:235-245`（loadSettings 读取设置）
- Modify: `src/ui/settings_dialog.cpp:247-255`（saveSettings 保存设置）

**Interfaces:**
- Consumes: `m_db->getSetting("min_record_threshold", "40")` / `m_db->setSetting(...)`
- Produces: SpinBox 最小值为 0，最大值为 300，步长为 5

- [ ] **Step 1: 在 `settings_dialog.h` 中添加成员变量 `m_minRecordThreshold`**

在第 50 行（`m_minTrackingSeconds` 声明之后）添加：

```cpp
QSpinBox *m_minRecordThreshold;
```

- [ ] **Step 2: 在 `settings_dialog.cpp` 的追踪设置 Tab 中添加 UI 控件**

在第 192 行（`trackLayout->addLayout(minTrackRow)` 之后，`trackLayout->addStretch()` 之前）添加：

```cpp
QHBoxLayout *minRecordRow = new QHBoxLayout();
minRecordRow->addWidget(new QLabel(QString::fromUtf8("\xe6\x9c\x80\xe4\xbd\x8e\xe8\xae\xb0\xe5\xbd\x95\xe9\x98\x88\xe5\x80\xbc\xef\xbc\x88\xe7\xa7\x92\xef\xbc\x89:"), this));
m_minRecordThreshold = new QSpinBox(this);
m_minRecordThreshold->setRange(0, 300);
m_minRecordThreshold->setValue(40);
m_minRecordThreshold->setSingleStep(5);
m_minRecordThreshold->setSuffix(QString::fromUtf8(" \xe7\xa7\x92"));
m_minRecordThreshold->setToolTip(QString::fromUtf8("\xe5\x8d\x95\xe6\xac\xa1\xe4\xbd\xbf\xe7\x94\xa8\xe6\x97\xb6\xe9\x95\xbf\xe4\xbd\x8e\xe4\xba\x8e\xe6\xad\xa4\xe5\x80\xbc\xe7\x9a\x84\xe8\xae\xb0\xe5\xbd\x95\xe5\xb0\x86\xe4\xb8\x8d\xe8\xae\xa1\xe5\x85\xa5\xe7\xbb\x9f\xe8\xae\xa1\xe5\x92\x8c\xe5\xaf\xbc\xe5\x87\xba\xef\xbc\x8c" "0\xe4\xb8\xba\xe4\xb8\x8d\xe9\x99\x90\xe5\x88\xb6"));
minRecordRow->addWidget(m_minRecordThreshold);
minRecordRow->addStretch();
trackLayout->addLayout(minRecordRow);
```

- [ ] **Step 3: 在 `loadSettings()` 中读取新设置**

在第 240 行（`m_minTrackingSeconds` 读取之后）添加：

```cpp
m_minRecordThreshold->setValue(m_db->getSetting("min_record_threshold", "40").toInt());
```

- [ ] **Step 4: 在 `saveSettings()` 中保存新设置**

在第 252 行（`m_minTrackingSeconds` 保存之后）添加：

```cpp
m_db->setSetting("min_record_threshold", QString::number(m_minRecordThreshold->value()));
```

- [ ] **Step 5: 构建验证**

在项目根目录执行：

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

预期：构建成功，无编译错误。

- [ ] **Step 6: 运行测试**

```powershell
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
.\build\tests\test_exporter.exe
```

预期：所有测试通过。

- [ ] **Step 7: 提交**

```bash
git add src/ui/settings_dialog.h src/ui/settings_dialog.cpp
git commit -m "feat: add min_record_threshold SpinBox to tracking settings"
```

---

