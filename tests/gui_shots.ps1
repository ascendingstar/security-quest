# Снимки страниц окна на демо-данных (см. demo.ps1) для визуальной проверки.
param(
    [Parameter(Mandatory)] [string]$Out,
    [string]$Data = (Join-Path $PSScriptRoot '..\demo'),
    [string]$Size = '1280x820'
)
$ErrorActionPreference = 'Stop'
$gui = (Resolve-Path (Join-Path $PSScriptRoot '..\build\sq-gui.exe')).Path
New-Item -ItemType Directory -Force $Out | Out-Null
$env:SQ_HOME = (Resolve-Path $Data).Path

$shots = @(
    @('overview', @('--page', 'overview')),
    @('missions', @('--page', 'missions')),
    @('sides', @('--page', 'sides')),
    @('campaign', @('--page', 'campaign')),
    @('campaign-boss', @('--dialog', 'select:1.3')),
    @('chronicle', @('--dialog', 'chronicle')),
    @('town', @('--page', 'town')),
    @('history', @('--page', 'history')),
    @('settings', @('--page', 'settings')),
    @('mission-dialog', @('--page', 'overview', '--dialog', 'mission')),
    @('battle', @('--dialog', 'battle:2.3@3.6')),
    @('skirmish', @('--dialog', 'skirmish@2.6')),
    @('skirmish-end', @('--dialog', 'skirmish@40'))
)
foreach ($s in $shots) {
    $args = @('--screenshot', "`"$Out\$($s[0]).png`"", '--size', $Size) + $s[1]
    $p = Start-Process $gui -ArgumentList $args -PassThru -Wait
    "{0,-16} exit {1}" -f $s[0], $p.ExitCode
}

# Первый вход: пустой каталог данных.
$empty = Join-Path $Out 'empty-data'
if (Test-Path $empty) { Remove-Item -Recurse -Force $empty }
$env:SQ_HOME = $empty
$p = Start-Process $gui -ArgumentList @('--screenshot', "`"$Out\first-run.png`"", '--size', $Size) -PassThru -Wait
"{0,-16} exit {1}" -f 'first-run', $p.ExitCode
