# 文档索引

当前推荐入口为 `platform/accelerated_main.c` 和 `make board`。Sapphire 已接公共 G2D／FRM 会话；电脑 SDL 仍只执行 CPU 绘图。现有主机验证与 RAM ELF 构建不能替代实板验收。

| 顺序 | 文档 | 内容 |
|---:|---|---|
| 1 | [项目README](../README.md) | 1／2／3模式、构建、串口键盘、试玩与当前边界 |
| 2 | [代码阅读指南](代码阅读指南.md) | 游戏、素材上传、双后端、平台和测试逐文件职责 |
| 3 | [公共绘图接口](公共绘图接口.md) | RGB565、CPU／FRAME目标、命令顺序、容量、所有权、比较和计时口径 |
| 4 | [板卡接入](板卡接入.md) | BSP、RAM布局、加载前提、发布合同、UART协议与故障诊断 |
| 5 | [验证记录](../PROGRESS.md) | 当轮实际结果、证据路径、未验证范围及历史记录 |
| 6 | [新手体验记录](../tests/experience_report.md) | 新手／标准配置、行为断言、固定输入参与时间 |
| 7 | [素材说明](素材说明.md) | 原始素材来源、转换规则与许可证边界 |
| 8 | [历史G2D场景](g2d_scene.md) | 保留的游戏专用旧接口，不是当前公共命令后端 |

当前调用链为 `Game → game_build_commands → game_hardware_commands → CPU context／公共 G2D job → FRM确认`。模式1的CPU结果也通过G2D全屏Copy发布；模式3依次发布同一冻结场景的CPU和G2D结果。两者都不写扫描中的硬件FRONT。

接口文档中的硬件事实以当前匹配RTL/BSP和发布合同为准；旧章节保留其当时结果，不覆盖最新状态。测试数量、软件解码像素和交叉构建不应写成板测或加速比。

许可证见[LICENSE](../LICENSE)，素材声明见[NOTICE](../NOTICE)。修改公共接口或板端入口时，同步维护本目录说明与 `PROGRESS.md`。
