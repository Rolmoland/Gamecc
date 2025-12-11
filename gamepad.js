/**
 * GamePad Tuner - 手柄调参工具
 * 模块化设计，易于维护和扩展
 */

// ========================================
// 主题管理模块
// ========================================

const ThemeManager = {
    currentTheme: 'dark',

    init() {
        // 从本地存储读取主题
        const savedTheme = localStorage.getItem('gamepadTheme') || 'dark';
        this.setTheme(savedTheme);

        // 绑定主题切换按钮
        document.querySelectorAll('.theme-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                this.setTheme(btn.dataset.theme);
            });
        });
    },

    setTheme(theme) {
        this.currentTheme = theme;
        document.documentElement.setAttribute('data-theme', theme);
        localStorage.setItem('gamepadTheme', theme);

        // 更新按钮状态
        document.querySelectorAll('.theme-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.theme === theme);
        });
    }
};

// ========================================
// 配置管理模块
// ========================================

const ConfigManager = {
    // 默认配置
    defaults: {
        sticks: {
            left: {
                deadzone: 10,
                sensitivity: 100,
                curve: 'linear',
                driftX: 0,
                driftY: 0
            },
            right: {
                deadzone: 10,
                sensitivity: 100,
                curve: 'linear',
                driftX: 0,
                driftY: 0
            }
        },
        triggers: {
            lt: {
                deadzone: 5,
                sensitivity: 100
            },
            rt: {
                deadzone: 5,
                sensitivity: 100
            }
        },
        vibration: {
            left: 50,
            right: 50
        },
        buttonMapping: {} // 按键重映射表
    },

    // 当前配置
    current: null,

    // 初始化配置
    init() {
        const saved = localStorage.getItem('gamepadConfig');
        if (saved) {
            try {
                this.current = JSON.parse(saved);
                // 合并默认值以防配置不完整
                this.current = this.mergeDeep(this.defaults, this.current);
            } catch (e) {
                console.warn('配置加载失败，使用默认配置');
                this.current = JSON.parse(JSON.stringify(this.defaults));
            }
        } else {
            this.current = JSON.parse(JSON.stringify(this.defaults));
        }
    },

    // 保存配置
    save() {
        localStorage.setItem('gamepadConfig', JSON.stringify(this.current));
    },

    // 导出配置
    export() {
        const data = JSON.stringify(this.current, null, 2);
        const blob = new Blob([data], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `gamepad-config-${Date.now()}.json`;
        a.click();
        URL.revokeObjectURL(url);
    },

    // 导入配置
    import(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = (e) => {
                try {
                    const config = JSON.parse(e.target.result);
                    this.current = this.mergeDeep(this.defaults, config);
                    this.save();
                    resolve(this.current);
                } catch (err) {
                    reject(new Error('配置文件格式无效'));
                }
            };
            reader.onerror = () => reject(new Error('文件读取失败'));
            reader.readAsText(file);
        });
    },

    // 重置为默认
    reset() {
        this.current = JSON.parse(JSON.stringify(this.defaults));
        this.save();
    },

    // 深度合并对象
    mergeDeep(target, source) {
        const result = { ...target };
        for (const key in source) {
            if (source[key] instanceof Object && key in target) {
                result[key] = this.mergeDeep(target[key], source[key]);
            } else {
                result[key] = source[key];
            }
        }
        return result;
    }
};

// ========================================
// 手柄管理模块
// ========================================

