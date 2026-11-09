# Демо: заполняет каталог demo примером недели работы и начала кампании и показывает результат.
# Ваши настоящие данные не затрагиваются. С ключом -Gui открывает окно на демо-данных.
param([switch]$Gui)
$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$data = Join-Path $PSScriptRoot 'demo'
if (Test-Path $data) { Remove-Item -Recurse -Force $data }
$env:SQ_HOME = $data
$sq = Join-Path $PSScriptRoot 'build\sq.exe'
function Q { & $sq @args 2>&1 | Out-Null }

Write-Host 'Готовлю демо-данные: неделя работы и начало кампании...'
Q mission 'Ревью модуля авторизации' 3
Q mission 'Дизайн-документ механизма токенов' 5 --week
Q mission 'Разобрать отчёт статического анализатора' 2
Q mission 'Починить проверку подписи в загрузчике' 4
Q done M1; Q done M2; Q done M3; Q done M4
Q log 'Созвон с коллегой по интеграции' лёгкий
Q log 'Помог на обсуждении архитектуры' средний
Q mission 'Ревью изменений в IPC-шлюзе' 3
Q mission 'Тесты на неправильное использование API' 2
Q mission 'Механизм ротации ключей' 5 --week
Q side 'Заполнить отчёт за месяц' средний
Q game hire guard 10
Q game hire archer 6
Q game fight 1.1

if ($Gui) {
    Start-Process (Join-Path $PSScriptRoot 'build\sq-gui.exe')
    Write-Host 'Окно открыто на демо-данных. Закрыть совсем — правой кнопкой по значку в трее, «Выход».'
    return
}
& $sq
Write-Host "`nДемо-данные: $data. Чтобы поработать с ними: `$env:SQ_HOME = '$data'"
