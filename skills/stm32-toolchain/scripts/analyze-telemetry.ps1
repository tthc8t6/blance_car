# 分析 Firewater 串口日志：逐列统计，可按设定值分组，可与另一次采集对比
#
# 用法:
#   .\analyze-telemetry.ps1 -Path run.log
#   .\analyze-telemetry.ps1 -Path run.log -GroupBy 0          # 按第0列(设定值)分组
#   .\analyze-telemetry.ps1 -Path new.log -Compare old.log -GroupBy 0
#   .\analyze-telemetry.ps1 -Path run.log -GroupBy 0 -Names 'target','left','right'
#
# 为什么要有这个脚本：
#   波形"看起来不对"是没法验证假设的。把日志跑成 均值/标准差/极值 才能回答
#   "到底是控制器在振荡，还是测量在抖"这类问题——标准差随转速膨胀说明是测量，
#   均值偏离设定值才是控制问题。定位时序类 bug 时这一步几乎是决定性的。

param(
    [Parameter(Mandatory=$true)][string]$Path,
    [string]$Compare = "",
    [int]$GroupBy    = -1,     # 按第几列分组（-1 表示不分组）
    [string[]]$Names = @(),    # 可选列名
    [double]$Max     = 0       # 撕裂帧判定阈值，0=按全局量级自动推断
)

function Read-Frames($file) {
    if (-not (Test-Path $file)) { Write-Output "[X] 找不到文件: $file"; exit 1 }
    # 只收完整帧：从数据流中间开始读，首行往往是残缺的半帧，属正常现象，直接丢弃
    $rows = @()
    foreach ($line in (Get-Content $file)) {
        $t = $line.Trim()
        if ($t -eq '') { continue }
        if ($t -notmatch '^-?\d+(\.\d+)?(,-?\d+(\.\d+)?)+$') { continue }
        $rows += ,@($t -split ',' | ForEach-Object { [double]$_ })
    }
    return $rows
}

function Get-Stat($values) {
    $n = $values.Count
    if ($n -eq 0) { return $null }
    $mean = ($values | Measure-Object -Average).Average
    $sd = if ($n -gt 1) {
        [math]::Sqrt((($values | ForEach-Object { [math]::Pow($_ - $mean, 2) } | Measure-Object -Sum).Sum) / $n)
    } else { 0 }
    return [PSCustomObject]@{
        N    = $n
        Mean = $mean
        SD   = $sd
        Min  = ($values | Measure-Object -Minimum).Minimum
        Max  = ($values | Measure-Object -Maximum).Maximum
    }
}

function Col-Name($i) {
    if ($i -lt $Names.Count) { return $Names[$i] }
    return "col$i"
}

