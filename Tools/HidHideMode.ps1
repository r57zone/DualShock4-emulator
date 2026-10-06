param(
    [ValidateSet('DS4', 'Xbox', 'Status')][string]$Mode = 'Status',
    [string]$EmulatorPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'DS4Emulator.exe')
)
$ErrorActionPreference = 'Stop'
$taskCli = Join-Path $env:ProgramFiles 'Nefarius Software Solutions/HidHide/x64/HidHideCLI.exe'
if (!(Test-Path -LiteralPath $taskCli)) { throw 'Install HidHide from its official release first.' }
if ($Mode -eq 'DS4') {
    if (!(Test-Path -LiteralPath $EmulatorPath -PathType Leaf)) { throw 'Specify -EmulatorPath with the full path to DS4Emulator.exe.' }
    $taskNativePath = (Resolve-Path -LiteralPath $EmulatorPath).Path
    & $taskCli --app-reg $taskNativePath --inv-off --cloak-on
    $taskState = (& $taskCli --cloak-state | Out-String).Trim()
    $taskApps = (& $taskCli --app-list | Out-String)
    if ($taskState -ne '--cloak-on' -or !$taskApps.Contains($taskNativePath)) { throw 'HidHide DS4-mode configuration failed.' }
    Write-Output 'DS4 mode: configured devices are hidden; this DS4Emulator path is allowed. Start DS4Emulator before the game.'
} elseif ($Mode -eq 'Xbox') {
    & $taskCli --cloak-off
    if ((& $taskCli --cloak-state | Out-String).Trim() -ne '--cloak-off') { throw 'HidHide Xbox-mode configuration failed.' }
    Write-Output 'Xbox mode: hiding is off. Close DS4Emulator and reopen the game to use the physical Xbox controller.'
} else {
    & $taskCli --cloak-state --inv-state --app-list --dev-list
}
