# Сквозная проверка консольной версии: миссии, побочные квесты, награды, кампания.
# Каталог данных специально с кириллицей и пробелами.
param([string]$Work = (Join-Path $env:TEMP 'sq smoke'))
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$sq = (Resolve-Path (Join-Path $PSScriptRoot '..\build\sq.exe')).Path
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
$env:SQ_HOME = Join-Path $Work 'данные игры'

$script:failures = 0
function Sq { $ErrorActionPreference = 'Continue'; $out = (& $sq @args 2>&1 | ForEach-Object { "$_" } | Out-String); Write-Host "> sq $args`n$out"; $out }
function Expect([string]$Text, [string]$Needle) {
    if ($Text.Contains($Needle)) { Write-Host "  ok: $Needle" -ForegroundColor Green }
    else { Write-Host "  FAIL: нет «$Needle»" -ForegroundColor Red; $script:failures++ }
}

# --- первый вход: задач нет ----------------------------------------------------
$out = Sq
Expect $out 'Активных задач нет'
Expect $out 'Следующий рубеж 1.1'

# --- миссии -------------------------------------------------------------------------
$out = Sq mission 'Ревью модуля авторизации' 3
Expect $out 'Миссия M1 добавлена'
$out = Sq mission 'Дизайн-документ механизма токенов' 5 --week
Expect $out 'Миссия M2 добавлена'
$out = Sq mission 'Слишком сложно' 7
Expect $out 'Сложность миссии — от 1 до 5'
$out = Sq
Expect $out 'Миссии на сегодня'
Expect $out 'Миссии на неделю'
$out = Sq done M1
Expect $out 'M1 выполнено: +35 опыта · +100 золота'
$out = Sq done m2
Expect $out 'трофей:'
Expect $out 'Новый уровень героя: 2'
$out = Sq done M1
Expect $out 'уже в истории'

# --- побочные квесты --------------------------------------------------------------
$out = Sq log 'Созвон с коллегой' лёгкий
Expect $out 'S1 выполнено: +5 опыта · +20 золота'
$out = Sq side 'Отчёт за месяц' средний
Expect $out 'Побочный квест S2 добавлен'
$out = Sq side 'Что-то' очень-сложный
Expect $out 'Сложность побочного квеста'
$out = Sq mission 'Отменённая задача' 2
$out = Sq drop M3
Expect $out 'M3 убрана'
$out = Sq history
Expect $out 'Созвон с коллегой'
if ($out.Contains('Отменённая задача')) { Write-Host '  FAIL: убранная задача попала в историю' -ForegroundColor Red; $script:failures++ }
else { Write-Host '  ok: убранная задача не в истории' -ForegroundColor Green }

# --- кампания ---------------------------------------------------------------------
$out = Sq game hire griffin 1
Expect $out 'доступны с 8 уровня'
$out = Sq game hire guard 100
Expect $out 'Не хватает'
$out = Sq game hire guard 10
Expect $out 'Готово'
$out = Sq game hire archer 4
$out = Sq game fight 1.3
Expect $out 'Сначала возьмите предыдущие рубежи'
$out = Sq game fight 1.1
Expect $out '1.1 «Застава у старой мельницы»: победа'
$out = Sq game
Expect $out '1.1  Застава у старой мельницы    взят'
Expect $out '1.2  Волчий овраг                 открыт'

# --- награды пересчитываются из журнала -------------------------------------------
$before = Sq
Add-Content -Path (Join-Path $env:SQ_HOME 'config.ini') -Value "`n[rewards]`nmission_gold = 30, 60, 1000, 160, 240" -Encoding UTF8
$after = Sq
$g1 = [int]([regex]::Match($before, 'казна (\d+)').Groups[1].Value)
$g2 = [int]([regex]::Match($after, 'казна (\d+)').Groups[1].Value)
if ($g2 - $g1 -eq 900) { Write-Host '  ok: изменённая награда применилась к прошлым задачам' -ForegroundColor Green }
else { Write-Host "  FAIL: казна {$g1} -> {$g2}" -ForegroundColor Red; $script:failures++ }

# --- кампании из файлов -------------------------------------------------------------
$camps = Join-Path $env:SQ_HOME 'campaigns'
New-Item -ItemType Directory -Force $camps | Out-Null
Copy-Item (Join-Path $PSScriptRoot '..\campaigns\severnaya-marka.ini') $camps
"[campaign]`nid = Плохой id`ntitle = Сломанная`n[chapter.1]`nname = Глава`n[frontier.1]`nchapter = 9`nname = Рубеж`nenemy1 = Гоблин; много; 8; 3; 2; 1-3; 5" | Set-Content (Join-Path $camps 'broken.ini') -Encoding UTF8
$out = Sq game campaigns
Expect $out 'north-march'
Expect $out 'broken.ini: [campaign] id'
Expect $out 'такой главы нет'
$goldShield = [int]([regex]::Match((Sq game), 'Казна (\d+)').Groups[1].Value)
$out = Sq game campaign north-march
Expect $out 'Тени Северной Марки'
$out = Sq game
Expect $out 'Снежная тропа'
$goldNorth = [int]([regex]::Match($out, 'Казна (\d+)').Groups[1].Value)
if ($goldShield -eq $goldNorth) { Write-Host '  ok: казна не меняется при смене кампании' -ForegroundColor Green }
else { Write-Host "  FAIL: казна $goldShield -> $goldNorth при смене кампании" -ForegroundColor Red; $script:failures++ }
$out = Sq game gift 500 проверка
Expect $out 'Готово'
$out = Sq game
Expect $out ("Казна {0} золота" -f ($goldNorth + 500))
$out = Sq game campaign shield
$out = Sq game
Expect $out '1.1  Застава у старой мельницы    взят'

