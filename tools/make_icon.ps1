# Рисует gui\app.ico: геральдический щит Хранителя с золотой замочной скважиной и скрещёнными мечами
# на винно-красной плитке с золотой каймой. Запускается один раз, результат лежит в репозитории,
# при сборке не нужен. Тот же рисунок в окне — draw_emblem (gui\battle.cpp).
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function P([double]$x, [double]$y) { New-Object Drawing.PointF ([single]$x), ([single]$y) }
function C([int]$a, [int]$r, [int]$g, [int]$b) { [Drawing.Color]::FromArgb($a, $r, $g, $b) }

function Shield-Path([double]$u, [double]$inset) {
    # Щит-«капля»: прямой верх, прямые бока, закруглённый низ к острию.
    $l = 24 + $inset; $r = 76 - $inset; $t = 17 + $inset; $m = 50; $bot = 91 - $inset * 1.4
    $p = New-Object Drawing.Drawing2D.GraphicsPath
    $p.AddLine((P ($l*$u) ($t*$u)), (P ($r*$u) ($t*$u)))
    $p.AddLine((P ($r*$u) ($t*$u)), (P ($r*$u) (50*$u)))
    $p.AddBezier((P ($r*$u) (50*$u)), (P ($r*$u) (70*$u)), (P (($m+12)*$u) (82*$u)), (P ($m*$u) ($bot*$u)))
    $p.AddBezier((P ($m*$u) ($bot*$u)), (P (($m-12)*$u) (82*$u)), (P ($l*$u) (70*$u)), (P ($l*$u) (50*$u)))
    $p.CloseFigure()
    , $p
}

function Keyhole-Path([double]$u) {
    $p = New-Object Drawing.Drawing2D.GraphicsPath
    $p.AddEllipse([single](41*$u), [single](34*$u), [single](18*$u), [single](18*$u))
    $p.AddPolygon([Drawing.PointF[]]@((P (46.5*$u) (46*$u)), (P (53.5*$u) (46*$u)), (P (56*$u) (70*$u)), (P (44*$u) (70*$u))))
    , $p
}

function Draw-Sword($g, [double]$u, $from, $to) {
    # Клинок от рукояти (from) к острию (to), гарда и навершие золотые.
    $dx = $to.X - $from.X; $dy = $to.Y - $from.Y; $len = [Math]::Sqrt($dx*$dx + $dy*$dy)
    $nx = $dx / $len; $ny = $dy / $len
    $blade = New-Object Drawing.Pen (C 255 222 228 238), ([single](6.5*$u))
    $blade.EndCap = 'Triangle'
    $edge = New-Object Drawing.Pen (C 255 120 128 146), ([single](1.4*$u))
    $gx = $from.X + $nx * 13*$u; $gy = $from.Y + $ny * 13*$u
    $g.DrawLine($blade, [single]$gx, [single]$gy, $to.X, $to.Y)
    $g.DrawLine($edge, [single]$gx, [single]$gy, $to.X, $to.Y)
    $hilt = New-Object Drawing.Pen (C 255 120 72 26), ([single](4.5*$u))
    $g.DrawLine($hilt, $from.X, $from.Y, [single]$gx, [single]$gy)
    $guard = New-Object Drawing.Pen (C 255 236 180 80), ([single](4.5*$u))
    $guard.StartCap = 'Round'; $guard.EndCap = 'Round'
    $g.DrawLine($guard, [single]($gx - $ny*9*$u), [single]($gy + $nx*9*$u), [single]($gx + $ny*9*$u), [single]($gy - $nx*9*$u))
    $g.FillEllipse((New-Object Drawing.SolidBrush (C 255 236 180 80)), [single]($from.X - 4*$u), [single]($from.Y - 4*$u), [single](8*$u), [single](8*$u))
}

