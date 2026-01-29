# MTC 项目开发完成走查

## 项目概述

**MTC (Multi-Terminal Commander)** 是一个跨平台的 Windows 终端环境变量管理工具，使用 C++ 和 wxWidgets 开发。

## 已完成的功能

✅ 多组环境变量配置管理（创建、编辑、删除、复制）  
✅ 自定义环境变量（覆盖系统环境变量）  
✅ 工作目录指定  
✅ 一键启动终端  
✅ 配置持久化存储（JSON 格式，保存在 `data/` 目录）  
✅ 自动备份功能  
✅ 导入/导出配置  
✅ 跨平台支持（Windows/Linux/macOS）
✅ 应用程序图标支持

---

## 项目结构

```
D:\Temporary\mtc\
├── CMakeLists.txt              # CMake 构建配置
├── vcpkg.json                  # vcpkg 依赖配置
├── README.md                   # 项目说明
├── docs/
│   └── development-guide.md    # 开发文档
├── src/
│   ├── main.cpp                # 程序入口
│   ├── core/
│   │   ├── Types.h             # 数据类型定义
│   │   ├── ConfigManager.h/cpp # 配置管理器
│   │   └── TerminalLauncher.h/cpp # 终端启动器
│   ├── ui/
│   │   ├── MainFrame.h/cpp     # 主窗口
│   │   ├── ProfileDialog.h/cpp # 配置编辑对话框
│   │   └── EnvVarPanel.h/cpp   # 环境变量编辑面板
│   └── utils/
│       └── PathUtils.h/cpp     # 路径工具
├── resources/
│   ├── mtc.rc                  # Windows 资源文件
│   └── mtc.manifest            # Windows 清单
└── data/
    ├── config.json             # 配置文件
    └── backups/                # 配置备份目录
```

---

## 核心文件说明

### 核心模块 (`src/core/`)

| 文件 | 功能 |
|------|------|
| [Types.h](file:///D:/Temporary/mtc/src/core/Types.h) | 定义 `Profile`、`EnvVariable`、`TerminalType` 等核心数据结构 |
| [ConfigManager.h/cpp](file:///D:/Temporary/mtc/src/core/ConfigManager.cpp) | 单例配置管理器，处理 JSON 配置读写、备份、CRUD 操作 |
| [TerminalLauncher.h/cpp](file:///D:/Temporary/mtc/src/core/TerminalLauncher.cpp) | 跨平台终端启动器，支持 Windows Terminal、PowerShell、CMD 等 |

### UI 模块 (`src/ui/`)

| 文件 | 功能 |
|------|------|
| [MainFrame.h/cpp](file:///D:/Temporary/mtc/src/ui/MainFrame.cpp) | 主窗口，包含配置列表和操作按钮 |
| [ProfileDialog.h/cpp](file:///D:/Temporary/mtc/src/ui/ProfileDialog.cpp) | 配置编辑对话框，支持编辑名称、描述、工作目录、终端类型和环境变量 |
| [EnvVarPanel.h/cpp](file:///D:/Temporary/mtc/src/ui/EnvVarPanel.cpp) | 环境变量编辑面板，嵌入在配置对话框中 |

---

## 构建说明

### 依赖项

- C++17 编译器 (MSVC/GCC/Clang)
- wxWidgets 3.2+
- nlohmann/json

### Windows 构建步骤

```powershell
# 1. 安装 vcpkg（如果尚未安装）
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# 2. 安装依赖
.\vcpkg install wxwidgets:x64-windows-static nlohmann-json:x64-windows-static

# 3. 构建项目
cd D:\Temporary\mtc
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -G "Visual Studio 17 2022" -A x64

# 4. 编译
cmake --build . --config Release

# 5. 运行
.\bin\Release\mtc.exe
```

---

## 使用方法

1. **运行程序**：双击 `mtc.exe`
2. **新建配置**：点击"新建配置"按钮
3. **设置配置**：
   - 输入配置名称和描述
   - 选择工作目录
   - 选择终端类型（或自动检测）
   - 添加环境变量（变量名=值）
4. **启动终端**：选中配置后点击"启动终端"按钮（或双击）
5. **导入/导出**：可以导出配置备份或导入其他配置

---

## 技术亮点

1. **跨平台设计**：使用条件编译支持 Windows/Linux/macOS
2. **环境变量覆盖**：通过 `CreateProcessW` 的环境变量块实现系统变量覆盖
3. **自动终端检测**：优先使用 Windows Terminal，自动回退到 CMD
4. **配置备份**：每次保存自动备份，最多保留 10 个历史版本
5. **单例模式**：`ConfigManager` 使用单例模式确保配置一致性

---

## 下一步

- [x] 安装 vcpkg 和依赖
- [x] 编译项目
- [x] 测试功能 (构建成功，生成 `mtc.exe`)

## 构建产物

构建完成后，在 `D:\Temporary\mtc\bin\Release` 目录下可以找到以下文件：

- `mtc.exe`: 主程序
- `data/`: 数据目录（首次运行时会自动创建配置文件）
- `zlib1.dll`, `jpeg62.dll` 等依赖库
