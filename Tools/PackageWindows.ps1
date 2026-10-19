param(
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.4',
    [string]$ArchiveDirectory = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'UE_StickArenaDemo.uproject'
if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
    $ArchiveDirectory = Join-Path $projectRoot 'Saved/Packages'
}

# Cooking must use the same native property schema as the packaged executable.
# A successful hot-reload build does not update the canonical editor DLL.
$projectEditors = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" |
    Where-Object { $_.CommandLine -and $_.CommandLine.Contains($projectFile) }
if ($projectEditors) {
    throw 'Save your work and close this project in Unreal Editor before packaging.'
}

$build = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
$uat = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
if (!(Test-Path -LiteralPath $build) -or !(Test-Path -LiteralPath $uat)) {
    throw "Unreal Engine build tools not found in $EngineRoot"
}

& $build UE_StickArenaDemoEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoLiveCoding
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed; cooking was not started.' }

# No iterative/skip-cook flags: recook Blueprints after reflected C++ layout changes.
& $uat BuildCookRun "-project=$projectFile" -noP4 -utf8output -unattended -platform=Win64 -clientconfig=Development `
    -build -skipbuildeditor -cook -stage -package -pak -iostore -compressed -archive "-archivedirectory=$ArchiveDirectory"
if ($LASTEXITCODE -ne 0) { throw 'Windows packaging failed.' }

Write-Host "Packaged game: $ArchiveDirectory/Windows/UE_StickArenaDemo.exe"
