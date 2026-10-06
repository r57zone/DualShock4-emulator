param([string]$OutputDirectory = 'output/touchpad', [string]$Python, [switch]$Render)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools not found' }
$msvc = (Get-ChildItem (Join-Path $vs 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
$sdk = (Get-ChildItem (Join-Path $kits 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$env:INCLUDE = "$msvc/include;$kits/Include/$sdk/ucrt;$kits/Include/$sdk/shared;$kits/Include/$sdk/um"
$env:LIB = "$msvc/lib/x64;$kits/Lib/$sdk/ucrt/x64;$kits/Lib/$sdk/um/x64"
Push-Location $root
try {
    New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
    $out = (Resolve-Path $OutputDirectory).Path
    & "$msvc/bin/Hostx64/x64/cl.exe" /nologo /std:c++17 /O2 /EHsc /MT Tests/touchpad_modes.cpp "/Fo$out/touchpad_modes.obj" "/Fe$out/touchpad_modes.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Test compile failed' }
    & "$out/touchpad_modes.exe" | Tee-Object "$out/unit-results.txt"
    if ($LASTEXITCODE -ne 0) { throw 'Touchpad tests failed' }
    & "$msvc/bin/Hostx64/x64/cl.exe" /nologo /std:c++17 /O2 /EHsc /MT Tests/motion_stick.cpp "/Fo$out/motion_stick.obj" "/Fe$out/motion_stick.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Motion test compile failed' }
    & "$out/motion_stick.exe" | Tee-Object "$out/motion-results.txt"
    if ($LASTEXITCODE -ne 0) { throw 'Motion tests failed' }
    & "$msvc/bin/Hostx64/x64/cl.exe" /nologo /std:c++17 /O2 /EHsc /MT Tests/motion_console.cpp "/Fo$out/motion_console.obj" "/Fe$out/motion_console.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Console test compile failed' }
    & "$out/motion_console.exe" | Tee-Object "$out/console-results.txt"
    if ($LASTEXITCODE -ne 0) { throw 'Console tests failed' }
    if ($Python) {
        & $Python Tests/xbox_report_regression.py --output $out
        if ($LASTEXITCODE -ne 0) { throw 'Report extraction failed' }
        & "$msvc/bin/Hostx64/x64/cl.exe" /nologo /std:c++17 /O2 /EHsc /MT /utf-8 "$out/xbox_report_regression.cpp" "/Fo$out/xbox_report_regression.obj" "/Fe$out/xbox_report_regression.exe" *> "$out/regression-build.log"
        if ($LASTEXITCODE -ne 0) { throw 'Report regression compile failed; see regression-build.log' }
        & "$out/xbox_report_regression.exe" | Tee-Object "$out/report-results.txt"
        if ($LASTEXITCODE -ne 0) { throw 'Report regression failed' }
    }
    if ($Render) {
        if (!$Python) { $Python = (Get-Command python -ErrorAction Stop).Source }
        & $Python Tests/touchpad_visualization.py --engine "$out/touchpad_modes.exe" --output $out
        if ($LASTEXITCODE -ne 0) { throw 'Visualization failed' }
    }
} finally { Pop-Location }
