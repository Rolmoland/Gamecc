# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GamePad Tuner - 一个纯前端的手柄调参工具网页，无需构建系统，直接在浏览器中打开 `index.html` 即可运行。

## Architecture

项目采用模块化设计，由三个文件组成：

- **index.html** - 页面结构
- **styles.css** - 样式（支持 dark/light/eye-care 三种主题，通过 `data-theme` 属性切换）
- **gamepad.js** - 业务逻辑，包含六个模块：
  - `ThemeManager` - 主题切换，状态存储在 localStorage
  - `ConfigManager` - 配置管理（死区、灵敏度、曲线、映射），支持导入/导出 JSON
  - `GamepadManager` - 手柄状态轮询（使用 Gamepad API），处理摇杆/扳机/按键/振动
  - `UIManager` - DOM 操作与事件绑定
  - `ModuleManager` - 手柄预览槽位配置（4个槽位可放置摇杆/十字键/按键任意组合）
  - `GamepadVisual` - 手柄图形实时状态更新

## Key Implementation Details

- 使用 `navigator.getGamepads()` 轮询手柄状态，通过 `requestAnimationFrame` 驱动
- 摇杆处理流程：原始值 → 漂移修正 → 死区过滤 → 响应曲线 → 灵敏度缩放
- CSS 变量定义主题色，主题切换通过修改 `document.documentElement` 的 `data-theme` 属性实现
- 配置持久化使用 localStorage，键名：
  - `gamepadConfig` - 手柄配置（死区/灵敏度/曲线/映射）
  - `gamepadTheme` - 主题
  - `gamepadModules` - 槽位配置（每个槽位的模块类型和样式）
  - `gamepadLayout` - 手柄布局预设（xbox/ps）
  - `gamepadVisualCollapsed` - 手柄预览卡片收起状态
- 槽位系统：4个位置（左上/左下/右上/右下），每个可放置 stick/dpad/buttons 类型模块
- 响应曲线类型：`linear`（线性）、`exponential`（指数）、`aggressive`（激进）、`relaxed`（平缓）

## Development

直接用浏览器打开 `index.html`，无需本地服务器。修改后刷新页面即可看到效果。
