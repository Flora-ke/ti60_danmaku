# Ti60 芙兰弹幕演示程序

纯 C 单 Boss 游戏：固定 60Hz 逻辑、Q8 定点、512 敌弹／64 玩家弹对象池。游戏生成有序公共 `gfx_command`，电脑版由公共 CPU 后端执行；Sapphire 版已接入同级 `bitblt-runtime` 的真实 G2D／FRM 显示会话，支持键盘 **1／2／3** 切换执行路径。库源码直接引用，没有复制成游戏私有分支。

游戏逻辑仍为 400×300，板端在 640×480 RGB565 画布的 `(120,90)` 居中显示，不缩放、不减少子弹。当前已完成主机验证和 RAM ELF 交叉构建，尚未在实板运行；板上位流、DDR 初始化与源码匹配仍须现场确认。没有自动连接串口、加载程序或烧写。

## 板端三种模式

| 按键 | 实际路径 |
|---|---|
| **1 CPU**（默认） | 同一完整场景由 CPU 画入自有 640×480 RAM，再经 G2D 全屏 Copy 搬到硬件分配的显示帧。它仍使用 G2D 进行显示搬运。 |
| **2 G2D** | 完整 Fill／Copy／CopyKey 命令由硬件执行，经 END、FRM 呈现和扫描健康确认。不会回退到 CPU 画图。 |
| **3 比较** | 冻结一份场景命令，先发布 CPU 结果，再发布原生 G2D 结果；两帧内容相同，依次呈现，串口分别统计。 |

模式切换在当前任务完整结束后生效，不重置游戏或硬件会话。比较模式不是并排显示；没有输出未经实板测量的加速比。画面应保持一致，可从串口 `mode/req/shown/pair/gpu_frame` 确认切换。

## 构建 RAM 固件

在已有裸机工具链的 WSL／Linux 中执行：

```sh
cd /mnt/d/Desktop/2026FPGA/ti60_danmaku
make board
# 同等入口；路径不在默认位置时可显式覆盖：
python3 tools/build_sapphire.py \
 --gfx-root /mnt/d/Desktop/2026FPGA/bitblt-runtime \
 --soc-root /mnt/d/Desktop/2026FPGA/bitblt_soc-ysl-dev/embedded_sw/newSoC \
 --riscv-bin /mnt/d/Desktop/2026FPGA/bitblt-runtime/build/toolchains/xpack-riscv-none-elf-gcc-14.2.0-3/bin/riscv-none-elf-
```

默认构建匹配 `bitblt_soc-ysl-dev/embedded_sw/newSoC` 的 BSP、启动文件与 C 库，产物在 `build/sapphire/`：`ti60_danmaku.elf`、map、bin、hex、asm 和 `build_manifest.json`。这些是 RAM 加载产物，不是 Flash／XIP 固件。构建脚本核对发布合同涉及的 12 个硬件／BSP文件哈希、ELF 架构、LOAD 范围、自有 DMA 数组、栈及发布指令；构建期间源码变化或硬件指纹不匹配即拒绝。

程序独占 `[0x1000,0x201000)` 的 2MiB 应用 DDR，其中栈为 64KiB；DMA 素材 252672B、自有 CPU 画布 614400B、CPU 工作区 90112B、G2D 工作区预留 128512B（精确需求 126504B）。一次实际链接占用 1565248B，约 74.64%；最终结果以本次 `build/sapphire/build_manifest.json` 的 `memory_bytes_from_origin` 为准，包含代码、数据、BSS、对齐与栈。硬件 A/B 帧由硬件拥有，未在应用内分配两份显示帧，也不由 CPU 直接写入。

加载前须确认匹配普通 CPU 位流、DDR 已初始化、整个应用区独占，并处于 CPU／G2D／显示协调冷态。具体流程、所有权和故障限制见[板卡接入](docs/板卡接入.md)。构建成功不代表板测通过。

## 键盘控制板卡

Windows PowerShell 中运行以下电脑端工具；COM6 仅为示例，请换成真实端口，先关闭占用同一串口的终端。

```powershell
cd D:\Desktop\2026FPGA\ti60_danmaku
py tools/uart_bridge.py --self-test
# 如电脑环境尚无 pyserial，先在该 Python 环境安装：
py -m pip install pyserial
py tools/uart_bridge.py --port COM6
```

