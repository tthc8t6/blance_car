# 本机 STM32 工具链路径 —— 换机器时只需改这一处
$Global:STM32_KEIL_UV4 = 'D:\Keil5\Core\UV4\UV4.exe'
$Global:STM32_OPENOCD  = 'D:\xpack-openocd-0.12.0-7\bin\openocd.exe'
$Global:STM32_GDB      = 'D:\ARM\bin\arm-none-eabi-gdb.exe'
$Global:STM32_CUBEMX   = 'D:\CubeMX\STM32CubeMX.exe'
$Global:STM32_ZADIG    = 'D:\Zadig\zadig-2.9.exe'

function Assert-Tool {
    param([string]$Path, [string]$Name)
    if (-not (Test-Path $Path)) {
        Write-Output "[X] 找不到 $Name : $Path"
        Write-Output "    路径可能因机器而异，请更新 scripts\_common.ps1 顶部的变量。"
        Write-Output "    Keil 可查注册表：Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*' | Where-Object { `$_.DisplayName -like '*Keil*' }"
        Write-Output "    其余工具通常在 C:\ 或 D:\ 根目录浅层，可用 Get-ChildItem -Depth 2 搜索。"
        exit 1
    }
}

# ST-Link 是否真的在线。必须用 -PresentOnly，否则会查到早已拔掉的幽灵记录。
function Test-StLink {
    $dev = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
           Where-Object { $_.InstanceId -match 'VID_0483' }
    if (-not $dev) {
        # 顺带看看是不是枚举失败的未知设备
        $bad = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
               Where-Object { $_.InstanceId -match 'VID_0000' -or $_.Status -eq 'Error' }
        Write-Output "[X] ST-Link 不在线（系统里找不到 VID_0483 设备）"
        if ($bad) {
            Write-Output "    但发现枚举失败的 USB 设备："
            $bad | ForEach-Object { Write-Output ("      " + $_.FriendlyName + "  [" + $_.Status + "]") }
            Write-Output "    这是 USB 枚举失败（线材/探针硬件问题），不是驱动问题，装 WinUSB 救不回来。"
        }
        Write-Output "    排查顺序：换数据线(很多线只供电不传数据) → 换USB口直插主机不走hub → 拔掉目标板单独测探针 → 换探针"
        return $false
    }
    if ($dev.Status -ne 'OK') {
        Write-Output ("[!] ST-Link 在线但状态异常：" + $dev.Status)
        Write-Output "    若状态为 Unknown，通常是驱动没绑定成 WinUSB，用 Zadig 处理：$Global:STM32_ZADIG"
        return $false
    }
    return $true
}

# 自动定位 .uvprojx：当前目录 → MDK-ARM\ → 递归两层
function Find-UvProj {
    param([string]$Hint = "")
    if ($Hint -ne "") {
        if (Test-Path $Hint) { return (Resolve-Path $Hint).Path }
        Write-Output "[X] 指定的工程文件不存在：$Hint"
        exit 1
    }
    $found = Get-ChildItem -Path . -Filter *.uvprojx -Depth 2 -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $found) {
        Write-Output "[X] 当前目录下两层内找不到 .uvprojx，请用 -Project 显式指定"
        exit 1
    }
    return $found.FullName
}