function Report($rows, $label) {
    if ($rows.Count -eq 0) { Write-Output "[X] $label 没有解析出任何完整帧"; return }
    $ncol = ($rows | ForEach-Object { $_.Count } | Group-Object | Sort-Object Count -Descending)[0].Name
    $ncol = [int]$ncol
    $rows = @($rows | Where-Object { $_.Count -eq $ncol })

    # 剔除撕裂帧。
    # 串口是字节流，丢一个换行符就会把两帧拼成一行，比如 "12.417" 和 "0.00" 粘成
    # "12417.00"。这种行列数正常、格式也合法，正则拦不住，但量级离谱——一个混进去
    # 就能把均值从 11.96 拉到 110.45（实测踩过）。
    # 判据用「全局量级」而不是分组内的统计离群：滑行衰减那种合法暂态在组内看就是离群点，
    # 会被误杀；而拼接帧比全局最大值还大一两个数量级，用全局尺度卡才不会伤及正常数据。
    $all = @($rows | ForEach-Object { $_ } | ForEach-Object { [math]::Abs($_) } | Sort-Object)
    $p99 = $all[[math]::Min($all.Count - 1, [int]($all.Count * 0.99))]
    $limit = if ($Max -gt 0) { $Max } else { [math]::Max(50.0 * $p99, 1000.0) }
    $suspect = @($rows | Where-Object { ($_ | ForEach-Object { [math]::Abs($_) } | Measure-Object -Maximum).Maximum -gt $limit })
    if ($suspect.Count -gt 0) {
        $rows = @($rows | Where-Object { ($_ | ForEach-Object { [math]::Abs($_) } | Measure-Object -Maximum).Maximum -le $limit })
    }

    Write-Output ""
    Write-Output "### $label   ($($rows.Count) 帧 x $ncol 列)"
    if ($suspect.Count -gt 0) {
        Write-Output ("[!] 剔除 " + $suspect.Count + " 个撕裂帧（阈值 |v|>" + ("{0:N0}" -f $limit) + "，多半是丢了换行符把两帧粘一起）：")
        $suspect | Select-Object -First 5 | ForEach-Object { Write-Output ("      " + ($_ -join ',')) }
        Write-Output "    若这些其实是有效数据，用 -Max <更大的值> 放宽阈值"
    }

    if ($GroupBy -lt 0 -or $GroupBy -ge $ncol) {
        Write-Output ""
        Write-Output ("{0,-10} {1,-7} {2,-9} {3,-8} {4,-9} {5,-9}" -f '列','N','均值','标准差','最小','最大')
        for ($c = 0; $c -lt $ncol; $c++) {
            $s = Get-Stat @($rows | ForEach-Object { $_[$c] })
            Write-Output ("{0,-10} {1,-7} {2,-9:F3} {3,-8:F3} {4,-9:F3} {5,-9:F3}" -f (Col-Name $c), $s.N, $s.Mean, $s.SD, $s.Min, $s.Max)
        }
        return
    }

    # 按某列分组（典型：第0列是设定值，看每个设定值下的跟踪质量）
    $groups = $rows | Group-Object { $_[$GroupBy] } | Sort-Object { [double]$_.Name }
    $header = ("{0,-9} {1,-6}" -f (Col-Name $GroupBy), 'N')
    for ($c = 0; $c -lt $ncol; $c++) {
        if ($c -eq $GroupBy) { continue }
        $header += ("{0,-32}" -f ((Col-Name $c) + ": 均值/σ/最小/最大"))
    }
    Write-Output ""
    Write-Output $header
    foreach ($g in $groups) {
        $line = ("{0,-9} {1,-6}" -f $g.Name, $g.Count)
        for ($c = 0; $c -lt $ncol; $c++) {
            if ($c -eq $GroupBy) { continue }
            $s = Get-Stat @($g.Group | ForEach-Object { $_[$c] })
            $line += ("{0,-32}" -f ("{0:F2} / {1:F2} / {2:F2} / {3:F2}" -f $s.Mean, $s.SD, $s.Min, $s.Max))
        }
        Write-Output $line
    }

    # 跟踪误差：分组列当作设定值时最有用
    Write-Output ""
    Write-Output "跟踪误差（各列均值 相对 $(Col-Name $GroupBy)）:"
    foreach ($g in $groups) {
        $sp = [double]$g.Name
        if ([math]::Abs($sp) -lt 1e-9) { continue }
        $parts = @()
        for ($c = 0; $c -lt $ncol; $c++) {
            if ($c -eq $GroupBy) { continue }
            $s = Get-Stat @($g.Group | ForEach-Object { $_[$c] })
            $parts += ("{0}: {1,6:F1}%" -f (Col-Name $c), (($s.Mean - $sp) / $sp * 100))
        }
        Write-Output ("  $(Col-Name $GroupBy)=" + ("{0,-8}" -f $g.Name) + ($parts -join '   '))
    }
}

$rowsA = Read-Frames $Path
Report $rowsA (Split-Path $Path -Leaf)

if ($Compare -ne "") {
    $rowsB = Read-Frames $Compare
    Report $rowsB (Split-Path $Compare -Leaf)
    Write-Output ""
    Write-Output "提示：两次采集若均值一致而标准差差别大，多半是测量通道变了（噪声/时基），"
    Write-Output "      若均值本身偏了，才是控制行为变了。"
}
