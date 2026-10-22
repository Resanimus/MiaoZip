param([string]$ToolRoot = 'D:\DevTools\NanaZip')
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$output = Join-Path $PSScriptRoot '..\Output\MiaoZipTests\dialogs'
$null = New-Item -ItemType Directory -Path $output -Force
$vcvars = Join-Path $ToolRoot 'BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$source = Join-Path $PSScriptRoot 'Test-Dialogs.cpp'
$exe = Join-Path $output 'dialogs-test.exe'
$command = '"' + $vcvars + '" >nul && cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MD /W4 /WX /DUNICODE /D_UNICODE ' +
    '"/Fe:' + $exe + '" "' + $source + '" /link user32.lib gdi32.lib advapi32.lib comctl32.lib dwmapi.lib uxtheme.lib'
Push-Location -LiteralPath $output
try {
    & cmd.exe /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Dialog test compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'Dialog tests failed.' }
} finally { Pop-Location }
