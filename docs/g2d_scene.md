# G2D 场景命令

`src/g2d_scene.c` 只负责把 `Game` 状态转换为一帧场景命令。它不读写 MMIO，不选择物理帧缓冲地址，不等待队列，也不执行缓存维护。

每帧命令顺序固定为：`BEGIN_FRAME`（3）、`FILL`（1）、`COPY`（2）、`END_FRAME`（4）。背景和角色使用 COPY，清屏、侧栏和血条使用 FILL。

`GameG2dCommand.source` 引用离线生成的 `GameSprite`，`color_key` 使用 `GAME_COLOR_KEY`，`alpha` 默认 `255`。运行时提交器将这些属性转换成硬件描述字段。

`GameG2dSink.submit` 是游戏层和运行时之间的边界。板端实现负责编码、队列背压、完成事件和错误处理；游戏层不依赖具体寄存器布局。

`tests/test_g2d_scene.c` 检查 frame id、色键/alpha 属性、首尾 opcode、背景 COPY 和动态对象命令。它不声称已经完成 MMIO、DMA、显示换帧或板测。
