# 构建 MiaoZip

## 已验证环境

- Windows 11 x64。
- Visual Studio C++ Build Tools，MSVC 14.51 与 ATL/MFC 组件。
- Windows SDK 10.0.26100.0。
- .NET SDK 10（用于编译 netstandard2.0 构建任务）。
- PowerShell、Git；NuGet 还原需要网络。

## 构建入口

`Build-MiaoZip.ps1` 的 `ToolRoot` 参数指向独立工具链根目录。默认 `D:\DevTools\NanaZip`，可以指定其他位置。脚本预期该根目录下存在 `BuildTools\MSBuild\Current\Bin\MSBuild.exe`，并在其下放置 NuGet 缓存和 .NET CLI 配置目录；将 Visual Studio Build Tools 的安装目录设置为相应的 `BuildTools` 子目录即可。

在源码根目录执行：

```powershell
.\Build-MiaoZip.ps1 -ToolRoot 'D:\DevTools\NanaZip'
```

依赖已还原时可以加 `-SkipRestore`。脚本构建 Release x64，固定 MiaoZip 版本为 1.0.0.0，并生成未签名 MSIX。结果位于 `Output/Binaries/AppPackages/`；本次发布运行包从其中的 x64 MSIX 提取运行文件，并附带许可、说明和 ICO。

`BuildAllTargets.cmd` 为保留的上游多目标构建入口，MiaoZip v1.0 使用上面的专用脚本。

## 测试

```powershell
.\MiaoZip.Tests\Test-Features.ps1 -ToolRoot 'D:\DevTools\NanaZip'
.\MiaoZip.Tests\Test-Engine.ps1 -ConsoleExe '解压后的运行目录\NanaZip.Universal.Console.exe' -ResultPath '.\Output\engine-validation.json'
python .\MiaoZip.Features\Build-BrandAssets.py --check
```

最后一项需要 Pillow；`--check` 只验证已提交的图标。去掉该参数会从保留的 `Assets/MiaoZip.png` 主图重新生成 ICO 与安装包 PNG，不会生成或改绘猫脸主图。

## 实现入口

| 路径 | 用途 |
|---|---|
| `MiaoZip.Features/LiveEdit.*` | 小文件编辑会话、监测、冲突和恢复 |
| `MiaoZip.Features/PanelLiveEdit.inl` | 文件列表与包内编辑集成 |
| `MiaoZip.Features/Caption.h` | 一体化标题绘制与窗口命中测试 |
| `MiaoZip.Features/Theme.h` | 原生主题与材质回退 |
| `MiaoZip.Features/TreeSurface.h`、`FolderTree.inl` | 目录树绘制、展开和导航 |
| `MiaoZip.Features/AppIcon.h` | 应用内部压缩包图标 |
| `MiaoZip.Features/ExtractionPreferences.h` | 共用的解压整理偏好 |
| `NanaZip.Modern/MainWindowToolBarPage.xaml` | 工具栏布局和按钮状态 |
| `NanaZip.Modern/CopyLocationPage.*` | 所选文件的解压位置设置 |
| `K7User/K7UserDarkMode.cpp` | 应用主题与系统文件选择窗口的隔离 |

设置使用 `HKCU\Software\MiaoZip`。上游原生工程和第三方源码按原有目录保留；本仓库不提交个人恢复材料、构建输出和工具链缓存。
