# Window Pin · 快捷键置顶

轻量 Windows 托盘工具：按 **Win + Ctrl + T**，切换当前窗口的置顶状态。

使用 Windows API 编写的独立 C++ 程序，不依赖 PowerToys 或 .NET。

## 使用

从 [Releases](https://github.com/ScalarFX/window-pin/releases) 下载 `WindowPin.exe`，双击运行，无需安装。

- 选中一个窗口，按 **Win + Ctrl + T** 置顶，再按一次取消。
- 右键托盘图标可取消本工具设置的全部置顶、切换开机启动或退出。
- 正常退出时会取消本工具设置的置顶。
- 开机启动针对当前用户，可在托盘菜单启用；启用后请保持 EXE 路径不变。删除前请关闭开机启动。

若快捷键被 PowerToys 等程序占用，会提示并退出。操作管理员权限窗口时，可能需要以管理员身份运行。强制结束进程不会执行退出清理。当前提供 Windows x64 构建。

## 构建

需要 Visual Studio 2022 C++ 构建工具和 Windows SDK。在 **Developer PowerShell for VS 2022** 中运行：

```powershell
.\build.ps1
```

输出：`输出结果/WindowPin.exe`。图标资源已包含，无需 Python；修改图标生成脚本时需要 Python 和 Pillow。

源码位于 `项目文件/`。本项目不包含 PowerToys 源码。

## 许可证

MIT，详见 [LICENSE](LICENSE)。
