# Battery Status

Show the battery level of your wireless mouse, keyboard and headset on the Windows 11 taskbar —
a small, dependency-free C++/Win32 app.

把无线鼠标 / 键盘 / 耳机的电量直接显示在 Windows 任务栏上。

> **状态：早期开发。** 目前只有设计文档，代码尚未开始。

---

## 界面预览（目标形态）

```
┌───────────────────────────────────────────────────────────┐
│  ...   🖱 85%   🎧 30%   │  ˄  🔊  🌐  ⌨   22:31   │
└───────────────────────────────────────────────────────────┘
                          ↑
                    Battery Status
```

- 常驻在通知区域左侧，跟随浅色 / 深色主题
- 电量 ≤ 15% 显示红色，充电时附充电标记
- 设备断连时变灰，并保留最后一次已知电量
- 竖排任务栏 / 任务栏不可用时，自动降级为托盘图标

点击弹出设备列表：每个设备可单独隐藏、手动刷新、开机自启、退出。

---

## 当前状态

| 模块 | 状态 |
|---|---|
| 项目文档、技术选型 | ✅ 完成 |
| 设备探测工具（`--dump`） | 📋 计划中 |
| 托盘图标（数据展示） | 📋 计划中 |
| 任务栏常驻文字 | 📋 计划中 |
| 设置项 / 开机自启 | 📋 计划中 |
| 厂商私有协议 | 📋 计划中 |

---

## 技术方案

电量来源分三层，从"零成本"到"要逆向"，逐层降级：

### 1. Windows 已知的蓝牙电量（通用，无需厂商协议）

Windows 自己在「设置 → 蓝牙和设备」里能显示电量的设备，这个值就存在 PnP 设备属性里。
用 `SetupDiGetClassDevsW` 枚举 `BTHENUM\` / `BTHLE\` / `BTHLEDEVICE\` 实例，读以下未被文档化的属性：

| 用途 | DEVPKEY | 类型 |
|---|---|---|
| 电量百分比 | `{104EA319-6EE2-4701-BD47-8DDBF425BBE5}, 2` | `DEVPROP_TYPE_BYTE` |
| 是否在线 | `{83DA6326-97A6-4088-9453-A1923F573B29}, 15` | `DEVPROP_TYPE_BOOLEAN` |
| 设备类型（CoD） | `{2BD67D8B-8BEB-48D5-87E0-6CDA3428040A}, 10` | `UINT32` |

同一物理设备的多个服务节点（如耳机的 A2DP 与 HFP）用 `DEVPKEY_Device_ContainerId` 归并；
在线状态以主设备节点为准 —— 音频走 A2DP 时 HFP 节点会显示"未连接"，但电量往往挂在它上面。

**优点：** 不连接设备、不消耗外设电量、覆盖所有蓝牙设备。
**限制：** 值由系统缓存，刷新时机不可控；且不出现在 Windows 设置里的设备读不到。

### 2. 标准 HID 电池字段（通用）

HID 规范里带电池语义的字段：

- `Usage Page 0x06 / Usage 0x20` — Battery Strength（外设最常见的写法）
- `Usage Page 0x85 / Usage 0x65` — Battery System 的 Relative State of Charge

流程：`HidD_GetPreparsedData` → `HidP_GetValueCaps` 找字段 → `HidD_GetFeature` /
`HidD_GetInputReport` 取原始值 → 按 `LogicalMin/Max` 归一化到 0–100。

已知例外：苹果设备（VID `0x05AC`）在 0–255 的量程里直接报百分比，需要特判。
数位板（`Usage Page 0x0D`）一律不读 —— 有设备被轮询后坐标轴翻转。

### 3. 厂商私有协议（按需）

只对"系统读不到、标准字段也没有"的设备实现，按 `VID/PID` 精确匹配，一个厂商一个文件：

- **Logitech** HID++ 2.0：特性发现后读 `0x1004` unified battery / `0x1000` battery level /
  `0x1001` battery voltage
- 其余厂商按实际持有的设备再补

---

## 任务栏实现要点

Windows 没有公开的"在任务栏上加自定义 UI"的 API，本项目采用的方式是
**给任务栏窗口挂一个分层子窗口**（不注入进程、不 hook、不需要管理员权限）：

- 父窗口 `Shell_TrayWnd`，必须以 `TrayNotifyWnd` 存在为前提
- 子窗口带 `WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_CHILD`，用 `UpdateLayeredWindow`
  逐像素合成（预乘 alpha），这样文字抗锯齿且能贴合任意任务栏背景
- 位置以 `TrayNotifyWnd` 为基准，约每秒校正一次，应对布局漂移
- 每个副屏的 `Shell_SecondaryTrayWnd` 挂一份镜像，各自跟随本地时钟
- 监听 `RegisterWindowMessageW(L"TaskbarCreated")`，explorer 重启后重新挂载
- Win11 的任务栏是 XAML 岛，与 Win10 的 `ReBarWindow32` 布局模型不同，需分别处理

图标用 `Segoe Fluent Icons`（回退 `Segoe MDL2 Assets`），文字用 `Segoe UI Variable Text`。

---

## 架构

```
src/
  main.cpp        入口、消息循环、轮询调度、托盘、设置、自启
  widget.cpp      任务栏分层子窗口：绘制、定位、多显示器镜像
  devices.cpp     设备模型与合并（同一设备多来源去重）
  providers/
    bt_devnode.cpp   PnP 属性里的蓝牙电量
    hid_generic.cpp  标准 HID 电池字段
    logitech.cpp     Logitech HID++ 2.0
  hid.cpp         HID 枚举与报文收发
```

每个 provider 输出统一的 `DeviceStatus`（名称、电量、充电中、在线、连接方式、来源键）。
设备以"容器 ID"或 `VID/PID` 为键去重，避免同一支耳机被报两次。

---

## 构建

计划支持 MSVC 与 MinGW-w64 两种工具链，产物为**单个 exe，无运行时依赖，无需管理员权限**。

```powershell
# 依赖：MSVC (Visual Studio Build Tools) 或 MinGW-w64
build.bat            # 或 make
```

> 代码尚未落地，此节会在 M1 之后补全实际命令。

---

## 路线图

- [ ] **M0 — 探测**：`battery_status --dump` 打印所有候选设备与读到的电量，用于验证可行性
- [ ] **M1 — 托盘**：托盘图标内绘制百分比，点击列出所有设备
- [ ] **M2 — 任务栏**：分层子窗口常驻显示，含副屏镜像与 explorer 重启恢复
- [ ] **M3 — 打磨**：深色/浅色主题、DPI 缩放、开机自启、配置持久化、低电量通知
- [ ] **M4 — 扩展**：按需增加厂商协议、多语言

---

## 非目标

- ❌ 不改动或注入 `explorer.exe`
- ❌ 不需要管理员权限
- ❌ 不常驻后台服务，不写驱动
- ❌ 不替代厂商软件（厂商软件开着也不冲突）
- ❌ 不支持 macOS / Linux（本仓库只针对 Windows）

---

## 许可

[MIT](LICENSE)
