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
if (-not (Test-Path -LiteralPath $msbuild -PathType Leaf)) {
    throw "MSBuild was not found at $msbuild. Install the components in NanaZip.vsconfig first."
}
$project = Join-Path $PSScriptRoot 'NanaZipPackage\NanaZipPackage.wapproj'
$logRoot = Join-Path $PSScriptRoot 'Output\DailyExperienceLogs'
$null = New-Item -ItemType Directory -Path $logRoot -Force
$common = @(
    '-property:Configuration=Release',
    '-property:Platform=x64',
    '-property:NanaZipBuildPreviewRelease=true',
    '-property:WindowsTargetPlatformVersion=10.0.26100.0',
    '-property:AppxPackageSigningEnabled=false',
    '-property:AppxBundle=Never',
    '-property:UapAppxPackageBuildMode=SideloadOnly',
    '-property:GenerateAppInstallerFile=false',
    '-property:PreferredToolArchitecture=x64',
    '-property:MultiProcessorCompilation=true',
    '-property:CL_MPCount=2'
)
Push-Location -LiteralPath $PSScriptRoot
try {
    & dotnet build '.\NanaZip.Build.Tasks\NanaZip.Build.Tasks.csproj' --configuration Release
    if ($LASTEXITCODE -ne 0) { throw 'Build task compilation failed.' }
    Copy-Item -LiteralPath '.\Output\Binaries\Release\netstandard2.0\NanaZip.Build.Tasks.dll' -Destination '.\NanaZip.Project\NanaZip.Build.Tasks.dll'
    # Keep the base release build number; the separate app folder and release.json identify this fork.
    & $msbuild '.\BuildAllTargets.proj' '-target:RefreshVersion' '-property:NanaZipBuildPreviewRelease=true'
    if ($LASTEXITCODE -ne 0) { throw 'Resource preparation failed.' }
    if (-not $SkipRestore) {
        & $msbuild $project '-target:Restore' @common
        if ($LASTEXITCODE -ne 0) { throw 'NuGet restore failed.' }
    }
    $logArgument = '-fileLoggerParameters:LogFile=' + (Join-Path $logRoot 'native-build.log') + ';Verbosity=normal'
    & $msbuild $project '-target:Build' '-maxCpuCount:4' @common '-verbosity:minimal' $logArgument
    if ($LASTEXITCODE -ne 0) { throw 'Native x64 build failed. See Output\DailyExperienceLogs\native-build.log.' }
    Write-Host ('Build completed. See ' + (Join-Path $PSScriptRoot 'Output\Binaries\Release\NanaZipPackage\x64'))
} finally { Pop-Location }
