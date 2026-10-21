param([string]$ToolRoot = 'D:\DevTools\NanaZip')
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$output = Join-Path $PSScriptRoot ('..\Output\MiaoZipTests\features-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$app = Join-Path $output 'app'
$null = New-Item -ItemType Directory -Path $app -Force
$vcvars = Join-Path $ToolRoot 'BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$source = Join-Path $PSScriptRoot 'Test-Features.cpp'
$implementation = Join-Path $PSScriptRoot '..\MiaoZip.Features\LiveEdit.cpp'
$exe = Join-Path $app 'features-test.exe'
$command = '"' + $vcvars + '" >nul && cl /nologo /std:c++20 /EHsc /utf-8 /O2 /MD /W4 /WX /DUNICODE /D_UNICODE ' +
    '"/Fe:' + $exe + '" "' + $source + '" "' + $implementation + '" /link ole32.lib bcrypt.lib user32.lib'
Push-Location -LiteralPath $output
try {
    & cmd.exe /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Native feature test compilation failed.' }
} finally { Pop-Location }
& $exe (Join-Path $output 'fixtures')
if ($LASTEXITCODE -ne 0) { throw 'Native feature tests failed.' }
Write-Host ('Tests passed. Fixtures and recovery records: ' + $output)