const GamepadManager = {
    gamepads: {},
    activeGamepad: null,
    animationFrame: null,
    lastPollTime: 0,
    pollCount: 0,
    pollRate: 0,

    // 标准按键名称映射
    buttonNames: [
        'A', 'B', 'X', 'Y',
        'LB', 'RB', 'LT', 'RT',
        'Back', 'Start',
        'L3', 'R3',
        'Up', 'Down', 'Left', 'Right',
        'Home', 'Share'
    ],

    // 初始化
    init() {
        window.addEventListener('gamepadconnected', (e) => this.onConnect(e));
        window.addEventListener('gamepaddisconnected', (e) => this.onDisconnect(e));

        // 检查已连接的手柄
        const gamepads = navigator.getGamepads();
        for (const gp of gamepads) {
            if (gp) {
                this.gamepads[gp.index] = gp;
            }
        }

        this.startPolling();
    },

    // 手柄连接事件
    onConnect(e) {
        console.log('手柄已连接:', e.gamepad.id);
        this.gamepads[e.gamepad.index] = e.gamepad;

        if (!this.activeGamepad) {
            this.activeGamepad = e.gamepad.index;
        }

        UIManager.updateConnectionStatus(true, e.gamepad);
        UIManager.updateGamepadSelector(this.gamepads, this.activeGamepad);
        UIManager.initButtonsGrid(e.gamepad.buttons.length);
        UIManager.initMappingList(e.gamepad.buttons.length, this.buttonNames);
    },

    // 手柄断开事件
    onDisconnect(e) {
        console.log('手柄已断开:', e.gamepad.id);
        delete this.gamepads[e.gamepad.index];

        if (this.activeGamepad === e.gamepad.index) {
            const remaining = Object.keys(this.gamepads);
            this.activeGamepad = remaining.length > 0 ? parseInt(remaining[0]) : null;
        }

        if (Object.keys(this.gamepads).length === 0) {
            UIManager.updateConnectionStatus(false, null);
        } else {
            const gp = this.gamepads[this.activeGamepad];
            UIManager.updateConnectionStatus(true, gp);
        }
        UIManager.updateGamepadSelector(this.gamepads, this.activeGamepad);
    },

    // 切换活动手柄
    setActiveGamepad(index) {
        this.activeGamepad = index;
        const gp = this.gamepads[index];
        if (gp) {
            UIManager.updateConnectionStatus(true, gp);
            UIManager.initButtonsGrid(gp.buttons.length);
            UIManager.initMappingList(gp.buttons.length, this.buttonNames);
        }
    },

    // 开始轮询
    startPolling() {
        const poll = () => {
            this.pollGamepad();
            this.animationFrame = requestAnimationFrame(poll);
        };
        poll();
    },

    // 停止轮询
    stopPolling() {
        if (this.animationFrame) {
            cancelAnimationFrame(this.animationFrame);
            this.animationFrame = null;
        }
    },

    // 轮询手柄状态
    pollGamepad() {
        // 计算轮询率
        const now = performance.now();
        this.pollCount++;
        if (now - this.lastPollTime >= 1000) {
            this.pollRate = this.pollCount;
            this.pollCount = 0;
            this.lastPollTime = now;
            UIManager.updatePollRate(this.pollRate);
        }

        // 获取最新状态
        const gamepads = navigator.getGamepads();
        for (const gp of gamepads) {
            if (gp) {
                this.gamepads[gp.index] = gp;
            }
        }

        if (this.activeGamepad === null) return;

        const gp = this.gamepads[this.activeGamepad];
        if (!gp) return;

        // 更新摇杆
        this.updateStick('left', gp.axes[0], gp.axes[1]);
        this.updateStick('right', gp.axes[2], gp.axes[3]);

        // 更新扳机键
        this.updateTrigger('lt', gp.buttons[6]?.value || 0);
        this.updateTrigger('rt', gp.buttons[7]?.value || 0);

        // 更新按键
        this.updateButtons(gp.buttons);

        // 更新时间戳
        UIManager.updateTimestamp();
    },

    // 更新摇杆状态
    updateStick(side, rawX, rawY) {
        const config = ConfigManager.current.sticks[side];

        // 应用漂移修正
        let x = rawX - config.driftX;
        let y = rawY - config.driftY;

        // 计算幅度
        const magnitude = Math.sqrt(x * x + y * y);

        // 应用死区
        const deadzone = config.deadzone / 100;
        if (magnitude < deadzone) {
            x = 0;
            y = 0;
        } else {
            // 重新映射到 0-1 范围
            const normalized = (magnitude - deadzone) / (1 - deadzone);
            const scale = normalized / magnitude;
            x *= scale;
            y *= scale;
        }

        // 应用响应曲线
        const curvedMag = this.applyCurve(Math.min(1, Math.sqrt(x * x + y * y)), config.curve);
        if (x !== 0 || y !== 0) {
            const currentMag = Math.sqrt(x * x + y * y);
            if (currentMag > 0) {
                x = (x / currentMag) * curvedMag;
                y = (y / currentMag) * curvedMag;
            }
        }

        // 应用灵敏度
        const sensitivity = config.sensitivity / 100;
        x = Math.max(-1, Math.min(1, x * sensitivity));
        y = Math.max(-1, Math.min(1, y * sensitivity));

        UIManager.updateStickVisual(side, x, y);
        GamepadVisual.updateStick(side, x, y);
    },

    // 应用响应曲线
    applyCurve(value, curve) {
        switch (curve) {
            case 'exponential':
                return value * value;
            case 'aggressive':
                return value * value * value;
            case 'relaxed':
                return Math.sqrt(value);
            default: // linear
                return value;
        }
    },

    // 更新扳机键状态
    updateTrigger(trigger, rawValue) {
        const config = ConfigManager.current.triggers[trigger];

        // 应用死区
        const deadzone = config.deadzone / 100;
        let value = rawValue;
        if (value < deadzone) {
            value = 0;
        } else {
            value = (value - deadzone) / (1 - deadzone);
        }

        // 应用灵敏度
        const sensitivity = config.sensitivity / 100;
        value = Math.min(1, value * sensitivity);

        UIManager.updateTriggerVisual(trigger, value);
    },

    // 更新按键状态
    updateButtons(buttons) {
        const mapping = ConfigManager.current.buttonMapping;

        buttons.forEach((button, index) => {
            // 应用重映射
            const mappedIndex = mapping[index] !== undefined ? mapping[index] : index;
            UIManager.updateButtonState(index, button.pressed, mappedIndex);
            GamepadVisual.updateButton(index, button.pressed);
        });
    },

    // 振动测试
    async vibrate(leftIntensity, rightIntensity, duration = 500) {
        if (this.activeGamepad === null) return;

        const gp = this.gamepads[this.activeGamepad];
        if (!gp || !gp.vibrationActuator) {
            console.warn('此手柄不支持振动功能');
            return;
        }

        try {
            await gp.vibrationActuator.playEffect('dual-rumble', {
                startDelay: 0,
                duration: duration,
                weakMagnitude: rightIntensity / 100,
                strongMagnitude: leftIntensity / 100
            });
        } catch (e) {
            console.warn('振动功能调用失败:', e);
        }
    },

    // 停止振动
    async stopVibration() {
        if (this.activeGamepad === null) return;

        const gp = this.gamepads[this.activeGamepad];
        if (!gp || !gp.vibrationActuator) return;

        try {
            await gp.vibrationActuator.reset();
        } catch (e) {
            console.warn('停止振动失败:', e);
        }
    },

    // 校准摇杆漂移
    calibrateDrift() {
        if (this.activeGamepad === null) return;

        const gp = this.gamepads[this.activeGamepad];
        if (!gp) return;

        // 记录当前摇杆位置作为漂移偏移量
        ConfigManager.current.sticks.left.driftX = gp.axes[0];
        ConfigManager.current.sticks.left.driftY = gp.axes[1];
        ConfigManager.current.sticks.right.driftX = gp.axes[2];
        ConfigManager.current.sticks.right.driftY = gp.axes[3];

        ConfigManager.save();

        console.log('漂移校准完成:', {
            left: { x: gp.axes[0], y: gp.axes[1] },
            right: { x: gp.axes[2], y: gp.axes[3] }
        });

        return true;
    }
};

// ========================================
// UI 管理模块
// ========================================

