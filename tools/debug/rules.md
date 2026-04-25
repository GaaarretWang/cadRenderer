# Debug Rules

本文件只约束调试、构建、运行、save-ref、compare 这条链路。通用规则见
[../../CLAUDE.md](../../CLAUDE.md)。

## 基本原则

1. 不要把时间限制写死在脚本内部。
2. 需要停止程序时，优先使用程序参数，例如 `--frames`。
3. 小步推进要兼顾速度。默认以一个可独立定位的问题域作为一个 commit，不要把步子切得过碎而拖慢主线。
4. 调试入口分两类：
   - 职责单一的固定 bat
   - 一个可直接修改内容的万能 bat `agent_debug.bat`
5. 默认在终端中执行脚本，不依赖双击窗口交互。
6. 我自己执行 build、run、compare、`git add`、`git commit`、删除文件、
   清理进程等命令时，一律先改 `agent_debug.bat` 的标记区块，再执行这个固定入口。
7. 固定职责小 bat 可以保留给人工手动使用，但我默认不用它们发起命令。
8. 统一调用格式固定为：在仓库根目录下执行
   `& .\tools\debug\agent_debug.bat`。
   以后即使需要权限，也仍然使用这一条命令，不再套别的调用包装。

## 修改后的最小验证

默认不要对每个 commit 做同等强度的重复测试。按下面规则执行：

- 不改变画面效果和外部行为的改动：
  - 自动执行最小验证
  - 通过后直接进入下一个 commit
  - 不等待用户逐步确认
- 改变渲染效果、切换主路径、删除旧链或出现不稳定迹象的改动：
  - 先执行自动化验证
  - 再给出明确的人工观察点
  - 等用户确认后再继续删旧链或进入下一阶段

最小验证默认执行：

```bat
build_and_run.bat --build-only
build_and_run.bat --scene 0 --frames 500
```

smoke test 成功判据：

- 编译通过
- 程序自然运行
- 日志中出现 `FPS:`

阶段边界、主路径切换、删旧链前，升级验证为：

```bat
build_and_run.bat --build-only
build_and_run.bat --scene 0 --frames 2000
```

smoke test 成功后，允许继续当前修改序列；非视觉改动可直接进入下一个 commit。

## Compare 节奏

以当前修改序列里我自己创建的提交为计数单位：

- 每满 3 次提交，必须全量跑一次 compare
- compare 入口优先使用 `tools/debug/compare_all.bat`

## Compare 失败后的处理

compare 不一致时，必须按下面流程处理：

1. 从上次全量 compare 成功后的第一个提交开始逐个测试
2. 不跳步猜测
3. 每轮都可以重新检查 build/run/compare 脚本和参数链路
4. 如果连续 3 轮仍不能恢复，则停止并交由用户判断

只有 compare 全通过后，才允许把结果合并到最新分支再重新验证。

## 调试脚本约束

- `build_only.bat` 只负责编译
- `run_frames.bat` 只负责按帧运行
- `save_refs_all.bat` 只负责全量保存参考图
- `compare_all.bat` 只负责全量 compare
- `kill_rendering.bat` 只负责清理 `Rendering.exe`
- `agent_debug.bat` 是唯一允许直接改脚本内容的万能 bat

## 万能 bat 规则

`agent_debug.bat` 必须保留固定 preamble，只允许直接修改标记区块内的命令：

- `REM AGENT_EDIT_START`
- `REM AGENT_EDIT_END`

后续如果需要通过固定脚本入口申请权限，默认使用 `agent_debug.bat`，
不要再为一次性命令临时造新的入口，也不要优先直接执行零散命令。

`agent_debug.bat` 允许承载的典型命令包括：

- `build_and_run.bat`
- `Rendering.exe --frames ...`
- `test_all_scenes_save.bat`
- `test_all_scenes_compare.bat`
- `git status` / `git add` / `git commit` / `git rm`
- `Remove-Item` / `del` / 清理中间文件
- `taskkill` / `Stop-Process`

默认工作流是：先改 `agent_debug.bat` 的标记区块，再执行这个固定入口。

统一执行命令：

```powershell
& .\tools\debug\agent_debug.bat
```

