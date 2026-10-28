# 直接读串口数据（PowerShell 内置 SerialPort，不需要 pyserial）
# 用法: .\monitor.ps1 [-Port COM6] [-Baud 115200] [-Seconds 3] [-Lines 15]
param(
    [string]$Port    = "",
    [int]$Baud       = 115200,
    [int]$Seconds    = 3,
    [int]$Lines      = 15,
    [string]$SaveTo  = ""      # 可选：把原始数据存到文件
)

if ($Port -eq "") {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 0) {
        Write-Output "[X] 系统里没有可用的 COM 口。先分诊，别直接下硬件故障的结论："
        Write-Output ""
        Write-Output "    问用户一句：「你那边设备管理器 / VOFA+ 里看得到 COM 口吗？」"
        Write-Output "      看得到 → 【沙箱】。Claude 的 Bash/PowerShell 工具默认沙箱运行，"
        Write-Output "               会过滤掉串口设备。带 dangerouslyDisableSandbox: true 重跑。"
        Write-Output "      看不到 → 适配器真的没插/线松了，让用户插回去。"
        Write-Output ""
        Write-Output "    补充：板子 UART 需要独立 USB-TTL 适配器接电脑；"
        Write-Output "    独立的 ST-Link V2 探针不带虚拟串口，不会出现 COM 口。"
        exit 1
    }
    $Port = $ports[0]
    if ($ports.Count -gt 1) { Write-Output ("可用串口: " + ($ports -join ', ') + "，自动选择 " + $Port) }
}

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
try {
    $sp.ReadTimeout = 2000
    $sp.Open()
    Start-Sleep -Milliseconds 300

    # 实测遇到过首次采集 0 字节、原样重试一次就正常的情况，所以空结果自动重试一轮，
    # 避免把一次瞬时抖动误判成"固件没在发"，让人白跑一趟去查接线
    $data = ""
    foreach ($attempt in 1..2) {
        $sb = New-Object System.Text.StringBuilder
        $deadline = (Get-Date).AddSeconds($Seconds)
        while ((Get-Date) -lt $deadline) {
            if ($sp.BytesToRead -gt 0) { [void]$sb.Append($sp.ReadExisting()) }
            Start-Sleep -Milliseconds 50
        }
        $data = $sb.ToString()
        if ($data.Length -gt 0) { break }
        if ($attempt -eq 1) { Write-Output "[!] 第1轮 0 字节，自动重试一轮..." }
    }

    Write-Output ("$Port @ $Baud  —  ${Seconds}s 内收到 " + $data.Length + " 字节")
    if ($data.Length -eq 0) {
        Write-Output "[!] 两轮都没收到数据。按这个顺序查："
        Write-Output "    1. 固件到底在不在跑？烧录后是否真的复位运行了（openocd 的 reset 有时没让它跑起来）"
        Write-Output "    2. 波特率是否匹配（本项目 115200）"
        Write-Output "    3. TX/RX 是否接反：适配器 TX→板子 RX，适配器 RX→板子 TX"
        Write-Output "    4. 共地是否接好（只接 TX/RX 不接 GND 是最常见的疏漏）"
        Write-Output "    5. 发送代码是否被条件挡住了（比如放在某个按键/使能分支里才执行）"
    } else {
        Write-Output "--- 前 $Lines 行 ---"
        ($data -split "[`r`n]+") | Where-Object { $_ -ne '' } | Select-Object -First $Lines
    }
    if ($SaveTo -ne "") {
        Set-Content -Path $SaveTo -Value $data -Encoding utf8
        Write-Output "原始数据已保存: $SaveTo"
    }
} catch {
    Write-Output ("[X] 打开串口失败: " + $_.Exception.Message)
    Write-Output "    串口同一时间只能被一个程序占用——VOFA+/串口助手开着的话要先关掉。"
    exit 1
} finally {
    # 必须关闭，否则 VOFA+ 等工具打不开这个口
    if ($sp.IsOpen) { $sp.Close() }
}
