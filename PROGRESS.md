# 实现与验证记录

2026-10-24，本次用户明确要求生成板端游戏，按该授权在独立目录实现游戏，不改原版UET、原software或硬件RTL。

## 已实现

- 芙兰单Boss手动玩法、三命/受伤保护、胜负/暂停/重开、60秒自动演示。
- Q8定点、固定512敌弹和64玩家弹、整数方向表；目标端无堆、无浮点、无SDL和图片字体音频解码。
- 三档强度、活动/峰值/丢弃计数；高档每100ms生成22颗，普通档每200ms。
- RGB565素材和CPU参考渲染，精灵裁战斗区、色键透明、左右角色帧、点阵HUD。
- 独立SDL2主机预览、无窗口PPM导出。
- 实际newSoC BSP UART/CLINT入口，CRC快照输入、断线释放、目标60Hz逻辑/30Hz绘图、有界追帧。
- 默认显示不可用时明确停止；RAM_ONLY_DIAGNOSTIC输出RAM帧哈希，不冒充显示或加速。
- 2MiB独占DDR应用链接布局和64KiB栈；未链接验证，未生成板端ELF或固化文件。

## 实际通过的检查

1. Linux GCC严格C11 `-O2 -Wall -Wextra -Werror -pedantic`：core、CPU renderer、host预览和UART协议构建通过。
2. `make test`：三档60秒确定性、50,000tick随机运行、边界/上限/命令边沿/暂停重开/碰撞与胜负/计时回绕，以及渲染stride保护区和UART错误恢复全部通过。
3. ASan+UBSan：core、render和UART三个测试程序全部通过；日志 `build/validation.json`。
4. SDL dummy软件显示运行2秒仍存活，随后测试主动终止。没有据此声称Windows桌面或WSLg窗口故障已经修好。
5. xPack GCC14.2 RV32 `rv32im_zicsr/ilp32/gnu11`，配 `bitblt_soc-main/embedded_sw/newSoC`真实BSP及standalone driver：默认入口和RAM诊断入口源码检查通过。保留厂商头文件3类/处警告：semihosting注释嵌套、print.h两个未使用变量。不是汇编、链接或板测通过。
6. 离线资源共8个像素数组，尺寸/非空/色键/SHA256及重复生成一致性通过，合计244284字节。

| 60秒自动演示档 | peak_enemy | dropped | 完整状态hash |
|---|---:|---:|---|
|128|128|2925|92a8ac9d|
|256|235|0|c243e458|
|512|458|0|5bdafcac|

Hash用于判断相同输入的逻辑重放一致，不是性能分数。低档的丢弹是确定性的容量限制，不是越界或随机缺弹。不同档位发射强度可能不同，比较CPU/FPGA时必须选同一档、同一帧输入。

## 当前不能声称完成的部分

- 当前RTL的G2D/显示控制器仍为空壳，显示FIFO未消费；真实屏幕画面未实现或验证。
- 当前renderer是CPU绘图；没有可用的FPGA加速后端，也没有硬件加速性能数字。
- 2MiB应用预留区需集成层确认独占；DDR在startup清BSS前就必须可用。默认124KiB链接区及Flash复制长度不能直接使用。
- 未验证显示端400×300 RGB565支持或2倍缩放，未测量DMA/cache一致性或板上帧率。
- 未接串口实物、未加载程序、未编译或生成板端ELF、未烧写FPGA或Flash。

## 硬件完成后的最短路径

先以RAM诊断确认程序、DDR和输入；完成真实显示链及视频适配器后打开出屏模式，验证双缓冲所有权和缓存；最后再依据真实G2D ABI替换CPU绘图并做相同输入下的对比。不得在缺少ABI时给占位窗口写入猜测命令。

## 工程化整理（系统日期 2026-10-18）

本轮只做打包与可移植性整理，**未修改任何游戏逻辑、渲染代码或素材数据**。

> 注：上文标注的「2026-10-24」与当前系统日期不一致；本次不修改历史记录，仅追加本轮条目。

### 中间产物核对

整理前全目录共 25 个文件，**不存在** `build/`、`*.o`、`*.elf`、`*.d` 等中间产物，也无可清理的空目录。因此本轮没有删除任何既有文件。

新增 `make clean` 目标，并扩充 `.gitignore` 覆盖 `build/`、目标文件、`*.ppm`、Python 缓存与编辑器目录。

### 关于构建平台（重要）

本项目在 **Linux** 上开发并验证。`Makefile` 使用 POSIX 命令（`mkdir -p`、`rm -rf`、`pkg-config`）。
Windows 上必须在 WSL 或 MSYS2 shell 中运行；Windows 原生 `make` 的配方经 `cmd.exe` 执行，
`mkdir -p` 会多建一个名为 `-p` 的目录，`rm -rf` 也会因缺少 `rm` 而失败。

整理过程中曾把 `Makefile` 改成按 `$(OS)` 分支、在 Windows 上调用 `cmd /c`。
该改动偏离了项目的实际验证环境，**已全部撤回**，恢复为纯 POSIX 版本。
游戏逻辑、渲染代码、素材与测试均未受影响。

### 新增文件

| 文件 | 作用 |
|---|---|
| `LICENSE` | 源代码 MIT 许可证 |
| `NOTICE` | 第三方素材版权与授权状态声明 |
| `.editorconfig` | 统一缩进与换行 |
| `.gitattributes` | 仓库内统一 LF |
| `.github/workflows/ci.yml` | Ubuntu 上执行 `make test`、`make headless` 与 Python 语法检查 |
| `docs/README.md` | 文档索引与阅读顺序 |

`README.md` 重构为标准仓库结构（状态、快速开始、结构树、文档、许可证），原有事实性结论全部保留，未新增未经测量的性能数字。

### 本轮实际通过的检查

1. 按 `test` 目标中的三条命令逐条执行（`cc` GCC 15.2.0，`-O2 -std=c11 -Wall -Wextra -Werror -pedantic`），三组单元测试全部通过，输出与整理前一致：`sizeof(Game)=11592`，三档哈希 `92a8ac9d` / `c243e458` / `5bdafcac`。
2. 三个测试程序的编译与运行退出码均为 0。

### 本轮未能在本机验证的项

- **`make` 目标本身未能在本机执行**。当前 Windows 环境没有可用的 POSIX shell：
  MSYS2 的 `make.exe`、`rm.exe`、`mkdir.exe` 在受限沙箱中无法启动
  （`couldn't create signal pipe`），WSL 亦不可用。
  因此改为逐条执行 `Makefile` 中的等价命令完成验证。
  `Makefile` 已恢复为原项目的 POSIX 形式，其在 Linux 上的可运行性由用户验证。

### 本轮未验证

- `.github/workflows/ci.yml` **尚未在 GitHub 上实际运行**，只做了本地等价命令验证。
- `make board-check` 本轮未执行（需要本机 RISC-V 工具链与 BSP 路径），其上次结果见上文第 5 条。
- 未生成 ELF、未烧写、未板测，状态与上文相同。

