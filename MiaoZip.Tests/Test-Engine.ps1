param(
    [string]$ConsoleExe = 'F:\MiaoZip\app\NanaZip.Universal.Console.exe',
    [string]$ResultPath = 'F:\MiaoZip\docs\引擎验证.json'
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$testRoot = Join-Path (Join-Path $PSScriptRoot '..\Output\MiaoZipTests') ('native-validation-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$inputRoot = Join-Path $testRoot 'input'
$null = New-Item -ItemType Directory -Path (Join-Path $inputRoot '资料')
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $inputRoot '合同-甲.txt'), "演示合同甲：用于中文文件名筛选和内容校验。`r`n", $utf8)
[IO.File]::WriteAllText((Join-Path $inputRoot '合同-乙.txt'), "演示合同乙：这些均为自动生成的测试内容。`r`n", $utf8)
[IO.File]::WriteAllText((Join-Path $inputRoot '说明.txt'), "MiaoZip测试样例。`r`n", $utf8)
[IO.File]::WriteAllText((Join-Path $inputRoot '资料\合同-附件.txt'), '子目录文件，筛选默认仅作用于当前列表。', $utf8)
$bytes = New-Object byte[] 32768
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
[IO.File]::WriteAllBytes((Join-Path $inputRoot '样本.bin'), $bytes)
[IO.File]::WriteAllBytes((Join-Path $inputRoot '空文件.txt'), [byte[]]::new(0))
$results = [Collections.Generic.List[object]]::new()
function Invoke-Native([string]$Name, [string[]]$Arguments, [int]$ExpectedExit = 0) {
    & $ConsoleExe @Arguments *> (Join-Path $testRoot ($Name + '.log'))
    $nativeExit = $LASTEXITCODE
    if ($nativeExit -ne $ExpectedExit) { throw "${Name}: expected $ExpectedExit, got $nativeExit" }
    return $nativeExit
}
function Compare-Tree([string]$Expected, [string]$Actual) {
    $files = @(Get-ChildItem -LiteralPath $Expected -File -Recurse)
    $outFiles = @(Get-ChildItem -LiteralPath $Actual -File -Recurse)
    if ($files.Count -ne $outFiles.Count) { throw 'Extracted file count mismatch' }
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($Expected, $file.FullName)
        $outFile = Join-Path $Actual $relative
        if (-not (Test-Path -LiteralPath $outFile -PathType Leaf)) { throw "Missing: $relative" }
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $outFile).Hash) {
            throw "SHA256 mismatch: $relative"
        }
    }
    return $files.Count
}
foreach ($case in @(
    @{Name='zip'; Format='zip'; Extra=@()},
    @{Name='7z'; Format='7z'; Extra=@()},
    @{Name='zip-aes'; Format='zip'; Extra=@('-pNanaZipSyntheticTestOnly2026','-mem=AES256')}
)) {
    $archive = Join-Path $testRoot ($case.Name + '.' + $case.Format)
    Push-Location -LiteralPath $inputRoot
    try { $createExit = Invoke-Native ($case.Name + '-create') (@('a', ('-t' + $case.Format), $archive, '.\*', '-mx=1', '-y') + $case.Extra) }
    finally { Pop-Location }
    $password = @($case.Extra | Where-Object { $_.StartsWith('-p') })
    $testExit = Invoke-Native ($case.Name + '-integrity') (@('t',$archive) + $password)
    $extracted = Join-Path $testRoot ($case.Name + '-extracted')
    $extractExit = Invoke-Native ($case.Name + '-extract') (@('x',$archive,('-o'+$extracted),'-y') + $password)
    $compared = Compare-Tree $inputRoot $extracted
    $results.Add([ordered]@{Name=$case.Name; CreateExitCode=$createExit; IntegrityExitCode=$testExit; ExtractExitCode=$extractExit; FilesCompared=$compared; SHA256RoundTrip=$true})
}
$wrongPasswordExit = Invoke-Native 'wrong-password' @('t',(Join-Path $testRoot 'zip-aes.zip'),'-pDeliberatelyWrongPassword') 2

# These verify the exact -sps and -aou switches used by the new native button.
$smartCases = @(
    @{Name='smart-single-file'; Items=@('.\说明.txt'); Expected='说明.txt'},
    @{Name='smart-root-folder'; Items=@('.\资料'); Expected='资料\合同-附件.txt'},
    @{Name='smart-loose-files'; Items=@('.\合同-甲.txt','.\合同-乙.txt'); Expected='smart-loose-files\合同-甲.txt'}
)
foreach ($case in $smartCases) {
    $archive = Join-Path $testRoot ($case.Name + '.zip')
    Push-Location -LiteralPath $inputRoot
    try { $null = Invoke-Native ($case.Name + '-create') (@('a','-tzip',$archive,'-y') + $case.Items) }
    finally { Pop-Location }
    $extracted = Join-Path $testRoot ($case.Name + '-out')
    $null = Invoke-Native ($case.Name + '-extract') @('x',$archive,('-o'+$extracted),'-sps','-aou','-y')
    $expectedPath = Join-Path $extracted $case.Expected
    if (-not (Test-Path -LiteralPath $expectedPath -PathType Leaf)) { throw ($case.Name + ': incorrect smart destination') }
    $before = Get-FileHash -LiteralPath $expectedPath
    $countBefore = @(Get-ChildItem -LiteralPath $extracted -File -Recurse).Count
    $null = Invoke-Native ($case.Name + '-collision') @('x',$archive,('-o'+$extracted),'-sps','-aou','-y')
    $countAfter = @(Get-ChildItem -LiteralPath $extracted -File -Recurse).Count
    if ((Get-FileHash -LiteralPath $expectedPath).Hash -ne $before.Hash -or $countAfter -ne (2 * $countBefore)) {
        throw ($case.Name + ': collision did not preserve existing files and rename new files')
    }
    $results.Add([ordered]@{Name=$case.Name; ExpectedRelativePath=$case.Expected; SmartDestinationCorrect=$true; ExistingFilePreserved=$true; NewFilesRenamed=$true; FilesAfterSecondExtraction=$countAfter})
}
$uiArchive = Join-Path $testRoot '日常体验演示.zip'
Copy-Item -LiteralPath (Join-Path $testRoot 'zip.zip') -Destination $uiArchive
$record = [ordered]@{
    CheckedAt=[DateTimeOffset]::Now.ToString('o')
    ConsoleExe=$ConsoleExe
    ConsoleSHA256=(Get-FileHash -LiteralPath $ConsoleExe).Hash
    TestRoot=$testRoot
    SyntheticGUIFixture=$uiArchive
    Cases=$results
    WrongPasswordRejected=($wrongPasswordExit -eq 2)
    GUI='Pending separate visible-window verification'
}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ResultPath -Encoding utf8
$record | ConvertTo-Json -Depth 8
