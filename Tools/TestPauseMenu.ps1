param(
    [string]$EditorPath = 'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$ServerPath = "$PSScriptRoot\..\..\WS_StickArenaServer\bin\x64\Release\VS_StickArenaServer.exe"
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath "$PSScriptRoot\..").Path
$project = Join-Path $root 'UE_StickArenaDemo.uproject'
$logs = Join-Path $root 'Saved\Logs'
$probe = [Net.Sockets.UdpClient]::new(0)
$port = $probe.Client.LocalEndPoint.Port
$probe.Dispose()
$serverInfo = [Diagnostics.ProcessStartInfo]::new()
$serverInfo.FileName = (Resolve-Path -LiteralPath $ServerPath).Path
$serverInfo.Arguments = [string]$port
$serverInfo.UseShellExecute = $false
$serverInfo.CreateNoWindow = $true
$serverInfo.RedirectStandardOutput = $true
$serverInfo.RedirectStandardError = $true
$server = [Diagnostics.Process]::Start($serverInfo)
$output = $server.StandardOutput.ReadToEndAsync()
$errors = $server.StandardError.ReadToEndAsync()
$game = $null
try {
    Start-Sleep -Milliseconds 300
    foreach ($mode in @('Controls', 'Quit')) {
        $log = Join-Path $logs "Menu${mode}Live.log"
        $info = [Diagnostics.ProcessStartInfo]::new()
        $info.FileName = $EditorPath
        $info.UseShellExecute = $false
        $info.CreateNoWindow = $true
        $quitFlag = if ($mode -eq 'Quit') { '-SAMenuQuitTest' } else { '' }
        $info.Arguments = '"{0}" /Game/Maps/Main -game -d3d11 -windowed -ResX=1280 -ResY=720 -unattended -nosound -SAServerPort={1} {2} "-ExecCmds=DisableAllScreenMessages,Automation RunTests StickArena.Menu.{3}Live" "-TestExit=Automation Test Queue Empty" "-abslog={4}"' -f $project, $port, $quitFlag, $mode, $log
        $game = [Diagnostics.Process]::Start($info)
        if (!$game.WaitForExit(90000)) { throw "$mode game test timed out" }
        if ($game.ExitCode -ne 0) { throw "$mode game exited with code $($game.ExitCode); see $log" }
        $game.Dispose()
        $game = $null
        $text = Get-Content -LiteralPath $log -Raw
        if ($text -match 'LogAutomationController: Error:|LogAutomationTest: Error:') { throw "$mode automation failed; see $log" }
        if ($mode -eq 'Controls' -and $text -notmatch 'Test Completed. Result=\{Success\}.*ControlsLive') { throw 'Controls test did not complete' }
        if ($mode -eq 'Quit') {
            $match = [regex]::Match($text, 'Menu quit: disconnecting session ([0-9a-fA-F]{32})')
            if (!$match.Success) { throw 'Quit button did not invoke disconnect' }
            $quitSession = $match.Groups[1].Value
        }
    }
    Start-Sleep -Milliseconds 300
} finally {
    # Only the two processes created by this script are eligible for cleanup.
    if ($game) {
        if (!$game.HasExited) { $game.Kill(); $game.WaitForExit() }
        $game.Dispose()
    }
    if (!$server.HasExited) { $server.Kill() }
    $server.WaitForExit()
    $stdout = $output.GetAwaiter().GetResult()
    $stderr = $errors.GetAwaiter().GetResult()
    $server.Dispose()
    $stdout | Set-Content -LiteralPath (Join-Path $logs 'MenuTestServer.log')
    $stderr | Set-Content -LiteralPath (Join-Path $logs 'MenuTestServerError.log')
}
if ($stdout -notmatch "Player left $quitSession \| players=0 \| reason=LEAVE") {
    throw "Server did not immediately remove the quit-menu session: $stdout $stderr"
}
Write-Output 'PASS: Esc opens/closes menu; Continue restores movement; menu blocks movement/attack while networking stays connected; Quit button exits and server removes its session via LEAVE.'
