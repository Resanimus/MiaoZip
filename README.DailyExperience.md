# NanaZip 日常体验增强版 0.1

本地预览版本，基于 NanaZip 7.0.1843.0 / commit `df5db3c4a327c980e9f66dc28fbabd6782cda5e0`。

## 使用

部署目录：`F:\NanaZip\custom\app`。

请打开 `NanaZip.Modern.FileManager.exe`，或桌面的“NanaZip 增强版（预览）”快捷方式。
`NanaZip.Universal.Windows.exe` 是图形命令处理程序，直接双击仍可能提示“Specify command”，它不是文件管理器入口。

这一版改动：

- 按要求移除主界面右上角的赞助按钮及其工具栏后台查询。
- 主工具栏显示“添加 / 解压 / 智能解压 / 测试 / 菜单”的图标和文字。复制、移动、删除、属性及设置可从更多操作或菜单进入，原快捷键继续可用。
- “智能解压”直接调用 NanaZip 原有智能解压引擎：单文件直接解出；已有单一总文件夹时保留；多个散文件自动归入以压缩包命名的文件夹。开始前仍显示原生解压对话框。
- 智能解压默认使用“自动重命名”处理新文件，保留已有文件。对话框允许调整此行为。遇到已有同名目录时可能合并目录并重命名冲突文件；这一版不自动另建整套“目录 (2)”。
- 右上角输入部分文件名，按 Enter 筛选当前文件夹，匹配不区分大小写；在筛选框内按 Esc 清除。筛选只影响当前列表，不搜索子目录内容。
- 筛选时状态栏按可见项目计数，例如全选两份匹配文件显示“选定 2 / 2 个项目”。
- 筛选后，全选、普通复制和普通解压默认只处理可见项目；没有匹配项目时阻止普通解压。“智能解压”始终处理整个压缩包，提示文字中有说明。
- 修复普通复制/移动对话框错误显示“全部解压”，以及复制目标路径字符串的生命周期问题。
- 补全解压对话框“定位解压结果”选项的中英文资源，修复中文界面落到其他语言的问题。

## 开发

增强版源码保留在本目录，原版源码仍在 `F:\NanaZip\source`。

工具链：`D:\DevTools\NanaZip\BuildTools`，包含 MSVC x64、ATL、Windows SDK 10.0.26100.0、MSIX 打包工具和 .NET SDK。微软安装程序的共享 SDK、运行时和安装缓存位于系统盘。

在 PowerShell 中运行：

```powershell
& 'F:\NanaZip\custom\source\Build-DailyExperience.ps1'
```

脚本恢复 NuGet 依赖并构建 Release/x64，不清空整个 Output 目录。源码保留上游构建编号；本地增强版由独立目录、Preview 资源及发布记录区分。生成文件位于 `Output\Binaries\Release`，完整布局或包位于其中的 `NanaZipPackage` 子目录。

主要修改：`NanaZip.Modern` 的 XAML 工具栏和复制对话框，`NanaZip.UI.Modern` 的文件列表/选择/命令分发，以及同步的 `NanaZip.UI.Classic` 逻辑。`NanaZip.DailyExperience/Interaction.h` 是筛选桥接的公共头文件。发布验证针对 Modern/x64；Classic 同步源码尚未单独构建验证。

## 范围与来源

参考 Bandizip 的交互思路，自行实现界面和操作连接，没有使用其程序代码、图标或商业资源。

- Bandizip 智能解压规则：https://www.bandisoft.com/bandizip/help/auto_dest/
- NanaZip 项目与来源：https://github.com/M2Team/NanaZip

未在这一版实现图片预览、密码管理、编码自动识别、独立任务队列或 ZIP 并行解压优化，不承诺压缩速度提升。

本地运行采用未注册的应用目录布局，未签名自编译文件不等于微软商店发行包；这一方式不提供完整 Explorer 右键集成。原版应用和快捷方式独立保留。

原项目及第三方许可证见 `License.md` 与仓库中对应说明，本地修改沿用适用的原许可条款。