const UIManager = {
    elements: {},
    isRemapping: false,
    remappingButtonIndex: null,

    // 初始化 UI
    init() {
        this.cacheElements();
        this.bindEvents();
        this.applyConfigToUI();
    },

    // 缓存 DOM 元素
    cacheElements() {
        this.elements = {
            // 状态
            statusIndicator: document.getElementById('status-indicator'),
            deviceInfo: document.getElementById('device-info'),
            gamepadSelector: document.getElementById('gamepad-selector'),
            gamepadSelect: document.getElementById('gamepad-select'),

            // 摇杆
            dotLeft: document.getElementById('dot-left'),
            dotRight: document.getElementById('dot-right'),
            leftX: document.getElementById('left-x'),
            leftY: document.getElementById('left-y'),
            rightX: document.getElementById('right-x'),
            rightY: document.getElementById('right-y'),
            deadzoneLeft: document.getElementById('deadzone-left'),
            deadzoneRight: document.getElementById('deadzone-right'),

            // 扳机
            triggerLt: document.getElementById('trigger-lt'),
            triggerRt: document.getElementById('trigger-rt'),
            ltValue: document.getElementById('lt-value'),
            rtValue: document.getElementById('rt-value'),
            triggerDeadzoneLt: document.getElementById('trigger-deadzone-lt'),
            triggerDeadzoneRt: document.getElementById('trigger-deadzone-rt'),

            // 按键
            buttonsGrid: document.getElementById('buttons-grid'),
            mappingList: document.getElementById('mapping-list'),

            // 设置滑块
            leftDeadzone: document.getElementById('left-deadzone'),
            leftSensitivity: document.getElementById('left-sensitivity'),
            leftCurve: document.getElementById('left-curve'),
            rightDeadzone: document.getElementById('right-deadzone'),
            rightSensitivity: document.getElementById('right-sensitivity'),
            rightCurve: document.getElementById('right-curve'),
            ltDeadzone: document.getElementById('lt-deadzone'),
            ltSensitivity: document.getElementById('lt-sensitivity'),
            rtDeadzone: document.getElementById('rt-deadzone'),
            rtSensitivity: document.getElementById('rt-sensitivity'),
            vibrationLeft: document.getElementById('vibration-left'),
            vibrationRight: document.getElementById('vibration-right'),

            // 设置值显示
            leftDeadzoneValue: document.getElementById('left-deadzone-value'),
            leftSensitivityValue: document.getElementById('left-sensitivity-value'),
            rightDeadzoneValue: document.getElementById('right-deadzone-value'),
            rightSensitivityValue: document.getElementById('right-sensitivity-value'),
            ltDeadzoneValue: document.getElementById('lt-deadzone-value'),
            ltSensitivityValue: document.getElementById('lt-sensitivity-value'),
            rtDeadzoneValue: document.getElementById('rt-deadzone-value'),
            rtSensitivityValue: document.getElementById('rt-sensitivity-value'),
            vibrationLeftValue: document.getElementById('vibration-left-value'),
            vibrationRightValue: document.getElementById('vibration-right-value'),

            // 按钮
            btnExport: document.getElementById('btn-export'),
            btnImport: document.getElementById('btn-import'),
            btnReset: document.getElementById('btn-reset'),
            btnCalibrate: document.getElementById('btn-calibrate'),
            btnVibrate: document.getElementById('btn-vibrate'),
            btnStopVibrate: document.getElementById('btn-stop-vibrate'),
            fileInput: document.getElementById('file-input'),

            // 模态框
            remapModal: document.getElementById('remap-modal'),
            modalClose: document.getElementById('modal-close'),
            currentButtonName: document.getElementById('current-button-name'),
            btnCancelRemap: document.getElementById('btn-cancel-remap'),
            btnResetMapping: document.getElementById('btn-reset-mapping'),

            // 状态栏
            pollRate: document.getElementById('poll-rate'),
            timestamp: document.getElementById('timestamp')
        };
    },

    // 绑定滑块和输入框双向同步
    bindSliderInput(slider, input, onChange) {
        const min = parseInt(slider.min);
        const max = parseInt(slider.max);

        // 滑块变化 -> 更新输入框
        slider.addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            input.value = value;
            onChange(value);
        });

        // 输入框变化 -> 更新滑块
        input.addEventListener('input', (e) => {
            let value = parseInt(e.target.value) || min;
            value = Math.max(min, Math.min(max, value));
            slider.value = value;
            onChange(value);
        });

        // 输入框失焦时校验范围
        input.addEventListener('blur', (e) => {
            let value = parseInt(e.target.value) || min;
            value = Math.max(min, Math.min(max, value));
            e.target.value = value;
            slider.value = value;
        });
    },

    // 绑定事件
    bindEvents() {
        // 连接状态卡片点击切换展开/最小化
        const connectionCard = document.getElementById('connection-card');
        connectionCard.addEventListener('click', (e) => {
            // 只有在最小化状态下点击才展开
            if (connectionCard.classList.contains('minimized')) {
                connectionCard.classList.remove('minimized');
                // 3秒后如果还是连接状态，自动最小化
                setTimeout(() => {
                    if (GamepadManager.activeGamepad !== null) {
                        connectionCard.classList.add('minimized');
                    }
                }, 3000);
            }
        });

        // 手柄选择
        this.elements.gamepadSelect.addEventListener('change', (e) => {
            GamepadManager.setActiveGamepad(parseInt(e.target.value));
        });

        // 摇杆设置 - 左摇杆
        this.bindSliderInput(
            this.elements.leftDeadzone,
            this.elements.leftDeadzoneValue,
            (value) => {
                ConfigManager.current.sticks.left.deadzone = value;
                this.updateDeadzoneVisual('left', value);
                ConfigManager.save();
            }
        );

        this.bindSliderInput(
            this.elements.leftSensitivity,
            this.elements.leftSensitivityValue,
            (value) => {
                ConfigManager.current.sticks.left.sensitivity = value;
                ConfigManager.save();
            }
        );

        this.elements.leftCurve.addEventListener('change', (e) => {
            ConfigManager.current.sticks.left.curve = e.target.value;
            ConfigManager.save();
        });

        // 摇杆设置 - 右摇杆
        this.bindSliderInput(
            this.elements.rightDeadzone,
            this.elements.rightDeadzoneValue,
            (value) => {
                ConfigManager.current.sticks.right.deadzone = value;
                this.updateDeadzoneVisual('right', value);
                ConfigManager.save();
            }
        );

        this.bindSliderInput(
            this.elements.rightSensitivity,
            this.elements.rightSensitivityValue,
            (value) => {
                ConfigManager.current.sticks.right.sensitivity = value;
                ConfigManager.save();
            }
        );

        this.elements.rightCurve.addEventListener('change', (e) => {
            ConfigManager.current.sticks.right.curve = e.target.value;
            ConfigManager.save();
        });

        // 扳机设置 - LT
        this.bindSliderInput(
            this.elements.ltDeadzone,
            this.elements.ltDeadzoneValue,
            (value) => {
                ConfigManager.current.triggers.lt.deadzone = value;
                this.elements.triggerDeadzoneLt.style.width = `${value}%`;
                ConfigManager.save();
            }
        );

        this.bindSliderInput(
            this.elements.ltSensitivity,
            this.elements.ltSensitivityValue,
            (value) => {
                ConfigManager.current.triggers.lt.sensitivity = value;
                ConfigManager.save();
            }
        );

        // 扳机设置 - RT
        this.bindSliderInput(
            this.elements.rtDeadzone,
            this.elements.rtDeadzoneValue,
            (value) => {
                ConfigManager.current.triggers.rt.deadzone = value;
                this.elements.triggerDeadzoneRt.style.width = `${value}%`;
                ConfigManager.save();
            }
        );

        this.bindSliderInput(
            this.elements.rtSensitivity,
            this.elements.rtSensitivityValue,
            (value) => {
                ConfigManager.current.triggers.rt.sensitivity = value;
                ConfigManager.save();
            }
        );

        // 振动设置
        this.bindSliderInput(
            this.elements.vibrationLeft,
            this.elements.vibrationLeftValue,
            (value) => {
                ConfigManager.current.vibration.left = value;
                ConfigManager.save();
            }
        );

        this.bindSliderInput(
            this.elements.vibrationRight,
            this.elements.vibrationRightValue,
            (value) => {
                ConfigManager.current.vibration.right = value;
                ConfigManager.save();
            }
        );

        // 振动测试
        this.elements.btnVibrate.addEventListener('click', () => {
            const left = ConfigManager.current.vibration.left;
            const right = ConfigManager.current.vibration.right;
            GamepadManager.vibrate(left, right, 1000);
        });

        this.elements.btnStopVibrate.addEventListener('click', () => {
            GamepadManager.stopVibration();
        });

        // 校准
        this.elements.btnCalibrate.addEventListener('click', () => {
            if (GamepadManager.calibrateDrift()) {
                alert('校准完成！摇杆漂移已记录。');
            }
        });

        // 配置导入导出
        this.elements.btnExport.addEventListener('click', () => {
            ConfigManager.export();
        });

        this.elements.btnImport.addEventListener('click', () => {
            this.elements.fileInput.click();
        });

        this.elements.fileInput.addEventListener('change', async (e) => {
            const file = e.target.files[0];
            if (file) {
                try {
                    await ConfigManager.import(file);
                    this.applyConfigToUI();
                    alert('配置导入成功！');
                } catch (err) {
                    alert('导入失败：' + err.message);
                }
            }
            e.target.value = '';
        });

        this.elements.btnReset.addEventListener('click', () => {
            if (confirm('确定要恢复默认设置吗？所有自定义配置将被清除。')) {
                ConfigManager.reset();
                this.applyConfigToUI();
                alert('已恢复默认设置。');
            }
        });

        // 模态框
        this.elements.modalClose.addEventListener('click', () => this.closeRemapModal());
        this.elements.btnCancelRemap.addEventListener('click', () => this.closeRemapModal());
        this.elements.btnResetMapping.addEventListener('click', () => {
            if (this.remappingButtonIndex !== null) {
                delete ConfigManager.current.buttonMapping[this.remappingButtonIndex];
                ConfigManager.save();
                this.updateMappingItem(this.remappingButtonIndex);
                this.closeRemapModal();
            }
        });

        // 点击模态框背景关闭
        this.elements.remapModal.addEventListener('click', (e) => {
            if (e.target === this.elements.remapModal) {
                this.closeRemapModal();
            }
        });

        // 键盘事件关闭模态框
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && this.isRemapping) {
                this.closeRemapModal();
            }
        });
    },

    // 将配置应用到 UI
    applyConfigToUI() {
        const config = ConfigManager.current;

        // 摇杆设置
        this.elements.leftDeadzone.value = config.sticks.left.deadzone;
        this.elements.leftDeadzoneValue.value = config.sticks.left.deadzone;
        this.elements.leftSensitivity.value = config.sticks.left.sensitivity;
        this.elements.leftSensitivityValue.value = config.sticks.left.sensitivity;
        this.elements.leftCurve.value = config.sticks.left.curve;

        this.elements.rightDeadzone.value = config.sticks.right.deadzone;
        this.elements.rightDeadzoneValue.value = config.sticks.right.deadzone;
        this.elements.rightSensitivity.value = config.sticks.right.sensitivity;
        this.elements.rightSensitivityValue.value = config.sticks.right.sensitivity;
        this.elements.rightCurve.value = config.sticks.right.curve;

        this.updateDeadzoneVisual('left', config.sticks.left.deadzone);
        this.updateDeadzoneVisual('right', config.sticks.right.deadzone);

        // 扳机设置
        this.elements.ltDeadzone.value = config.triggers.lt.deadzone;
        this.elements.ltDeadzoneValue.value = config.triggers.lt.deadzone;
        this.elements.ltSensitivity.value = config.triggers.lt.sensitivity;
        this.elements.ltSensitivityValue.value = config.triggers.lt.sensitivity;
        this.elements.triggerDeadzoneLt.style.width = `${config.triggers.lt.deadzone}%`;

        this.elements.rtDeadzone.value = config.triggers.rt.deadzone;
        this.elements.rtDeadzoneValue.value = config.triggers.rt.deadzone;
        this.elements.rtSensitivity.value = config.triggers.rt.sensitivity;
        this.elements.rtSensitivityValue.value = config.triggers.rt.sensitivity;
        this.elements.triggerDeadzoneRt.style.width = `${config.triggers.rt.deadzone}%`;

        // 振动设置
        this.elements.vibrationLeft.value = config.vibration.left;
        this.elements.vibrationLeftValue.value = config.vibration.left;
        this.elements.vibrationRight.value = config.vibration.right;
        this.elements.vibrationRightValue.value = config.vibration.right;

        // 更新映射列表
        this.refreshMappingList();
    },

    // 更新连接状态
    updateConnectionStatus(connected, gamepad) {
        const indicator = this.elements.statusIndicator;
        const info = this.elements.deviceInfo;
        const card = document.getElementById('connection-card');

        if (connected && gamepad) {
            indicator.textContent = '已连接';
            indicator.classList.add('connected');

            info.classList.add('active');
            info.innerHTML = `
                <p class="device-name">${gamepad.id}</p>
                <p class="device-vendor">按键数: ${gamepad.buttons.length} | 轴数: ${gamepad.axes.length}</p>
            `;

            // 连接后最小化卡片
            card.classList.add('minimized');
        } else {
            indicator.textContent = '未连接';
            indicator.classList.remove('connected');

            info.classList.remove('active');
            info.innerHTML = '<p class="hint">请连接手柄并按下任意按键激活</p>';

            // 断开后恢复卡片
            card.classList.remove('minimized');
        }
    },

    // 更新手柄选择器
    updateGamepadSelector(gamepads, activeIndex) {
        const selector = this.elements.gamepadSelector;
        const select = this.elements.gamepadSelect;

        const keys = Object.keys(gamepads);
        if (keys.length > 1) {
            selector.style.display = 'flex';
            select.innerHTML = keys.map(key => {
                const gp = gamepads[key];
                return `<option value="${key}" ${key == activeIndex ? 'selected' : ''}>${gp.id}</option>`;
            }).join('');
        } else {
            selector.style.display = 'none';
        }
    },

    // 初始化按键网格
    initButtonsGrid(buttonCount) {
        const grid = this.elements.buttonsGrid;
        grid.innerHTML = '';

        for (let i = 0; i < buttonCount; i++) {
            const name = GamepadManager.buttonNames[i] || `B${i}`;
            const item = document.createElement('div');
            item.className = 'button-item';
            item.dataset.index = i;
            item.innerHTML = `
                <span class="button-name">${name}</span>
                <span class="button-index">#${i}</span>
            `;
            grid.appendChild(item);
        }
    },

    // 初始化映射列表
    initMappingList(buttonCount, buttonNames) {
        const list = this.elements.mappingList;
        list.innerHTML = '';

        for (let i = 0; i < buttonCount; i++) {
            const name = buttonNames[i] || `Button ${i}`;
            const item = document.createElement('div');
            item.className = 'mapping-item';
            item.dataset.index = i;

            const mappedIndex = ConfigManager.current.buttonMapping[i];
            const mappedName = mappedIndex !== undefined
                ? (buttonNames[mappedIndex] || `B${mappedIndex}`)
                : name;

            if (mappedIndex !== undefined && mappedIndex !== i) {
                item.classList.add('remapped');
            }

            item.innerHTML = `
                <span class="original">${name} (#${i})</span>
                <span class="mapped">${mappedName}</span>
            `;

            item.addEventListener('click', () => this.openRemapModal(i, name));
            list.appendChild(item);
        }
    },

    // 刷新映射列表
    refreshMappingList() {
        const items = this.elements.mappingList.querySelectorAll('.mapping-item');
        items.forEach(item => {
            const index = parseInt(item.dataset.index);
            this.updateMappingItem(index);
        });
    },

    // 更新单个映射项
    updateMappingItem(index) {
        const item = this.elements.mappingList.querySelector(`[data-index="${index}"]`);
        if (!item) return;

        const name = GamepadManager.buttonNames[index] || `Button ${index}`;
        const mappedIndex = ConfigManager.current.buttonMapping[index];
        const mappedName = mappedIndex !== undefined
            ? (GamepadManager.buttonNames[mappedIndex] || `B${mappedIndex}`)
            : name;

        item.classList.toggle('remapped', mappedIndex !== undefined && mappedIndex !== index);
        item.querySelector('.mapped').textContent = mappedName;
    },

    // 打开重映射模态框
    openRemapModal(buttonIndex, buttonName) {
        this.isRemapping = true;
        this.remappingButtonIndex = buttonIndex;
        this.elements.currentButtonName.textContent = buttonName;
        this.elements.remapModal.classList.add('active');

        // 监听手柄按键
        this.remapPollInterval = setInterval(() => {
            if (GamepadManager.activeGamepad === null) return;

            const gp = GamepadManager.gamepads[GamepadManager.activeGamepad];
            if (!gp) return;

            for (let i = 0; i < gp.buttons.length; i++) {
                if (gp.buttons[i].pressed && i !== buttonIndex) {
                    // 设置映射
                    ConfigManager.current.buttonMapping[buttonIndex] = i;
                    ConfigManager.save();
                    this.updateMappingItem(buttonIndex);
                    this.closeRemapModal();
                    break;
                }
            }
        }, 50);
    },

    // 关闭重映射模态框
    closeRemapModal() {
        this.isRemapping = false;
        this.remappingButtonIndex = null;
        this.elements.remapModal.classList.remove('active');

        if (this.remapPollInterval) {
            clearInterval(this.remapPollInterval);
            this.remapPollInterval = null;
        }
    },

    // 更新摇杆可视化
    updateStickVisual(side, x, y) {
        const dot = side === 'left' ? this.elements.dotLeft : this.elements.dotRight;
        const xDisplay = side === 'left' ? this.elements.leftX : this.elements.rightX;
        const yDisplay = side === 'left' ? this.elements.leftY : this.elements.rightY;

        // 计算位置 (将 -1~1 映射到 0~100%)
        const posX = 50 + (x * 40); // 40% 是半径，留出点的大小
        const posY = 50 + (y * 40);

        dot.style.left = `${posX}%`;
        dot.style.top = `${posY}%`;

        // 活动状态
        const isActive = Math.abs(x) > 0.1 || Math.abs(y) > 0.1;
        dot.classList.toggle('active', isActive);

        // 更新数值显示
        xDisplay.textContent = x.toFixed(2);
        yDisplay.textContent = y.toFixed(2);
    },

    // 更新死区可视化
    updateDeadzoneVisual(side, value) {
        const element = side === 'left' ? this.elements.deadzoneLeft : this.elements.deadzoneRight;
        element.style.width = `${value}%`;
        element.style.height = `${value}%`;
    },

    // 更新扳机可视化
    updateTriggerVisual(trigger, value) {
        const fill = trigger === 'lt' ? this.elements.triggerLt : this.elements.triggerRt;
        const display = trigger === 'lt' ? this.elements.ltValue : this.elements.rtValue;

        fill.style.width = `${value * 100}%`;
        display.textContent = `${Math.round(value * 100)}%`;
    },

    // 更新按键状态
    updateButtonState(index, pressed, mappedIndex) {
        const item = this.elements.buttonsGrid.querySelector(`[data-index="${index}"]`);
        if (item) {
            item.classList.toggle('pressed', pressed);
        }
    },

    // 更新轮询率
    updatePollRate(rate) {
        this.elements.pollRate.textContent = `轮询率: ${rate} Hz`;
    },

    // 更新时间戳
    updateTimestamp() {
        const now = new Date();
        const time = now.toLocaleTimeString('zh-CN', { hour12: false });
        this.elements.timestamp.textContent = `最后更新: ${time}`;
    }
};

