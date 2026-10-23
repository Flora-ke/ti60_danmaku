# Ti60 芙兰弹幕演示程序

纯 C 实现的单 Boss 弹幕游戏，面向 Ti60 / Sapphire SoC（100 MHz、RV32IM、256 MiB DDR、400×300 RGB565）。

固定对象池、Q8 定点运算、离线内嵌图片，**无堆分配、无浮点、无运行时解码、无文件系统**。

> **当前状态：可以在电脑上完整运行；已通过真实 BSP 的 RV32 源码检查；当前硬件位流尚不能出屏。**
>
> 详见 [当前状态](#当前状态) 与 [PROGRESS.md](PROGRESS.md)。

---

## 目录

- [这是什么](#这是什么)
- [当前状态](#当前状态)
- [快速开始](#快速开始)
- [操作说明](#操作说明)
- [弹幕档位与内存预算](#弹幕档位与内存预算)
- [板端集成](#板端集成)
- [项目结构](#项目结构)
- [文档](#文档)
- [许可证与素材](#许可证与素材)

---

## 这是什么

一个从零重写的板端弹幕游戏，不是对原 C++ 游戏的移植。

它借用 [UET_Danmaku](https://github.com/cusnaruto/UET_Danmaku) 的角色素材与「放射＋扫射」玩法设计，把原本基于 C++ ECS + SDL2 的实现改写为固定容量 C 程序：整数方向表替代浮点角度，固定数组替代动态容器，离线 RGB565 素材替代运行时贴图加载。

因此**不宣称与原游戏逐帧一致**：多 Boss 流程、音频、完整菜单均未移植。

设计目标是为 RISC-V + FPGA 图形平台提供一个可控、可测、内存可预算的绘图负载。

## 当前状态

### 已经能做的

- 电脑端完整可玩：手动模式、自动演示、暂停、重开、胜负判定、三档弹幕强度
- 严格 C11 构建通过（`-O2 -Wall -Wextra -Werror -pedantic`）
- 三组单元测试通过：游戏逻辑确定性、渲染越界保护、UART 协议
- 使用真实 newSoC BSP 头文件做 RV32 `-fsyntax-only` 源码检查通过
- 离线素材生成可重复（SHA-256 校验一致）

### 还不能做的

**当前位流不能出屏。** 已核对的 `bitblt_soc-main/src/g2d/g2d_controller.v` 与 `displayopt_controller.v` 中，启动信号仍固定为零，顶层显示 FIFO 没有消费者。软件无法补出缺失的视频链路。

因此默认板端入口遇到未接通的显示适配器时会**记录错误并停止**，不会假装已显示。显式的 `RAM_ONLY_DIAGNOSTIC` 模式可做游戏逻辑与 RAM 绘图的板端集成，但仍需正确的 DDR 启动/加载流程。

**尚未进行任何板测**：未接串口实物、未生成板端 ELF、未烧写 FPGA 或 Flash、未测量板上帧率。

### 关于「FPGA 加速」

当前 renderer 只做 **CPU 参考绘图**。代码中没有任何 G2D 寄存器地址、命令或缓存指令的猜测实现。

把 CPU 绘图换成硬件绘制，必须依据实际验收后的 G2D ABI 另行接入，不能把 CPU 运行结果冒充 FPGA 加速。

## 快速开始

### 环境要求

| 用途 | 依赖 |
|---|---|
| 运行环境 | **Linux**（已验证）；Windows 需要 WSL 或 MSYS2 shell |
| 构建与测试 | C11 编译器（GCC/Clang）、GNU Make |
| 电脑试玩 | 上述 + SDL2 开发库（仅窗口与按键，图形全部由本项目绘制） |
| 素材重新生成 | Python 3 + Pillow（仅电脑端，板卡不需要） |
| 串口操作桥 | Python 3 + 可选 `pyserial` |

### 构建与测试

```sh
make test        # 编译并运行三组单元测试
make headless    # 构建无窗口版
make clean       # 删除 build/ 与全部中间产物
make help        # 列出所有目标
```

### 电脑试玩

需要 SDL2 开发库：

```sh
make preview
./build/danmaku-preview
```

> 本项目在 **Linux** 上开发与验证。`Makefile` 使用 POSIX 命令（`mkdir -p`、`rm -rf`、`pkg-config`），
> Windows 请在 WSL 或 MSYS2 shell 中运行，不要从 PowerShell / cmd 直接调用 `make`。

### 无窗口运行并导出画面

```sh
make headless
./build/danmaku-headless --frames 1200 --limit 512 --ppm build/frame.ppm
```

### 串口桥自测

```sh
python tools/uart_bridge.py --self-test
```

## 操作说明

| 按键 | 作用 |
|---|---|
| `Enter` / `R` | 进入手动模式 / 重新开始 |
| 方向键 | 移动 |
| `Z` / 空格 | 射击 |
| `P` | 暂停 |
| `D` | 自动演示 |
| `Tab` | 切换弹幕强度 |
| `Esc` | 退出 |

默认播放一轮 60 秒自动演示。手动模式 3 命，受伤后 2 秒无敌；击败 Boss 则胜利。

自动演示中玩家无敌、自动移动与射击，Boss 不会提前死亡，时间到显示 `FINISHED`。

## 弹幕档位与内存预算

| 档位 | 发射周期（60 Hz 逻辑） | 敌弹上限 | 60 秒演示实测峰值 | 丢弃新敌弹 |
|---|---|---:|---:|---:|
| 低 | 12 tick / 200 ms | 128 | 128 | 2925 |
| 中（默认） | 12 tick / 200 ms | 256 | 235 | 0 |
| 高 | 6 tick / 100 ms | 512 | 458 | 0 |

每轮 20 颗星形放射 ＋ 2 颗扫射。上限不是同屏数量的保证。

低档**有意展示容量限制**：达到上限后丢弃新弹并计数，绝不无限分配。玩家弹另有 64 个槽位。

| 固定数据 | 字节数 |
|---|---:|
| RGB565 素材像素 | 244 284 |
| 双帧缓冲 | 480 000 |
| 游戏对象（含全部弹池） | 11 592 |
| RV32 精灵元数据 | 64 |
| **小计** | **735 940**（约 718.7 KiB） |

小计不含代码、字符串、查表、串口状态和栈。最终总量仍须链接 map 核实。

项目提供独占 2 MiB DDR 的应用布局与 64 KiB 栈；默认 124 KiB 应用区**不能沿用**。没有给 DMA 或显示发明固定帧地址。

## 板端集成

入口为 `platform/sapphire_main.c`，使用实际 newSoC BSP 的 CLINT 与 UART，目标 60 Hz 逻辑、30 Hz 绘图。帧缓冲静态分配且 64 字节对齐。

要点：

- 编译公共源 `src/game.c` `src/render.c` `assets/assets.c` `platform/board_port.c` `platform/sapphire_main.c`，**排除 host 入口**。
- 复用同一硬件导出的 `embedded_sw/newSoC` BSP、C 库与 `software/standalone/common/start.S`，RV32IM/Zicsr 软浮点 ABI。BSP 使用 GNU `asm`，需要 `gnu11`。
- `platform/game_ddr.ld` 提供独占 `[0x1000, 0x201000)` 的 2 MiB RAM 加载布局与启动符号；**该脚本未链接验证**。
- 当前位流只能先选择 `RAM_ONLY_DIAGNOSTIC`。
- 出屏需要显示 RTL 完成并提供 `BOARD_VIDEO_ADAPTER_EXTERNAL` 实现；当前默认明确返回 `BOARD_UNAVAILABLE`。

源码级检查（需要本机的 RISC-V 工具链与 BSP 路径）：

```sh
make board-check \
  RISCV_CC=/path/to/riscv-none-elf-gcc \
  SOC_ROOT=/path/to/bitblt_soc-main/embedded_sw/newSoC
```

`board-check` **仅做源码语法检查**，不生成 ELF、不下载、不烧写。

完整接口契约、UART 报文格式与限制见 [板卡接入](docs/板卡接入.md)。

## 项目结构

```text
ti60_danmaku/
├── include/            公共头文件
│   ├── game.h          坐标、按键、状态与对象池定义
│   ├── render.h        CPU 绘图接口契约
│   ├── assets.h        精灵元数据（宽高、像素指针、透明色）
│   └── board_port.h    UART 协议与视频适配器契约
├── src/
│   ├── game.c          固定容量游戏逻辑，与平台无关
│   └── render.c        RGB565 CPU 参考渲染
├── assets/
│   ├── assets.c        离线生成的 const 像素数组（第三方素材，见 NOTICE）
│   └── manifest.json   尺寸、字节数、来源哈希、色键统计
├── platform/
│   ├── host_main.c     SDL2 窗口预览 / 无窗口 PPM 导出
│   ├── sapphire_main.c 真实 BSP 入口：UART、CLINT、双缓冲
│   ├── board_port.c    UART 帧解析、CRC、断线释放（可脱离 BSP 测试）
│   └── game_ddr.ld     2 MiB DDR 应用布局（未链接验证）
├── tests/
│   ├── test_game.c     逻辑确定性、上限、碰撞、计时回绕
│   ├── test_render.c   越界、负坐标、非紧凑 stride
│   └── test_board_uart.c CRC、坏帧、重同步、超时释放
├── tools/
│   ├── convert_assets.py 离线素材转换（裁切、缩放、RGB565）
│   ├── uart_bridge.py     电脑端串口操作桥
│   └── validate.py        本机综合校验脚本
├── docs/               设计文档
├── Makefile
├── PROGRESS.md         实现与验证记录（含实测数据）
├── NOTICE              第三方素材声明
└── LICENSE             MIT
```

## 文档

| 文档 | 内容 |
|---|---|
| [文档索引](docs/README.md) | 全部文档的导读顺序 |
| [代码阅读指南](docs/代码阅读指南.md) | 每个文件负责什么、调用链怎么走 |
| [板卡接入](docs/板卡接入.md) | BSP 事实、DDR/栈集成、视频适配器契约、UART 协议 |
| [素材说明](docs/素材说明.md) | 素材来源、裁切参数、转换规则、重复生成 |
| [PROGRESS.md](PROGRESS.md) | 实际通过的检查、实测哈希、未完成范围 |

## 许可证与素材

- **源代码**：[MIT](LICENSE) © 2026 WangKechun
- **素材**：`assets/` 下的像素数据来自 [UET_Danmaku](https://github.com/cusnaruto/UET_Danmaku)，
  **不属于 MIT 许可范围**。该来源仓库未附带许可证文件，其授权状态不由本项目确定。
  详见 [NOTICE](NOTICE)。

如需干净的授权状态，可删除 `assets/assets.c` 与 `assets/manifest.json`，用 `tools/convert_assets.py` 从你持有授权的图片重新生成。

## 致谢

- 角色素材与玩法灵感：[UET_Danmaku](https://github.com/cusnaruto/UET_Danmaku)
