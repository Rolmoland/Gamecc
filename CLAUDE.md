# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GamePad Tuner - 一个纯前端的手柄调参工具网页，无需构建系统，直接在浏览器中打开 `index.html` 即可运行。

## Architecture

项目采用模块化设计，由三个文件组成：

- **index.html** - 页面结构
- **styles.css** - 样式（支持 dark/light/eye-care 三种主题，通过 `data-theme` 属性切换）
- **gamepad.js** - 业务逻辑，包含四个核心模块：
  - `ThemeManager` - 主题切换，状态存储在 localStorage
  - `ConfigManager` - 配置管理（死区、灵敏度、曲线、映射），支持导入/导出 JSON
  - `GamepadManager` - 手柄状态轮询（使用 Gamepad API），处理摇杆/扳机/按键/振动
  - `UIManager` - DOM 操作与事件绑定

## Key Implementation Details

- 使用 `navigator.getGamepads()` 轮询手柄状态，通过 `requestAnimationFrame` 驱动
- 摇杆处理流程：原始值 → 漂移修正 → 死区过滤 → 响应曲线 → 灵敏度缩放
- CSS 变量定义主题色，主题切换通过修改 `document.documentElement` 的 `data-theme` 属性实现
- 配置持久化使用 localStorage，键名：`gamepadConfig`（配置）、`gamepadTheme`（主题）

## Development

直接用浏览器打开 `index.html`，无需本地服务器。修改后刷新页面即可看到效果。
