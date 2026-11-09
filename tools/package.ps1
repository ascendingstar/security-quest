# Собирает портативную версию:
#   dist\SecurityQuest\                — рабочая копия; обновляются только программы, кампании и README
#   dist\SecurityQuest-portable.zip    — для первой установки (с пустой папкой data)
#   dist\SecurityQuest-update.zip      — для обновления: без папки data, распаковать поверх старой версии
# Папка data — данные пользователя: скрипт её никогда не удаляет и не кладёт в архивы.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$dist = Join-Path $root 'dist'
$app = Join-Path $dist 'SecurityQuest'

$running = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($app, [StringComparison]::OrdinalIgnoreCase) }
if ($running) {
    Write-Host "Программа из $app сейчас запущена. Закройте её (значок в трее → «Выход») и запустите упаковку снова." -ForegroundColor Yellow
    exit 1
}

& (Join-Path $root 'build.ps1')
if ($LASTEXITCODE) { throw 'Сборка не прошла' }
$version = (& (Join-Path $root 'build\sq.exe') --version) -replace '^Security Quest (\S+).*$', '$1'

$readme = @'
Security Quest — портативная версия

Security Quest.exe  — окно (двойной щелчок)
sq.exe              — то же из командной строки (sq help)
campaigns\          — кампании, которые идут с программой
data\               — ваши данные: config.ini, events.log, game.log, свои кампании, резервные копии

Папку можно положить куда угодно, в том числе на флешку. Установка не нужна,
в реестр и профиль пользователя ничего не пишется. Нужна Windows 10 или новее.
Чтобы перенести прогресс, скопируйте папку data.

Обновление: закройте программу (значок в трее → «Выход»), распакуйте
SecurityQuest-update.zip прямо в эту папку с заменой файлов и запустите снова.
Папки data в архиве обновления нет, поэтому прогресс не пострадает.
'@

function Fill([string]$dir, [bool]$withData) {
    New-Item -ItemType Directory -Force $dir, (Join-Path $dir 'campaigns') | Out-Null
    if ($withData) { New-Item -ItemType Directory -Force (Join-Path $dir 'data') | Out-Null }
    Copy-Item (Join-Path $root 'build\sq-gui.exe') (Join-Path $dir 'Security Quest.exe') -Force
    Copy-Item (Join-Path $root 'build\sq.exe') (Join-Path $dir 'sq.exe') -Force
    Copy-Item (Join-Path $root 'campaigns\*.ini') (Join-Path $dir 'campaigns') -Force
    Copy-Item (Join-Path $root 'docs\campaign-format.md') (Join-Path $dir 'campaigns\ФОРМАТ.md') -Force
    $readme | Set-Content -Path (Join-Path $dir 'README.txt') -Encoding UTF8
}

# Рабочая копия: data остаётся как есть.
Fill $app $true

# Архивы собираются из отдельных чистых копий, чтобы в них не попали ничьи данные.
$stagingRoot = Join-Path $dist '_staging'
foreach ($kind in @(@('portable', $true), @('update', $false))) {
    if (Test-Path $stagingRoot) { Remove-Item -Recurse -Force $stagingRoot }
    $staging = Join-Path $stagingRoot 'SecurityQuest'
    Fill $staging $kind[1]
    $zip = Join-Path $dist "SecurityQuest-$($kind[0]).zip"
    if (Test-Path $zip) { Remove-Item $zip }
    # Полный архив — с папкой SecurityQuest; обновление — файлы сразу в корне, чтобы распаковать прямо в папку программы.
    $what = if ($kind[1]) { $staging } else { Join-Path $staging '*' }
    Compress-Archive -Path $what -DestinationPath $zip
}
Remove-Item -Recurse -Force $stagingRoot

Write-Host "Версия $version"
Get-ChildItem $app | Select-Object Name, Length
Get-ChildItem $dist -Filter *.zip | Select-Object Name, Length
