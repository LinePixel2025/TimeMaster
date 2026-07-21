# Time Master（时间大师）

Windows 桌面时间追踪工具。通过系统托盘常驻运行，自动记录前台窗口使用时长，数据存储在本地 SQLite 数据库中，支持统计查看和导出（CSV / XLSX）。

## 技术栈

- **C++17** / **Qt6**（Widgets + Sql）
- **CMake** 3.22+ / **vcpkg**（manifest 模式）
- **SQLite**（通过 Qt SQL 模块嵌入，无需单独安装）

## 环境要求

- Visual Studio 2022
- vcpkg，需安装 `qt6-base` 和 `qt6-tools`
- 设置环境变量 `VCPKG_ROOT` 指向 vcpkg 目录

## 构建

```powershell
cmake --preset default
cmake --build build --config Release
```

## 测试

```powershell
ctest --test-dir build -C Debug
ctest --test-dir build -C Debug -R test_database   # 运行单个测试
```

## 使用

运行 `build\Release\TimeMaster.exe`。程序驻留在系统托盘中，右键托盘图标可打开菜单。前台窗口切换时自动记录各应用的使用时长。