// ========================================
// 模块样式管理
// ========================================

const ModuleManager = {
    // 模块类型定义
    moduleTypes: {
        stick: { name: '摇杆', icon: '🎮' },
        dpad: { name: '十字键', icon: '✚' },
        buttons: { name: '按键', icon: '🔘' }
    },

    // 各类型可用样式
    moduleStyles: {
        stick: [
            { id: 'standard', name: '标准' },
            { id: 'compact', name: '紧凑' },
            { id: 'pro', name: 'Pro' }
        ],
        dpad: [
            { id: 'standard', name: '十字' },
            { id: 'disc', name: '圆盘' }
        ],
        buttons: [
            { id: 'standard', name: 'Xbox' },
            { id: 'ps', name: 'PS' },
            { id: 'nintendo', name: 'NS' }
        ]
    },

    // 槽位名称
    slotNames: {
        'top-left': '左上',
        'bottom-left': '左下',
        'top-right': '右上',
        'bottom-right': '右下'
    },

    // 预设布局
    presets: {
        xbox: {
            'top-left': { type: 'stick', style: 'standard', label: 'L' },
            'bottom-left': { type: 'dpad', style: 'standard' },
            'top-right': { type: 'buttons', style: 'standard' },
            'bottom-right': { type: 'stick', style: 'standard', label: 'R' }
        },
        ps: {
            'top-left': { type: 'dpad', style: 'standard' },
            'bottom-left': { type: 'stick', style: 'standard', label: 'L' },
            'top-right': { type: 'buttons', style: 'ps' },
            'bottom-right': { type: 'stick', style: 'standard', label: 'R' }
        },
        ns: {
            'top-left': { type: 'stick', style: 'standard', label: 'L' },
            'bottom-left': { type: 'dpad', style: 'disc' },
            'top-right': { type: 'buttons', style: 'nintendo' },
            'bottom-right': { type: 'stick', style: 'standard', label: 'R' }
        }
    },

    currentSlot: null,
    currentConfig: {},
    currentLayout: 'xbox',
    isCollapsed: false,
    dragSourceSlot: null, // 拖拽源槽位

    init() {
        // 从 localStorage 加载配置
        const saved = localStorage.getItem('gamepadModules');
        if (saved) {
            try {
                this.currentConfig = JSON.parse(saved);
            } catch (e) {
                this.currentConfig = {};
            }
        }

        // 如果没有配置，使用默认 Xbox 布局
        if (Object.keys(this.currentConfig).length === 0) {
            this.currentConfig = JSON.parse(JSON.stringify(this.presets.xbox));
            this.saveConfig();
        }

        // 加载布局配置
        this.currentLayout = localStorage.getItem('gamepadLayout') || 'xbox';

        // 加载收起状态
        this.isCollapsed = localStorage.getItem('gamepadVisualCollapsed') === 'true';
        const card = document.getElementById('gamepad-visual-card');
        if (this.isCollapsed) {
            card.classList.add('collapsed');
        }

        // 渲染所有槽位
        this.renderAllSlots();

        // 更新布局按钮状态
        this.updateLayoutButtons();

        // 绑定布局切换按钮（改为应用预设，需确认）
        document.querySelectorAll('.layout-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                this.applyPreset(btn.dataset.layout);
            });
        });

        // 绑定槽位事件（点击 + 拖拽）
        this.bindSlotEvents();

        // 绑定确认按钮 - 收起卡片
        document.getElementById('btn-confirm-module').addEventListener('click', () => {
            this.collapseCard();
        });

        // 绑定卡片点击展开
        card.addEventListener('click', (e) => {
            if (this.isCollapsed && e.target.closest('.card-header')) {
                this.expandCard();
            }
        });

        // 绑定模态框关闭
        document.getElementById('module-modal-close').addEventListener('click', () => {
            this.closeModal();
        });

        document.getElementById('module-modal').addEventListener('click', (e) => {
            if (e.target.id === 'module-modal') {
                this.closeModal();
            }
        });

        // 绑定模态框内的事件（事件委托）
        this.bindModalEvents();
    },

    // 绑定槽位事件（使用事件委托优化性能）
    bindSlotEvents() {
        const gamepadFace = document.querySelector('.gamepad-face');
        if (!gamepadFace) return;

        // 设置所有槽位可拖拽
        gamepadFace.querySelectorAll('.slot').forEach(slot => {
            slot.setAttribute('draggable', 'true');
        });

        // 点击事件委托
        gamepadFace.addEventListener('click', (e) => {
            if (this.dragSourceSlot) return;
            const slot = e.target.closest('.slot');
            if (slot) {
                e.stopPropagation();
                this.openSlotModal(slot.dataset.slot);
            }
        });

        // 拖拽开始
        gamepadFace.addEventListener('dragstart', (e) => {
            const slot = e.target.closest('.slot');
            if (!slot) return;
            this.dragSourceSlot = slot.dataset.slot;
            slot.classList.add('dragging');
            gamepadFace.classList.add('dragging-active');
            e.dataTransfer.effectAllowed = 'move';
        });

        // 拖拽结束
        gamepadFace.addEventListener('dragend', (e) => {
            const slot = e.target.closest('.slot');
            if (slot) slot.classList.remove('dragging');
            gamepadFace.querySelectorAll('.slot').forEach(s => s.classList.remove('drag-over'));
            gamepadFace.classList.remove('dragging-active');
            this.dragSourceSlot = null;
        });

        // 拖拽经过
        gamepadFace.addEventListener('dragover', (e) => {
            e.preventDefault();
            const slot = e.target.closest('.slot');
            if (slot && slot.dataset.slot !== this.dragSourceSlot) {
                e.dataTransfer.dropEffect = 'move';
                slot.classList.add('drag-over');
            }
        });

        // 拖拽离开
        gamepadFace.addEventListener('dragleave', (e) => {
            const slot = e.target.closest('.slot');
            if (slot) slot.classList.remove('drag-over');
        });

        // 放置
        gamepadFace.addEventListener('drop', (e) => {
            e.preventDefault();
            const slot = e.target.closest('.slot');
            if (slot) {
                slot.classList.remove('drag-over');
                const targetSlot = slot.dataset.slot;
                if (this.dragSourceSlot && targetSlot !== this.dragSourceSlot) {
                    this.swapSlots(this.dragSourceSlot, targetSlot);
                }
            }
        });
    },

    // 交换两个槽位的内容
    swapSlots(slot1, slot2) {
        const config1 = this.currentConfig[slot1];
        const config2 = this.currentConfig[slot2];

        // 交换配置
        this.currentConfig[slot1] = config2;
        this.currentConfig[slot2] = config1;

        // 重新渲染
        this.renderSlot(slot1);
        this.renderSlot(slot2);

        // 保存
        this.saveConfig();
    },

    // 应用预设布局（带确认）
    applyPreset(layout) {
        // 检查当前配置是否与预设相同
        const preset = this.presets[layout];
        const isSame = JSON.stringify(this.currentConfig) === JSON.stringify(preset);

        if (isSame) {
            // 已经是该预设，只更新按钮状态
            this.currentLayout = layout;
            localStorage.setItem('gamepadLayout', layout);
            this.updateLayoutButtons();
            return;
        }

        // 应用预设
        if (confirm(`应用 ${layout.toUpperCase()} 预设布局？\n当前自定义配置将被覆盖。`)) {
            this.currentLayout = layout;
            localStorage.setItem('gamepadLayout', layout);
            this.currentConfig = JSON.parse(JSON.stringify(preset));
            this.saveConfig();
            this.renderAllSlots();
            this.updateLayoutButtons();
        }
    },

    // 更新布局按钮状态
    updateLayoutButtons() {
        document.querySelectorAll('.layout-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.layout === this.currentLayout);
        });
    },

    // 渲染所有槽位
    renderAllSlots() {
        Object.keys(this.slotNames).forEach(slotId => {
            this.renderSlot(slotId);
        });
    },

    // 渲染单个槽位
    renderSlot(slotId) {
        const slotEl = document.querySelector(`[data-slot="${slotId}"]`);
        if (!slotEl) return;

        const config = this.currentConfig[slotId];
        if (!config) return;

        let html = '';
        const style = config.style || 'standard';

        switch (config.type) {
            case 'stick':
                const stickId = slotId.includes('left') ? 'visual-stick-left' :
                               (slotId.includes('right') && slotId.includes('bottom')) ? 'visual-stick-right' :
                               `visual-stick-${slotId}`;
                html = `
                    <div class="stick-module" data-style="${style}">
                        <div class="module-ring"></div>
                        <div class="module-stick" id="${stickId}"></div>
                        ${config.label ? `<span class="module-label">${config.label}</span>` : ''}
                    </div>
                `;
                break;

            case 'dpad':
                html = `
                    <div class="dpad-module" data-style="${style}">
                        <div class="dpad-up" id="dpad-up-${slotId}"></div>
                        <div class="dpad-right" id="dpad-right-${slotId}"></div>
                        <div class="dpad-down" id="dpad-down-${slotId}"></div>
                        <div class="dpad-left" id="dpad-left-${slotId}"></div>
                        <div class="dpad-center"></div>
                    </div>
                `;
                break;

            case 'buttons':
                html = `
                    <div class="buttons-module" data-style="${style}">
                        <div class="face-btn btn-y" id="face-y-${slotId}">Y</div>
                        <div class="face-btn btn-x" id="face-x-${slotId}">X</div>
                        <div class="face-btn btn-b" id="face-b-${slotId}">B</div>
                        <div class="face-btn btn-a" id="face-a-${slotId}">A</div>
                    </div>
                `;
                break;
        }

        slotEl.innerHTML = html;
    },

    // 打开槽位配置弹窗
    openSlotModal(slotId) {
        this.currentSlot = slotId;
        const modal = document.getElementById('module-modal');
        const nameEl = document.getElementById('current-slot-name');
        const typeGrid = document.getElementById('module-type-grid');

        nameEl.textContent = this.slotNames[slotId];

        const config = this.currentConfig[slotId] || { type: 'stick', style: 'standard' };

        // 生成模块类型选项
        typeGrid.innerHTML = Object.keys(this.moduleTypes).map(type => {
            const info = this.moduleTypes[type];
            return `
                <div class="type-option ${config.type === type ? 'selected' : ''}" data-type="${type}">
                    <div class="type-icon">${info.icon}</div>
                    <div class="type-name">${info.name}</div>
                </div>
            `;
        }).join('');

        // 生成样式选项
        this.renderStyleOptions(config.type, config.style);

        modal.classList.add('active');
    },

    // 渲染样式选项
    renderStyleOptions(type, currentStyle) {
        const styleGrid = document.getElementById('module-style-grid');
        const styles = this.moduleStyles[type] || [];

        styleGrid.innerHTML = styles.map(s => `
            <div class="style-option ${s.id === currentStyle ? 'selected' : ''}" data-style="${s.id}">
                ${s.name}
            </div>
        `).join('');
    },

    // 绑定模态框内事件（使用事件委托，只绑定一次）
    bindModalEvents() {
        const typeGrid = document.getElementById('module-type-grid');
        const styleGrid = document.getElementById('module-style-grid');

        // 类型选择 - 事件委托
        typeGrid.addEventListener('click', (e) => {
            const opt = e.target.closest('.type-option');
            if (!opt || !this.currentSlot) return;

            const type = opt.dataset.type;
            // 更新类型选中状态
            typeGrid.querySelectorAll('.type-option').forEach(o => {
                o.classList.toggle('selected', o.dataset.type === type);
            });
            // 更新配置
            this.currentConfig[this.currentSlot].type = type;
            this.currentConfig[this.currentSlot].style = 'standard';
            // 重新渲染样式选项
            this.renderStyleOptions(type, 'standard');
            // 重新渲染槽位
            this.renderSlot(this.currentSlot);
            this.saveConfig();
        });

        // 样式选择 - 事件委托
        styleGrid.addEventListener('click', (e) => {
            const opt = e.target.closest('.style-option');
            if (!opt || !this.currentSlot) return;

            const style = opt.dataset.style;
            // 更新选中状态
            styleGrid.querySelectorAll('.style-option').forEach(o => {
                o.classList.toggle('selected', o.dataset.style === style);
            });
            // 更新配置
            this.currentConfig[this.currentSlot].style = style;
            // 重新渲染槽位
            this.renderSlot(this.currentSlot);
            this.saveConfig();
        });
    },

    // 保存配置
    saveConfig() {
        localStorage.setItem('gamepadModules', JSON.stringify(this.currentConfig));
    },

    collapseCard() {
        const card = document.getElementById('gamepad-visual-card');
        card.classList.add('collapsed');
        this.isCollapsed = true;
        localStorage.setItem('gamepadVisualCollapsed', 'true');
    },

    expandCard() {
        const card = document.getElementById('gamepad-visual-card');
        card.classList.remove('collapsed');
        this.isCollapsed = false;
        localStorage.setItem('gamepadVisualCollapsed', 'false');
    },

    closeModal() {
        document.getElementById('module-modal').classList.remove('active');
        this.currentSlot = null;
    }
};

