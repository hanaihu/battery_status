# Battery Status — 项目指引

把无线鼠标 / 键盘 / 耳机的电量显示在 Windows 任务栏上的小型 C++/Win32 程序。

**当前状态：M0 之前。** 只有文档，尚无代码。开工前先确认 `docs/QUALITY.md` 与 README 的路线图。

## 先读什么

1. `docs/QUALITY.md` — 质量要求与优先级，与本文冲突时以它为准
2. `README.md` — 技术方案（三层电量来源）、任务栏实现要点、非目标
3. `LICENSE` — MIT

## 硬约束

| 项 | 要求 |
|---|---|
| 平台 | 仅 Windows 10/11 x64 |
| 语言 | C++17，Win32 API，不用 MFC / Qt / ATL 向导 |
| 运行时 | 无外部运行时依赖，静态 CRT（`/MT`），单 exe |
| 权限 | 绝不要求管理员权限 |
| 网络 | 零网络访问，不做自动更新 |
| 体积 | 目标 < 500 KB |
| 构建 | `build.bat`（MSVC）或 `make`（MinGW-w64），不引入 CMake 之外的构建系统 |
| 许可 | MIT；**不得粘贴 GPL 项目的源码**，只可参考 Windows / HID 规范中的公开事实 |

## 代码约定

- **不抛异常**（`/EHsc-`），错误走返回码 + 状态枚举
- 电量三态：`Level::Unknown` / `Stale` / `Fresh`，禁止用 0% 表示未知
- 每个 device 只产生一个 `DeviceStatus`，按容器 ID 或 `VID/PID` 去重
- 新增厂商 = `src/providers/<vendor>.cpp` + 在 provider 表里加一行；**不要**改核心调度
- 协议解析写成纯函数（`const BYTE* + len → 结构体`），内部不得发起 IO，便于单测
- 设备返回的字节一律视为不可信：先校验长度再取下标
- 设备名不得参与路径拼接；过滤控制字符与 bidi 覆盖字符
- `Paint()` 内禁止创建 GDI 对象；字体、画笔在初始化时建、销毁时删
- 每个 `Create*` 必须能指出它的销毁路径
- 魔法数字必须命名（HID usage、特性号、VID/PID 一律用可读常量）

## 目录

```
src/main.cpp        入口、消息循环、轮询线程、托盘、设置、自启
src/widget.cpp      任务栏分层子窗口：绘制、定位、多显示器镜像
src/devices.cpp     设备模型与合并去重
src/hid.cpp         HID 枚举与报文收发
src/bt.cpp          SetupDi 枚举与 PnP 属性读取
src/providers/      每厂商一个文件
tests/              协议解析单测 + --dump fixture
docs/               设计文档
```

## 提交与语言

- Conventional Commits，**英文**，祈使句，主题 ≤ 50 字符
- 一次提交只做一件事
- 提交信息、代码、标识符、注释：英文；文档正文与对话：中文

## 禁止

- 修改或注入 `explorer.exe`，不使用 DeskBand / DLL 注入
- 引入第三方运行时、安装服务或驱动
- 未经要求的大范围重构
- 提交构建产物（见 `.gitignore`）
