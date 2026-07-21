### Task 3 完成报告

**状态**: 已完成

**提交 SHA**: `d4a75a9ecaf73322aa8aee9670de56162e83f4a2`

**更改摘要**: 在 `src/tracker/window_tracker.cpp:227` 的 `reloadSettings()` 方法中新增一行，读取 `min_record_threshold` 设置项（默认值 `"40"`）。

**测试结果**:
- `test_database.exe` — 全部 12 项测试通过
- `test_exporter.exe` — 全部 2 项测试通过
