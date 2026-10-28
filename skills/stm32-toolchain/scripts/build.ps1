# Keil uVision 命令行编译，输出精简后的错误/警告
# 用法: .\build.ps1 [-Project MDK-ARM\xxx.uvprojx] [-Full]
param(
    [string]$Project = "",
    [switch]$Full          # 打印完整日志而非只打印错误行
)

. "$PSScriptRoot\_common.ps1"
Assert-Tool $Global:STM32_KEIL_UV4 'Keil UV4.exe'

$proj = Find-UvProj $Project
$projDir = Split-Path $proj -Parent
$log = Join-Path $projDir 'build.log'

Write-Output "编译: $proj"
& $Global:STM32_KEIL_UV4 -b $proj -o $log -j0 | Out-Null
$code = $LASTEXITCODE

if (-not (Test-Path $log)) {
    Write-Output "[X] 编译未产生日志文件，UV4 退出码 $code"
    exit 1
}

$lines = Get-Content $log
if ($Full) {
    $lines | ForEach-Object { Write-Output $_ }
} else {
    # 只保留错误、警告和汇总行；错误常有连锁，第一条才是根因
    $lines | Where-Object { $_ -match 'error|warning|Error\(s\)|Target not created|Program Size' } |
        ForEach-Object { Write-Output $_ }
}

# UV4 退出码: 0=成功 1=有警告 2=有错误 3=致命错误
switch ($code) {
    0 { Write-Output "`n[OK] 编译成功，无错误无警告" }
    1 { Write-Output "`n[!] 编译成功，但有警告" }
    2 { Write-Output "`n[X] 编译失败（有错误）—— 注意：头文件语法错误会连锁污染后续所有头文件，先看第一条错误，不要看数量" }
    3 { Write-Output "`n[X] 致命错误（工程/环境问题，非代码问题）" }
    default { Write-Output "`n[?] UV4 退出码 $code" }
}
exit $code
