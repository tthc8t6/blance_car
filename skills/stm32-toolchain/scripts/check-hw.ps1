# 一次性体检：ST-Link 探针 + COM 串口 + 残留 openocd 进程
# 用法: .\check-hw.ps1
#
# 动硬件之前先跑这个，比出错后再回头猜快得多。
#
# ⚠️ 如果本脚本报告"什么都查不到"，先确认调用时禁用了沙箱。
#    Claude 的 Bash/PowerShell 工具默认在沙箱里跑，沙箱会过滤掉串口设备，
#    导致明明插着的 COM 口在这里完全查不到。详见 troubleshooting.md 第 0 条。

. "$PSScriptRoot\_common.ps1"

$foundProbe = $false
$foundPort  = $false

Write-Output "=== ST-Link 探针 ==="
$probe = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
         Where-Object { $_.InstanceId -match 'VID_0483' }
if ($probe) {
    $foundProbe = $true
    foreach ($d in $probe) {
        $mark = if ($d.Status -eq 'OK') { '[OK]' } else { '[!] ' }
        Write-Output ("  $mark " + $d.FriendlyName + "   Status=" + $d.Status)
        Write-Output ("        " + $d.InstanceId)
        if ($d.Status -ne 'OK') {
            Write-Output "        Status 非 OK 通常是驱动没绑成 WinUSB，用 Zadig 处理：$Global:STM32_ZADIG"
        }
    }
    # Class 能反映绑的是哪个驱动：USBDevice 一般是 Zadig 装的 WinUSB
    Write-Output ("  驱动类别: " + (($probe | ForEach-Object { $_.Class }) -join ', ') + "  (USBDevice≈WinUSB/OpenOCD 可用)")
} else {
    Write-Output "  [X] 查不到 VID_0483 设备"
    $bad = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
           Where-Object { $_.InstanceId -match 'VID_0000' -or $_.Status -eq 'Error' }
    if ($bad) {
        Write-Output "      但发现枚举失败的设备（USB 硬件/线材问题，装驱动救不回来）："
        $bad | ForEach-Object { Write-Output ("        " + $_.FriendlyName + "  [" + $_.Status + "]") }
    }
}

Write-Output ""
Write-Output "=== COM 串口 ==="
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($ports.Count -gt 0) {
    $foundPort = $true
    $detail = Get-CimInstance -ClassName Win32_SerialPort -ErrorAction SilentlyContinue
    foreach ($p in $ports) {
        $d = $detail | Where-Object { $_.DeviceID -eq $p }
        if ($d) { Write-Output ("  [OK] $p  -  " + $d.Description) }
        else    { Write-Output ("  [OK] $p") }
    }
} else {
    Write-Output "  [X] 查不到任何 COM 口"
    Write-Output "      板子 UART 需要独立 USB-TTL 适配器；独立 ST-Link V2 不带虚拟串口"
}

Write-Output ""
Write-Output "=== 残留进程（会占着探针/串口）==="
$ocd = Get-Process openocd -ErrorAction SilentlyContinue
if ($ocd) {
    $ocd | ForEach-Object { Write-Output ("  [!] openocd PID=" + $_.Id + " 仍在运行，会占着探针") }
    Write-Output "      清理: Get-Process openocd | Stop-Process -Force"
} else {
    Write-Output "  [OK] 没有残留的 openocd"
}
$hog = Get-Process -ErrorAction SilentlyContinue |
       Where-Object { $_.ProcessName -match 'VOFA|putty|sscom|xcom|Terminal' }
if ($hog) {
    $hog | ForEach-Object { Write-Output ("  [!] " + $_.ProcessName + " 在运行，可能占着 COM 口") }
}

# 分诊：沙箱只挡串口、不挡 ST-Link 的 PnP 枚举，这个不对称正好可以拿来区分
if (-not $foundProbe -or -not $foundPort) {
    Write-Output ""
    Write-Output "=== 分诊 ==="
}
if (-not $foundProbe -and $foundPort) {
    Write-Output "  串口查得到、探针查不到 —— 沙箱不屏蔽探针枚举，所以这基本就是"
    Write-Output "  【探针物理上没插】。烧录完顺手拔掉是最常见的原因，让用户插回去即可。"
    Write-Output "  （后面还要调试的话，记得提醒用户探针先别拔）"
}
if ($foundProbe -and -not $foundPort) {
    Write-Output "  探针查得到、串口查不到 —— 两种可能，本脚本无法自行区分"
    Write-Output "  （脚本看不出自己是不是跑在沙箱里）："
    Write-Output "    a) 沙箱过滤：沙箱只屏蔽串口、不屏蔽探针，正好是这个现象。"
    Write-Output "       → 带 dangerouslyDisableSandbox: true 重跑本脚本"
    Write-Output "    b) 已经禁用沙箱还是查不到 → USB-TTL 适配器是真的没连/线松了，"
    Write-Output "       让用户插回去（注意它和 ST-Link 是两个独立的 USB 设备，"
    Write-Output "       拔掉其中一个不影响另一个）"
}
if (-not $foundProbe -and -not $foundPort) {
    Write-Output "  两者都查不到 —— 无法从这里区分，先问用户一句："
    Write-Output "      「你那边设备管理器 / VOFA+ 里看得到吗？」"
    Write-Output "    看得到 → 沙箱，加 dangerouslyDisableSandbox: true 重跑"
    Write-Output "    看不到 → 设备真的没插/线松了，让用户插回去"
    Write-Output "  这一句比自己换着法子猜快得多。"
}
