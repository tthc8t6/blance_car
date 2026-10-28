# 用 OpenOCD 通过 ST-Link 烧录固件（program + verify + reset）
# 用法: .\flash.ps1 [-HexFile MDK-ARM\xxx\xxx.hex] [-Target stm32f1x]
param(
    [string]$HexFile = "",
    [string]$Target  = "stm32f1x",
    [string]$Interface = "stlink"
)

. "$PSScriptRoot\_common.ps1"
Assert-Tool $Global:STM32_OPENOCD 'OpenOCD'

# 先确认探针在线，否则 OpenOCD 只会甩一句难懂的 "Error: open failed"
if (-not (Test-StLink)) { exit 1 }

if ($HexFile -eq "") {
    $found = Get-ChildItem -Path . -Filter *.hex -Recurse -Depth 3 -ErrorAction SilentlyContinue |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $found) {
        Write-Output "[X] 找不到 .hex 文件，请先编译或用 -HexFile 指定"
        exit 1
    }
    $HexFile = $found.FullName
    Write-Output "自动选择最新的 hex: $HexFile"
}
if (-not (Test-Path $HexFile)) {
    Write-Output "[X] hex 文件不存在: $HexFile"
    exit 1
}

# OpenOCD 的 program 命令需要正斜杠路径
$hexFwd = (Resolve-Path $HexFile).Path -replace '\\', '/'

Write-Output "烧录: $HexFile"
$out = & $Global:STM32_OPENOCD -f "interface/$Interface.cfg" -f "target/$Target.cfg" `
       -c "program $hexFwd verify reset exit" 2>&1
$code = $LASTEXITCODE

$out | Where-Object { $_ -match 'Programming|Verified|Error|error|flash size|device id|Resetting' } |
    ForEach-Object { Write-Output $_ }

if ($out -match 'Verified OK') {
    Write-Output "`n[OK] 烧录并校验成功，芯片已复位运行"
    exit 0
} else {
    Write-Output "`n[X] 烧录失败"
    if ($out -match 'open failed') {
        Write-Output "    'open failed' 是 USB 打开失败，不是 OpenOCD 版本问题。"
        Write-Output "    多半是驱动没绑成 WinUSB(用 Zadig 处理)，或探针刚掉线。"
    }
    exit 1
}
