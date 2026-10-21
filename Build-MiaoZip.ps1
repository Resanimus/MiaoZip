param(
    [string]$ToolRoot = 'D:\DevTools\NanaZip',
    [switch]$SkipRestore
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$env:NUGET_PACKAGES = Join-Path $ToolRoot 'nuget'
$env:DOTNET_CLI_HOME = Join-Path $ToolRoot 'dotnet-home'
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$msbuild = Join-Path $ToolRoot 'BuildTools\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild -PathType Leaf)) { throw "MSBuild is missing: $msbuild" }
$project = Join-Path $PSScriptRoot 'NanaZipPackage\NanaZipPackage.wapproj'
$logRoot = Join-Path $PSScriptRoot 'Output\MiaoZipLogs'
$null = New-Item -ItemType Directory -Path $logRoot -Force
$common = @(
    '-property:Configuration=Release', '-property:Platform=x64',
    '-property:NanaZipBuildPreviewRelease=false', '-property:WindowsTargetPlatformVersion=10.0.26100.0',
    '-property:AppxPackageSigningEnabled=false', '-property:AppxBundle=Never',
    '-property:UapAppxPackageBuildMode=SideloadOnly', '-property:GenerateAppInstallerFile=false',
    '-property:PreferredToolArchitecture=x64', '-property:MultiProcessorCompilation=true', '-property:CL_MPCount=2'
)
Push-Location -LiteralPath $PSScriptRoot
try {
    & dotnet build '.\NanaZip.Build.Tasks\NanaZip.Build.Tasks.csproj' --configuration Release
    if ($LASTEXITCODE -ne 0) { throw 'Build task compilation failed.' }
    Copy-Item -LiteralPath '.\Output\Binaries\Release\netstandard2.0\NanaZip.Build.Tasks.dll' -Destination '.\NanaZip.Project\NanaZip.Build.Tasks.dll'
    # MiaoZip owns its manifest and product version; upstream RefreshVersion would overwrite them.
    if (-not $SkipRestore) {
        & $msbuild $project '-target:Restore' @common
        if ($LASTEXITCODE -ne 0) { throw 'NuGet restore failed.' }
    }
    & $msbuild $project '-target:Build' '-maxCpuCount:4' @common '-verbosity:minimal' ('-fileLoggerParameters:LogFile=' + (Join-Path $logRoot 'native-build.log') + ';Verbosity=normal')
    if ($LASTEXITCODE -ne 0) { throw 'MiaoZip build failed. See Output\MiaoZipLogs\native-build.log.' }
    Write-Host ('MiaoZip built: ' + (Join-Path $PSScriptRoot 'Output\Binaries\Release\NanaZipPackage\x64\MiaoZip.exe'))
} finally { Pop-Location }
