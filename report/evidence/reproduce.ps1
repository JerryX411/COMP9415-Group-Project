param(
    [string]$Executable = '',
    [string]$Destination = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $Executable) {
    $Executable = Join-Path $projectRoot 'build/CoreExplorer.exe'
    if (-not (Test-Path -LiteralPath $Executable)) { $Executable = Join-Path $projectRoot 'build/Release/CoreExplorer.exe' }
}
if (-not $Destination) { $Destination = Join-Path $projectRoot 'output/report-reproduction' }
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$resolvedDestination = (Resolve-Path -LiteralPath $Destination).Path
Push-Location $projectRoot
try {
    $levels = @('tutorial', 'helios', 'corona')
    for ($i = 0; $i -lt $levels.Count; $i++) {
        & $resolvedExecutable --benchmark --level $i --benchmark-output (Join-Path $resolvedDestination ($levels[$i] + '.csv'))
        if ($LASTEXITCODE -ne 0) { throw "Benchmark failed: $($levels[$i])" }
    }
    foreach ($eventName in @('core', 'impact', 'pad', 'run', 'gate-closed', 'gate-opening', 'gate-open')) {
        & $resolvedExecutable --effect $eventName --frames 2 --capture (Join-Path $resolvedDestination ($eventName + '.png'))
        if ($LASTEXITCODE -ne 0) { throw "Capture failed: $eventName" }
    }
    & $resolvedExecutable --play --freeze --frames 2 --capture (Join-Path $resolvedDestination 'normal-on.png')
    if ($LASTEXITCODE -ne 0) { throw 'Normal-ON capture failed' }
    & $resolvedExecutable --play --freeze --normal-off --frames 2 --capture (Join-Path $resolvedDestination 'normal-off.png')
    if ($LASTEXITCODE -ne 0) { throw 'Normal-OFF capture failed' }
}
finally { Pop-Location }