# --- запись боя версии 0.3 (без кампании и добычи) читается как раньше ---------------
$legacy = Join-Path $Work 'legacy'
New-Item -ItemType Directory -Force $legacy | Out-Null
"2026-11-01T10:00:00`ttask.add`tid=M1`tkind=mission`ttitle=x`tdiff=1`tplan=day`n2026-11-01T10:01:00`ttask.done`tid=M1" | Set-Content (Join-Path $legacy 'events.log') -Encoding UTF8
"2026-11-01T10:02:00`thire`tunit=guard`tcount=1`tgold=20`n2026-11-01T10:03:00`tbattle`tfrontier=1.1`twon=1`tlosses=" | Set-Content (Join-Path $legacy 'game.log') -Encoding UTF8
$saved = $env:SQ_HOME; $env:SQ_HOME = $legacy
$out = Sq game
Expect $out 'Казна 70 золота'
Expect $out '1.1  Застава у старой мельницы    взят'
$out = Sq --version
Expect $out 'формат данных 1'
Expect (Get-Content (Join-Path $legacy 'config.ini') -Raw -Encoding UTF8) 'format = 1'
$env:SQ_HOME = $saved

# --- поражение без потерь и сброс игры ----------------------------------------------
$out = Sq game hire guard 3
$before = (Sq game) -replace '(?s).*Стражник × (\d+).*', '$1'
$out = Sq game fight 1.2
Expect $out 'поражение'
$after = (Sq game) -replace '(?s).*Стражник × (\d+).*', '$1'
if ($before -eq $after) { Write-Host '  ok: при поражении армия цела' -ForegroundColor Green }
else { Write-Host "  FAIL: стражников было $before, стало $after" -ForegroundColor Red; $script:failures++ }
$out = Sq game reset
Expect $out 'Подтвердите'
$out = Sq game reset --yes
Expect $out 'Готово'
$out = Sq game
Expect $out '1.1  Застава у старой мельницы    открыт'
Expect $out 'Армия (сила 0)'
if (Get-ChildItem (Join-Path $env:SQ_HOME 'backups') -Directory | Where-Object Name -like '*до сброса игры*') { Write-Host '  ok: копия журнала игры сохранена' -ForegroundColor Green }
else { Write-Host '  FAIL: нет копии перед сбросом' -ForegroundColor Red; $script:failures++ }

# --- Ристалище и новые признаки врагов ----------------------------------------------
$gameLog = Join-Path $env:SQ_HOME 'game.log'
$linesBefore = if (Test-Path $gameLog) { (Get-Content $gameLog -Encoding UTF8).Count } else { 0 }
$out = Sq game arena 20
Expect $out 'Ристалище: побед'
$linesAfter = if (Test-Path $gameLog) { (Get-Content $gameLog -Encoding UTF8).Count } else { 0 }
if ($linesBefore -eq $linesAfter) { Write-Host '  ok: Ристалище ничего не пишет в журнал' -ForegroundColor Green }
else { Write-Host "  FAIL: журнал игры вырос с $linesBefore до $linesAfter строк" -ForegroundColor Red; $script:failures++ }
"[campaign]`nid = flags-test`ntitle = Признаки`n[chapter.1]`nname = Глава`n[frontier.1]`nchapter = 1`nname = Рубеж`nenemy1 = Улан; 5; 20; 5; 3; 4-6; 7; натиск, без ответа`nenemy2 = Самострел; 5; 10; 6; 2; 3-5; 3; стрелок, дальнобой" | Set-Content (Join-Path $camps 'flags.ini') -Encoding UTF8
$out = Sq game campaigns
Expect $out 'flags-test'
if ($out -match 'Ошибка: flags\.ini') { Write-Host '  FAIL: признаки натиск/без ответа/дальнобой не разобраны' -ForegroundColor Red; $script:failures++ }
else { Write-Host '  ok: новые признаки врагов разобраны' -ForegroundColor Green }
if ($script:failures) { Write-Host "`nПровалено проверок: $script:failures" -ForegroundColor Red; exit 1 }
Write-Host "`nВсе проверки пройдены" -ForegroundColor Green
