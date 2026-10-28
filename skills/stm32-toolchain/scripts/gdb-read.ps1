# 用 OpenOCD + GDB 读寄存器/变量/内存（自动起停 gdb server）
# 用法:
#   .\gdb-read.ps1                                   # 默认读 pc/sp 和当前源码位置
#   .\gdb-read.ps1 -Read 'bat_v','pid_motor_l'       # 按变量名读值
#   .\gdb-read.ps1 -Commands 'info locals','bt'      # 任意 gdb 命令
#   .\gdb-read.ps1 -NoHalt                           # 不复位暂停（一般不要用，见下）
param(
    [string]$Axf        = "",
    [string[]]$Read     = @(),
    [string[]]$Commands = @(),
    [string]$Target     = "stm32f1x",
    [string]$Interface  = "stlink",
    [switch]$NoHalt
)

. "$PSScriptRoot\_common.ps1"
Assert-Tool $Global:STM32_OPENOCD 'OpenOCD'
Assert-Tool $Global:STM32_GDB     'arm-none-eabi-gdb'
if (-not (Test-StLink)) { exit 1 }

if ($Axf -eq "") {
    $found = Get-ChildItem -Path . -Include *.axf,*.elf -Recurse -Depth 3 -ErrorAction SilentlyContinue |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $found) {
        Write-Output "[X] 找不到 .axf/.elf（GDB 需要它来解析符号），请用 -Axf 指定"
        exit 1
    }
    $Axf = $found.FullName
}
Write-Output "符号文件: $Axf"

# 后台起 OpenOCD 作为 gdb server
$ocd = Start-Process -FilePath $Global:STM32_OPENOCD `
       -ArgumentList "-f","interface/$Interface.cfg","-f","target/$Target.cfg" `
       -PassThru -WindowStyle Hidden -RedirectStandardError "$env:TEMP\ocd_err.log"
Start-Sleep -Seconds 2

try {
    $axfFwd = (Resolve-Path $Axf).Path -replace '\\', '/'
    $gdbArgs = @('-batch','-nx','-ex',"file $axfFwd",'-ex','target extended-remote localhost:3333')

    # 读寄存器/变量前必须先暂停 CPU，否则报 "Could not read register 'pc'"
    if (-not $NoHalt) { $gdbArgs += @('-ex','monitor reset halt') }

    if ($Read.Count -eq 0 -and $Commands.Count -eq 0) {
        $gdbArgs += @('-ex','info registers pc sp','-ex','info line *$pc')
    }
    foreach ($sym in $Read) {
        $gdbArgs += @('-ex',"print $sym",'-ex',"info address $sym")
    }
    foreach ($cmd in $Commands) { $gdbArgs += @('-ex',$cmd) }

    # 不能用 `2>&1`：PowerShell 5.1 会把原生程序的每行 stderr 包装成 NativeCommandError，
    # 刷屏且会把 $? 置为 false。改用文件重定向，stdout/stderr 分开收再合并。
    $gdbOut = Join-Path $env:TEMP 'gdb_out.log'
    $gdbErr = Join-Path $env:TEMP 'gdb_err.log'
    # Start-Process 的 -ArgumentList 传数组时只是用空格拼接、不做引号保护，
    # 含空格的参数（如 -ex "target extended-remote ..."）会被拆散，必须自己加引号。
    $argString = ($gdbArgs | ForEach-Object {
        if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
    }) -join ' '
    $g = Start-Process -FilePath $Global:STM32_GDB -ArgumentList $argString `
         -PassThru -NoNewWindow -Wait `
         -RedirectStandardOutput $gdbOut -RedirectStandardError $gdbErr
    $out = @()
    $out += Get-Content $gdbOut -ErrorAction SilentlyContinue
    $out += Get-Content $gdbErr -ErrorAction SilentlyContinue

    # 关键：连不上目标板时 GDB 不会报错退出，而是从 .axf 文件里读静态初始值继续打印，
    # 看起来像成功读到了芯片上的实时值，实际是误导。必须显式识别并拦截。
    $connFailed = $out | Where-Object { $_ -match 'could not connect|Connection timed out|not supported by this target|Remote communication error' }
    if ($connFailed) {
        Write-Output "[X] GDB 未能连接到目标板，以下数值不可信（那是 .axf 里的静态初始值，不是芯片上的实时值）"
        $connFailed | ForEach-Object { Write-Output ("    " + $_) }
        Write-Output ""
        Write-Output "    OpenOCD 启动日志："
        Get-Content "$env:TEMP\ocd_err.log" -ErrorAction SilentlyContinue |
            Where-Object { $_ -match 'Error|error|Info : STLINK|Info : Target voltage' } |
            ForEach-Object { Write-Output ("    " + $_) }
        Write-Output ""
        Write-Output "    若日志显示 'open failed'：设备状态 OK 不代表绑的是 WinUSB 驱动。"
        Write-Output "    换过 USB 口会让 Zadig 的绑定失效，插回原来那个口，或用 Zadig 重新绑定 WinUSB。"
        exit 1
    }

    # 过滤掉 Keil 生成 ELF 时那条无害的 scatter-loading 提示
    $out | Where-Object { $_ -notmatch 'RW_IRAM1|^\s*in .*\.axf$|re-reading symbols' } |
        ForEach-Object { Write-Output $_ }
} finally {
    if ($ocd -and -not $ocd.HasExited) { Stop-Process -Id $ocd.Id -Force }
    # 确保释放探针，否则 Keil/后续 OpenOCD 都连不上
    Get-Process openocd -ErrorAction SilentlyContinue | Stop-Process -Force
}
