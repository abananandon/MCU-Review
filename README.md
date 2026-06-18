# MCU-Review

基于 STM32F103xE (Cortex-M3) 的嵌入式复习项目，覆盖 STM32 基本外设、数字滤波算法、嵌入式常用数据结构和三相无刷直流电机 FOC 控制。


## 硬件平台

| 项目 | 参数 |
|------|------|
| MCU | STM32F103xE (Cortex-M3, 72 MHz) |
| Flash | 512 KB |
| RAM | 64 KB |
| 调试器 | ST-Link / J-Link (OpenOCD) |

## 学习路线

```
App/Peripherals/         STM32 基本外设练习
    ├── GPIO             输入/输出控制 LED、按键
    ├── EXTI             外部中断
    ├── UART/USART       串口通信（轮询/中断/DMA）
    ├── SPI              外设通信（Flash、传感器）
    ├── I2C              外设通信（EEPROM、传感器）
    ├── TIM/PWM           定时器、PWM 输出、输入捕获
    ├── ADC/DAC           模拟信号采集与输出（单次/连续/DMA）
    ├── DMA              存储器到外设、存储器到存储器
    ├── RTC              实时时钟与闹钟
    └── IWDG/WWDG        独立看门狗 / 窗口看门狗

Components/
    ├── Filter/           数字滤波算法
    │   ├── 一阶低通 / 高通
    │   ├── 滑动平均
    │   ├── 中值滤波
    │   ├── 卡尔曼滤波
    │   └── 互补滤波
    ├── Math/             数学工具库
    │   ├── PID 控制器
    │   ├── FFT / DCT
    │   └── 三角函数查表
    └── DataStructures/   嵌入式常用数据结构
        ├── 环形缓冲区 (Ring Buffer)
        ├── 链表 / 队列
        ├── 状态机框架
        └── 位图 / 位带操作

App/MotorControl/        三相 PMSM/BLDC FOC 算法
    ├── Clark / Park 变换
    ├── SVPWM 空间矢量调制
    ├── 电流环 PI
    ├── 速度环 / 位置环
    ├── 角度估算（Hall / 编码器 / 无感）
    └── MTPA / 弱磁控制
```

## 工程结构

```
MCU-Review/
├── App/                          # 应用层代码
│   ├── main.c                    # 入口
│   ├── Peripherals/              # 外设练习
│   ├── MotorControl/             # FOC 算法
│   └── Algorithms/               # 通用算法
├── Components/                   # 硬件无关软件库
│   ├── Filter/                   # 滤波算法
│   ├── Math/                     # 数学工具
│   └── DataStructures/           # 数据结构
├── Hardware/                     # 板级驱动
│   ├── BSP/                      # 板级支持包 (LED, 按键等)
│   └── SYSTEM/                   # 系统级工具 (delay, printf 重定向)
├── Config/                       # 工程配置
│   ├── stm32f1xx_hal_conf.h      # HAL 模块裁剪
│   ├── STM32F103xE_FLASH.ld      # 链接脚本 (512K Flash / 64K RAM)
│   └── CMakeLists.txt            # 全局宏定义 (INTERFACE 库)
├── Drivers/                      # 驱动层
│   ├── CMSIS/                    # CMSIS-Core + Device
│   └── STM32F1xx_HAL_Driver/    # STM32 HAL 外设库 (45 个模块)
├── Startup/                      # 启动文件
│   ├── gcc/startup_stm32f103xe.s # GCC 汇编
│   └── arm/startup_stm32f103xe.s # ARM/Keil 汇编
├── Middlewares/                  # 中间件 (FreeRTOS 预留)
├── Tools/                        # 调试工具
│   └── Debug/STM32F103xx.svd     # 外设寄存器描述文件
├── Build/                        # CMake 构建目录 (gitignored)
├── Outputs/                      # 编译产物 .elf .hex .bin (gitignored)
├── arm-none-eabi-gcc.cmake       # CMake 交叉编译工具链文件
└── CMakeLists.txt                # 顶层 CMake 构建脚本
```

## 环境搭建

### 必需工具

| 工具 | 用途 |
|------|------|
| [ARM GNU Toolchain](https://developer.arm.com/downloads/-/gnu-rm) | 交叉编译器 (arm-none-eabi-gcc) |
| [CMake](https://cmake.org/download/) (>= 3.15) | 构建系统生成 |
| [Ninja](https://ninja-build.org/) | 构建后端 |
| [OpenOCD](http://openocd.org/) | 片上调试与烧录 |
| [VSCode](https://code.visualstudio.com/) | 编辑器 + 调试器 |
| VSCode 扩展: [Cortex-Debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug) | 调试集成 |
| VSCode 扩展: [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) | CMake 项目支持 |

### 构建

```bash
# Debug
cmake -B Build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-gcc.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build Build

# Release
cmake -B Build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-gcc.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build Build
```

产物生成在 `Outputs/`：
- `MCU-REVIEW.elf` — 调试文件
- `MCU-REVIEW.hex` — 烧录文件 (Intel HEX)
- `MCU-REVIEW.bin` — 烧录文件 (纯二进制)
- `MCU-REVIEW.map` — 链接内存映射

### VSCode 一键调试

项目已预置 `.vscode/` 配置，直接 F5 启动调试：

1. 连接 ST-Link 或 J-Link 到开发板
2. 按 **F5**
3. CMake Tools 自动构建 → OpenOCD 连接 → 烧录 → 停在 `main()`
4. 左侧"外设"面板展开寄存器视图（由 SVD 文件驱动）
5. 设置断点、查看变量、单步调试

## 外设练习大纲

| 序号 | 外设 | 实验内容 |
|------|------|---------|
| 1 | GPIO | LED 流水灯、按键扫描（消抖）、位带操作 |
| 2 | EXTI | 外部中断响应按键、中断优先级实验 |
| 3 | UART | 轮询收发、中断收发、printf 重定向，环回测试 |
| 4 | UART+DMA | DMA 不定长接收、空闲中断 + DMA |
| 5 | SPI | 读写 W25Qxx Flash、SPI 屏驱 |
| 6 | I2C | 读写 AT24Cxx EEPROM、I2C 传感器 (MPU6050) |
| 7 | TIM | 定时器中断、PWM 输出、输入捕获测频率/占空比 |
| 8 | ADC | 单通道扫描、多通道 DMA、内部温度传感器 |
| 9 | DAC | 输出正弦波、三角波 (TIM 触发 DMA) |
| 10 | DMA | 内存到外设、内存到内存传输实验 |
| 11 | RTC | 日历、闹钟中断、秒中断 |
| 12 | Watchdog | IWDG 饥饿复位实验、WWDG 窗口实验 |

## 依赖关系

```
app (ELF)
├── startup (vector table, Reset_Handler)
├── hardware (BSP, SYSTEM)
│   └── drivers (HAL + CMSIS)
│       └── config (STM32F103xE, USE_HAL_DRIVER)
└── components (Filter, Math, DataStructures)
    └── config
```

## 许可

MIT License