// ========================================
// 手柄图形更新
// ========================================

const GamepadVisual = {
    init() {
        // 动态元素，不需要缓存
    },

    // 更新摇杆位置（动态查找元素）
    updateStick(side, x, y) {
        const stick = document.getElementById(`visual-stick-${side}`);
        if (!stick) return;

        // 映射到像素偏移 (最大约8px)
        const offsetX = x * 8;
        const offsetY = y * 8;
        stick.style.transform = `translate(${offsetX}px, ${offsetY}px)`;

        // 活动状态
        const isActive = Math.abs(x) > 0.1 || Math.abs(y) > 0.1;
        stick.classList.toggle('active', isActive);
    },

    // 更新按键状态
    updateButton(buttonIndex, pressed) {
        // 动态查找元素
        const selectors = {
            0: '.btn-a',           // A
            1: '.btn-b',           // B
            2: '.btn-x',           // X
            3: '.btn-y',           // Y
            4: '.shoulder-lb',     // LB
            5: '.shoulder-rb',     // RB
            6: '.shoulder-lt',     // LT
            7: '.shoulder-rt',     // RT
            8: '#btn-back',        // Back
            9: '#btn-start',       // Start
            12: '[id^="dpad-up"]', // Up
            13: '[id^="dpad-down"]', // Down
            14: '[id^="dpad-left"]', // Left
            15: '[id^="dpad-right"]', // Right
            16: '#btn-home'        // Home
        };

        const selector = selectors[buttonIndex];
        if (selector) {
            const elements = document.querySelectorAll(selector);
            elements.forEach(el => el.classList.toggle('active', pressed));
        }
    }
};

// ========================================
// 应用初始化
// ========================================

document.addEventListener('DOMContentLoaded', () => {
    ThemeManager.init();
    ConfigManager.init();
    UIManager.init();
    GamepadManager.init();
    ModuleManager.init();
    GamepadVisual.init();

    console.log('GamePad Tuner 已启动');
});
