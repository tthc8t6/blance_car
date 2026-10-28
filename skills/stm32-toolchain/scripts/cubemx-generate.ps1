# STM32CubeMX 无界面(headless)生成代码
# 用法: .\cubemx-generate.ps1 -Ioc xxx.ioc [-Force]
#
# 警告：CubeMX 重新生成会重写工程文件，且会抹掉 Keil .uvprojx 里所有手动添加的源文件。
# 因此默认拒绝在有未提交改动的 git 仓库里运行。
param(
    [string]$Ioc = "",
    [switch]$Force,
    [int]$TimeoutSec = 300
)

. "$PSScriptRoot\_common.ps1"
Assert-Tool $Global:STM32_CUBEMX 'STM32CubeMX'

if ($Ioc -eq "") {
    $found = Get-ChildItem -Path . -Filter *.ioc -Depth 2 -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $found) { Write-Output "[X] 找不到 .ioc 文件，请用 -Ioc 指定"; exit 1 }
    $Ioc = $found.FullName
}
if (-not (Test-Path $Ioc)) { Write-Output "[X] .ioc 不存在: $Ioc"; exit 1 }
$Ioc = (Resolve-Path $Ioc).Path

# 安全闸门：有未提交改动时不允许直接生成
if (-not $Force) {
    $iocDir = Split-Path $Ioc -Parent
    Push-Location $iocDir
    $dirty = git status --porcelain 2>$null
    Pop-Location
    if ($LASTEXITCODE -eq 0 -and $dirty) {
        Write-Output "[X] 该目录有未提交的 git 改动，已阻止生成（CubeMX 会覆盖文件）。"
        Write-Output "    先提交或 stash，或确认无误后加 -Force。"
        Write-Output ""
        Write-Output "    另外务必知道：重新生成后 .uvprojx 里只会剩下 CubeMX 自己的文件"
        Write-Output "    (main.c gpio.c adc.c i2c.c tim.c usart.c stm32f1xx_it.c)，"
        Write-Output "    所有手动添加的源文件都会从工程中消失（文件本身还在硬盘上，需要重新加回工程）。"
        Write-Output "    生成前建议先记录当前工程文件列表："
        Write-Output "      Select-String -Path *.uvprojx -Pattern '<FileName>' "
        exit 1
    }
}

$script = Join-Path $env:TEMP ("cubemx_" + [guid]::NewGuid().ToString('N') + ".txt")
Set-Content -Path $script -Encoding utf8 -Value @(
    "config load $Ioc",
    "project generate",
    "exit"
)

Write-Output "CubeMX 无界面生成中（约需 30 秒）..."
$p = Start-Process -FilePath $Global:STM32_CUBEMX -ArgumentList "-q", $script `
     -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\cubemx_out.log"
if (-not $p.WaitForExit($TimeoutSec * 1000)) {
    Stop-Process -Id $p.Id -Force
    Write-Output "[X] 超时 ${TimeoutSec}s，已终止"
    Remove-Item $script -ErrorAction SilentlyContinue
    exit 1
}

$log = Get-Content "$env:TEMP\cubemx_out.log" -ErrorAction SilentlyContinue
$generated = ($log | Where-Object { $_ -match 'Generated code:' }).Count
Remove-Item $script -ErrorAction SilentlyContinue

if ($p.ExitCode -eq 0) {
    Write-Output "[OK] 生成完成，共生成 $generated 个代码文件"
    Write-Output "     记得检查 .uvprojx 是否丢失了手动添加的源文件"
} else {
    Write-Output ("[X] CubeMX 退出码 " + $p.ExitCode)
    $log | Select-Object -Last 15 | ForEach-Object { Write-Output $_ }
    exit 1
}
