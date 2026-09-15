# Window Pin · 快捷键置顶

轻量 Windows 托盘工具：按 **Win + Ctrl + T**，切换当前窗口的置顶状态。

使用 Windows API 编写的独立 C++ 程序，不依赖 PowerToys 或 .NET。

## 使用

从 [Releases](https://github.com/ScalarFX/window-pin/releases) 下载 `WindowPin.exe`，双击运行，无需安装。

- 选中一个窗口，按 **Win + Ctrl + T** 置顶，再按一次取消。
- 置顶后显示蓝色细边框，随窗口移动和缩放，并匹配系统圆角设置；最大化时使用直角。最小化时隐藏，取消置顶或关闭窗口时移除。边框不拦截鼠标操作。
- 右键托盘图标可取消本工具设置的全部置顶、切换开机启动或退出。
- 正常退出时会取消本工具设置的置顶。
- 开机启动针对当前用户，可在托盘菜单启用；启用后请保持 EXE 路径不变。删除前请关闭开机启动。

从 v1.0.0 更新：先退出旧版，再替换 EXE。在新版托盘菜单中启用“开机启动”，会创建启动文件夹快捷方式并移除旧注册表启动项。

源码项目根目录也提供 `一键启动.bat`，用于启动 `输出结果/WindowPin.exe`。

若快捷键被 PowerToys 等程序占用，会提示并退出。操作管理员权限窗口时，可能需要以管理员身份运行。强制结束进程不会执行退出清理。当前提供 Windows x64 构建。

## 构建

需要 Visual Studio 2022 C++ 构建工具和 Windows SDK。在 **Developer PowerShell for VS 2022** 中运行：

```powershell
.\build.ps1
```

输出：`输出结果/WindowPin.exe`。图标资源已包含，无需 Python；修改图标生成脚本时需要 Python 和 Pillow。

源码位于 `项目文件/`。边框绘制部分适配自 Microsoft PowerToys 的 `WindowBorder.cpp` 和 `FrameDrawer.cpp`，使用 Direct2D 和窗口位置事件更新。

PowerToys 原始源码：https://github.com/microsoft/PowerToys/tree/main/src/modules/alwaysontop/AlwaysOnTop

相关代码采用 MIT 许可证，版权声明见 [PowerToys-LICENSE.txt](文档/PowerToys-LICENSE.txt)。

## 许可证

MIT，详见 [LICENSE](LICENSE)。
