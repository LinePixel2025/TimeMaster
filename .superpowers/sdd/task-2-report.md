### Task 2 完成报告

**状态**: 已完成

**Commit SHA**: `c77b0ad`

**修改内容**:
- `src/ui/settings_dialog.h:50`: 添加 `QSpinBox *m_minRecordThreshold;` 成员变量
- `src/ui/settings_dialog.cpp`: 在追踪设置 Tab 中添加 "最低记录阈值" SpinBox UI（范围 0-300，步长 5，默认 40，含 tooltip）
- `src/ui/settings_dialog.cpp`: loadSettings 中读取 `min_record_threshold` 设置，默认值 `"40"`
- `src/ui/settings_dialog.cpp`: saveSettings 中保存 `min_record_threshold` 设置

**测试结果**: 全部通过
- test_database.exe: 12/12 PASS
- test_exporter.exe: 2/2 PASS