function New-Frame([int]$s) {
    $bmp = New-Object Drawing.Bitmap $s, $s, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.PixelOffsetMode = 'HighQuality'
    $g.Clear([Drawing.Color]::Transparent)
    $u = $s / 100.0
    $small = $s -le 24

    # Плитка: винная кожа с золотой каймой.
    $r = [single]($s * 0.2)
    $bg = New-Object Drawing.Drawing2D.GraphicsPath
    $bg.AddArc(0, 0, $r, $r, 180, 90); $bg.AddArc($s - $r - 1, 0, $r, $r, 270, 90)
    $bg.AddArc($s - $r - 1, $s - $r - 1, $r, $r, 0, 90); $bg.AddArc(0, $s - $r - 1, $r, $r, 90, 90); $bg.CloseFigure()
    $tile = New-Object Drawing.Drawing2D.LinearGradientBrush (P 0 0), (P 0 $s), (C 255 112 34 42), (C 255 38 14 20)
    $g.FillPath($tile, $bg)
    if (-not $small) {
        $rim = New-Object Drawing.Pen (C 255 214 160 70), ([single][Math]::Max(1, 2.5*$u))
        $inner = New-Object Drawing.Drawing2D.GraphicsPath
        $k = 5*$u; $ri = [single]($r * 0.75)
        $inner.AddArc([single]$k, [single]$k, $ri, $ri, 180, 90); $inner.AddArc([single]($s - $k - $ri), [single]$k, $ri, $ri, 270, 90)
        $inner.AddArc([single]($s - $k - $ri), [single]($s - $k - $ri), $ri, $ri, 0, 90); $inner.AddArc([single]$k, [single]($s - $k - $ri), $ri, $ri, 90, 90)
        $inner.CloseFigure()
        $g.DrawPath($rim, $inner)
    }

    # Скрещённые мечи за щитом — только там, где они различимы.
    if ($s -ge 32) {
        Draw-Sword $g $u (P (20*$u) (82*$u)) (P (84*$u) (14*$u))
        Draw-Sword $g $u (P (80*$u) (82*$u)) (P (16*$u) (14*$u))
    }

    # Щит: золотая оправа, тёмное поле, золотая замочная скважина.
    $outer = Shield-Path $u ($(if ($small) { -4 } else { 0 }))
    $gold = New-Object Drawing.Drawing2D.LinearGradientBrush (P 0 (15*$u)), (P 0 (92*$u)), (C 255 255 222 130), (C 255 190 120 36)
    if (-not $small) { $g.FillPath((New-Object Drawing.SolidBrush (C 120 0 0 0)), (Shield-Path $u -1.5)) }
    $g.FillPath($gold, $outer)
    if ($small) {
        $g.FillPath((New-Object Drawing.SolidBrush (C 255 52 18 24)), (Keyhole-Path $u))
    } else {
        $field = New-Object Drawing.Drawing2D.LinearGradientBrush (P 0 (20*$u)), (P 0 (88*$u)), (C 255 60 26 54), (C 255 30 12 30)
        $g.FillPath($field, (Shield-Path $u 6))
        # Блик на левой половине поля.
        $g.SetClip((Shield-Path $u 6))
        $g.FillRectangle((New-Object Drawing.SolidBrush (C 26 255 255 255)), [single](18*$u), [single](15*$u), [single](32*$u), [single](80*$u))
        $g.ResetClip()
        $g.FillPath($gold, (Keyhole-Path $u))
        $shadow = New-Object Drawing.Pen (C 160 30 10 10), ([single][Math]::Max(1, 1.2*$u))
        $g.DrawPath($shadow, (Keyhole-Path $u))
    }
    $g.Dispose()

    $ms = New-Object IO.MemoryStream
    $bmp.Save($ms, [Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    , $ms.ToArray()
}

$sizes = 16, 24, 32, 48, 64, 256
$frames = $sizes | ForEach-Object { , (New-Frame $_) }
$out = New-Object IO.MemoryStream
$w = New-Object IO.BinaryWriter $out
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $len = $frames[$i].Length
    $w.Write([byte]($s % 256)); $w.Write([byte]($s % 256)); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([uint16]1); $w.Write([uint16]32); $w.Write([uint32]$len); $w.Write([uint32]$offset)
    $offset += $len
}
foreach ($f in $frames) { $w.Write($f) }
$w.Flush()
[IO.File]::WriteAllBytes((Join-Path $PSScriptRoot '..\gui\app.ico'), $out.ToArray())
$png = Join-Path $PSScriptRoot '..\gui\app-256.png'
[IO.File]::WriteAllBytes($png, $frames[-1])
