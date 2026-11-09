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
    @('edit-dialog', @('--page', 'missions', '--dialog', 'edit:M7')),
    @('cutscene-easy', @('--dialog', 'cutscene:1@1.2')),
    @('cutscene-easy-end', @('--dialog', 'cutscene:1@6')),
    @('cutscene-clash', @('--dialog', 'cutscene:3@1.6')),
    @('cutscene-fireball', @('--dialog', 'cutscene:3@2.3')),
    @('cutscene-quest-end', @('--dialog', 'cutscene:3@7')),
    @('cutscene-lightning', @('--dialog', 'cutscene:4@2.7')),
    @('cutscene-legend', @('--dialog', 'cutscene:6@1.6')),
    @('cutscene-boss', @('--dialog', 'cutscene:6@4.05')),
    @('cutscene-victory', @('--dialog', 'cutscene:6@4.9')),
    @('cutscene-legend-end', @('--dialog', 'cutscene:6@9'))
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