Windows 默认采样启动时前台终端的真实按下／松开，支持同时按键；失焦立即发送全松开。方向键或 WASD 移动，按住 Z／空格射击，Enter／R 开始或重开，P 暂停，M 演示，K 弹幕强度，N 新手／标准，1／2／3 切换绘图模式，Q／Esc／Ctrl+C 退出控制端并尽力发送释放报文。显示端不会因控制端退出自动停显。

Linux 可用 `python3 tools/uart_bridge.py --port /dev/ttyUSB0`；终端输入依赖 180ms 重复键租约，无法提供真实松键，空格切换持续射击。Windows 也可加 `--terminal` 使用该方式。桥只发送统一按键快照和显示遥测，不下载或烧写固件。协议为 115200/8N1、8 字节 CRC 帧、13 位允许位图；完整定义见[板卡接入](docs/板卡接入.md)。

## 电脑试玩与难度

```sh
cd /mnt/d/Desktop/2026FPGA/ti60_danmaku
make preview
./build/danmaku-preview
```

需要 SDL2 开发库。电脑版只执行 CPU 绘图；按 2／3 会明确提示需要 Sapphire FPGA，`--backend 2/3` 返回错误，不模拟硬件。默认新手标题界面，Enter 开始，方向键移动，N 在标题／胜负界面选择配置；P 暂停，Tab 切换弹幕强度，D 自动演示，Esc 退出。

| 手动配置 | 初始生命 | 受伤保护 | 射击 |
|---|---:|---:|---|
| 新手（默认） | 10 | 180 tick／3 秒 | 自动 |
| 标准 | 3 | 120 tick／2 秒 | 按键 |

参数集中在 `src/game.c` 的 `GamePlayConfig`。难度与低／中／高弹幕强度独立：低／中上限128／256、每12tick发射，高上限512、每6tick发射，均为20环形＋2扫射。受伤不清弹幕，判定仍为小判定点；保护期角色始终可见并有青／白闪框和 HUD 提示。暂停冻结逻辑、保护及冷却；重开保留所选难度、清除上一局对象。DEMO独立保持60秒确定性无敌演示。

此前固定输入测试中新手参与约30～42秒、标准约6.5～7.8秒，十二组最终均失败；不是新人真人试玩或通关保证。体验参数可集中调整，不以减少弹幕改善性能。

## 验证与阅读

```sh
make test headless preview
make test-hardware
./build/danmaku-headless --demo --limit 512 --frames 3600 --ppm build/frame.ppm
python3 tools/validate.py
```

游戏命令容量864条，满512敌弹＋64玩家弹及完整HUD／覆盖界面最重组合814条；不漏画已存在子弹。新硬件集成主机检查覆盖39场景全640×480画布、12帧软件解码硬件命令像素对比、同一会话跨1／2／3切换、三次高档完整演示5400个任务和2819052条硬件协议命令退休，零分配。包含第3600tick结束界面的演示峰值601条；此前552是不同抽样边界，不能当作当前完整演示上限。这些是主机像素／协议证据，不是板上像素或性能结果。

- [实际验证记录](PROGRESS.md)
- [公共命令、生命周期与测量口径](docs/公共绘图接口.md)
- [板卡构建、输入、所有权及故障处理](docs/板卡接入.md)
- [逐文件阅读指南](docs/代码阅读指南.md)
- [新手体验记录](tests/experience_report.md)、[素材说明](docs/素材说明.md)

保留的 `g2d_scene.[ch]`、旧 `BOARD_G2D_ADAPTER_EXTERNAL` 及 `platform/sapphire_main.c` 属于历史路径；当前板端推荐 `platform/accelerated_main.c` 和 `make board`。旧入口可继续用于显式 RAM 诊断，不应当作本轮真实加速接口。

## 许可证与素材


- **源代码**：[MIT](LICENSE) © 2026 WangKechun
- **素材**：`assets/` 下的像素数据来自 [UET_Danmaku](https://github.com/cusnaruto/UET_Danmaku)，
  **不属于 MIT 许可范围**。该来源仓库未附带许可证文件，其授权状态不由本项目确定。
  详见 [NOTICE](NOTICE)。

如需干净的授权状态，可删除 `assets/assets.c` 与 `assets/manifest.json`，用 `tools/convert_assets.py` 从你持有授权的图片重新生成。
