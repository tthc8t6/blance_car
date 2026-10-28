---
name: stm32-toolchain
description: "本机已安装并验证过的 STM32 嵌入式开发工具链，让 Claude 无需用户截图即可直接编译、烧录、调试、读串口数据。任何涉及以下内容时都应使用本 skill：STM32 / STM32F1 / 单片机 / MCU / 嵌入式固件 / 平衡车等嵌入式项目开发；Keil uVision / MDK-ARM / .uvprojx 工程编译、编译报错、链接报错 Undefined symbol；OpenOCD / ST-Link / SWD 烧录下载程序、进调试、读寄存器、下断点、读变量值；STM32CubeMX / .ioc 文件 / 重新生成外设初始化代码；串口调试、UART 数据、COM 口读取、VOFA+ 波形、Firewater / JustFloat 协议；HAL 库 / CMSIS / ARMCC 编译错误排查；新增 .c 文件后需要加入 Keil 工程。串口/COM 口或 ST-Link 探针查不到、连不上、掉线的分诊排查；VOFA+ 波形日志的统计分析与 PID 跟踪质量评估。也适用于用户说『帮我编译一下』『烧录进去看看』『看看板子上这个变量现在是多少』『串口没数据』『COM 口不见了』『ST-Link 连不上』『这个波形不对劲』这类请求，即使没点名工具。"
---

# STM32 工具链（本机已配置并实测通过）

这台机器上已经打通了完整的命令行工具链，**不需要让用户截图 Keil / CubeMX 界面**，也不需要用户手动点 F7 或进调试模式。所有操作都能拿到可解析的文本结果。

## Quick Reference

脚本位于本 skill 目录的 `scripts\` 下。用绝对路径调用最稳妥：

```powershell
$S = "$HOME\.claude\skills\stm32-toolchain\scripts"
& "$S\build.ps1"
```

| 任务 | 脚本 |
|---|---|
| **动硬件前先体检**（探针/串口/残留进程） | `check-hw.ps1` |
| 编译 Keil 工程并读报错 | `build.ps1` |
| 烧录固件到芯片 | `flash.ps1` |
| 读串口数据 | `monitor.ps1` |
| 分析串口日志（逐列均值/标准差，可分组可对比） | `analyze-telemetry.ps1 -Path run.log -GroupBy 0` |
| 读寄存器 / 按变量名读值 | `gdb-read.ps1 -Read 'bat_v'` |
| 新增 .c/.h 加入 Keil 工程 | `add-source.ps1 -Files ..\Core\Src\pid.c` |
| CubeMX 无界面生成代码 | `cubemx-generate.ps1` |
| **遇到任何异常** | 先读 [troubleshooting.md](troubleshooting.md) |

每个脚本都会自检工具路径和硬件连接，失败时给出可操作的下一步，而不是抛出底层错误。

## 碰硬件之前：查不到设备时先分诊，别猜

"查不到 ST-Link / 查不到 COM 口"有三个原因，**排查方向互相矛盾**，猜错就是一路白跑。同一次会话里三种都遇到过。

**先问用户一句，比自己换着法子查快得多：**

> 「你那边设备管理器 / VOFA+ 里看得到吗？」

| 用户看得到？ | 结论 | 动作 |
|---|---|---|
| 看得到，你查不到 | **沙箱过滤**（只影响串口） | 加 `dangerouslyDisableSandbox: true` 重跑 |
| 用户也看不到 | **设备真的不在**（多半是用户拔了） | 让用户插回去 |

两条实测边界，能少问一半：

- **串口**：沙箱下 `GetPortNames()` / `Win32_SerialPort` / `Get-PnpDevice` 全部返回空，`$sp.Open()` 报 `The port 'COM6' does not exist.`——四种方法互相印证，看起来铁定是硬件坏了，其实用户的 VOFA+ 里 COM6 好端端列着。**凡是碰串口的命令一律禁用沙箱。**
- **ST-Link**：沙箱**不**屏蔽它的 PnP 枚举。所以 `VID_0483` 查不到基本就是**真的没插**，别往沙箱上想。

`check-hw.ps1` 会一次性报告三者状态，并利用这个不对称自动给出分诊结论。

## 典型工作流

```powershell
# 1. 改完代码先编译。编译不过就没必要碰硬件
& "$S\build.ps1"

# 2. 新加的 .c 文件如果没进工程，会在链接阶段报 Undefined symbol
& "$S\add-source.ps1" -Files ..\Core\Src\new_module.c
& "$S\build.ps1"

# 3. 烧录（会自动找最新的 .hex，先检查探针在线）
& "$S\flash.ps1"

# 4a. 看运行时数据流
& "$S\monitor.ps1" -Seconds 3

# 4b. 或暂停下来读某个变量的真实值
& "$S\gdb-read.ps1" -Read 'bat_v','target_speed'
```

## 烧录前先想一秒：这个固件会让硬件动起来吗

嵌入式项目常常驱动电机、舵机、加热丝这类**会产生物理动作**的外设。烧录后芯片立即复位运行，如果固件里的主循环会驱动电机，轮子会当场转起来——板子没固定好可能摔下桌子，或者用户正用手拿着。

所以烧录**会驱动执行机构**的固件前，先确认用户知情：板子是否固定/架空、是否希望现在就动起来。用户明确表示只想测传感器/通信时，把驱动部分注释掉再编译烧录，比事后道歉便宜得多。

只读传感器、只发串口数据的固件没有这个顾虑，正常烧就行。

### 烧录前先让电机停下来

除了安全，还有个实际原因：**电机运转时 PWM 开关噪声会干扰 SWD**。本次会话几次 SWD 握手失败（`init mode failed` / `DP initialisation failed`）全都发生在轮子转着的时候，停机后同样的命令一次就过。

### 烧录完成 ≠ 探针可以拔

用户很自然会觉得"烧完了探针就没用了"顺手拔掉，而你后面可能还要 GDB 读变量、或改完再烧一次。本次会话连续踩了两次，每次都要绕好几轮才想到查设备在不在。

**后面还要调试的话，烧录成功时主动说一句"探针先别拔"。**

## 工具路径（本机已验证）

| 工具 | 路径 | 版本 |
|---|---|---|
| Keil uVision 命令行 | `D:\Keil5\Core\UV4\UV4.exe` | ARMCC V5.06 update 7 |
| OpenOCD | `D:\xpack-openocd-0.12.0-7\bin\openocd.exe` | 0.12.0+dev |
| arm-none-eabi-gdb | `D:\ARM\bin\arm-none-eabi-gdb.exe` | GNU ARM 工具链 |
| STM32CubeMX | `D:\CubeMX\STM32CubeMX.exe` | 支持 headless 脚本模式 |
| Zadig（切驱动） | `D:\Zadig\zadig-2.9.exe` | 2.9 |

路径集中定义在 `scripts\_common.ps1` 顶部，**换机器时只改那一处**，六个脚本自动跟着变。

## 底层命令（需要手工微调时参考）

```powershell
# 编译（退出码：0=成功 1=有警告 2=有错误 3=致命错误）
UV4.exe -b project.uvprojx -o build.log -j0

# 烧录 + 校验 + 复位
openocd.exe -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program fw.hex verify reset exit"

# 起 gdb server（端口 3333），再用 gdb 连
openocd.exe -f interface/stlink.cfg -f target/stm32f1x.cfg
arm-none-eabi-gdb.exe -batch -nx -ex "file fw.axf" -ex "target extended-remote localhost:3333" `
                      -ex "monitor reset halt" -ex "print bat_v"

# CubeMX 无界面：脚本内容为 `config load <ioc>` / `project generate` / `exit`
STM32CubeMX.exe -q script.txt

# 读串口（PowerShell 内置，不需要 pyserial）
$sp = New-Object System.IO.Ports.SerialPort 'COM6',115200,'None',8,'One'
```

在 PowerShell 里直接调用这些原生程序时，**不要用 `2>&1`**：PS 5.1 会把每行 stderr 包装成 NativeCommandError，刷屏且把 `$?` 置为 false。OpenOCD 偏偏把几乎所有信息（包括正常的 `Verified OK`）都打到 stderr，所以必须收，但要分开收：

```powershell
$stderr = "$env:TEMP\ocd_err.log"
& $openocd -f "interface/stlink.cfg" -f "target/stm32f1x.cfg" `
           -c "program $hexFwd verify reset exit" 1>"$env:TEMP\ocd_out.log" 2>$stderr
Write-Output "ExitCode: $LASTEXITCODE"
Get-Content $stderr | Select-String -Pattern 'Programming|Verified|Error|Resetting|Target voltage'
```

判断成功看 `** Verified OK **`。`Target voltage` 这行还兼做 SWD 故障分诊的判据（见上面第 2b 条）。

## 必须知道的几件事

这些都是实测踩过的坑，动手前扫一眼能省大量往返。完整版见 [troubleshooting.md](troubleshooting.md)。

1. **Keil 和 OpenOCD 不能同时用一个 ST-Link**。驱动二选一：Keil 要 ST 官方驱动，OpenOCD 要 WinUSB（Zadig 装）。要两边都用就准备两个探针。
2. **`Error: open failed` 不是 OpenOCD 版本问题**，是 USB 打不开。先查探针在不在线，别去升级 OpenOCD。
2b. **`init mode failed` / `DP initialisation failed` 是另一回事**——USB 层是好的，SWD 握手失败。一眼分辨的判据：日志里**有没有 `Info : Target voltage: 3.2xx`**。有这行说明探针打开成功、板子有电，再查 USB/驱动全是白费。先重试一次（经常直接过），再让电机停机。
3. **查 USB 设备必须加 `-PresentOnly`**，否则会查到早已拔掉的幽灵记录（状态 `Unknown`），误判成设备在线。
4. **`设备描述符请求失败` / `VID_0000&PID_0002` 是硬件问题**（多半是纯充电线，或探针坏了），装驱动救不回来。
5. **CubeMX 重新生成会抹掉 `.uvprojx` 里所有手动添加的源文件**，只剩 CubeMX 自己那 7 个。优先改 `.ioc` 文本而不是完整重新生成；真要生成，事后用 `add-source.ps1` 批量加回。
6. **编译错误看第一条，不要看数量**。一个 `#ifndef` 少写 `#` 就能在 CMSIS 头文件里连锁产生 25+ 个幻影错误，修掉根因那一个字符全部消失。
7. **读寄存器前必须先 `halt` 或 `reset halt`**，否则报 `Could not read register 'pc'`。调试完记得 `reset run`，并确保 openocd 进程退出释放探针。
8. **GDB 读到的值要确认可信**。连不上目标板时 GDB 不报错退出，而是从 `.axf` 里读静态初始值继续打印——看起来像真实值其实是假的（`gdb-read.ps1` 已经会拦截这种情况并警告）。**最硬的自查：隔两秒读两次 `uwTick`，值一样就是没连上**（真机上它每毫秒 +1）。同理，芯片上跑的固件必须和你加载符号的 `.axf` 是同一次编译产物，否则地址对不上，读出来是垃圾。
9. **GDB 的路径要用正斜杠**。反斜杠会被 GDB 命令解析器当转义吃掉，`c:\Users\...` 变成 `c:Users...` 然后报 `No such file or directory`。
10. **`.ps1` 必须存成带 BOM 的 UTF-8**。PS 5.1 没 BOM 时按系统 ANSI 码页读脚本，中文注释变乱码并啃掉字符串引号，报 `The string is missing the terminator`——跟脚本逻辑毫无关系。改完脚本查一下：`head -c 3 x.ps1 | xxd -p` 应为 `efbbbf`。
11. **串口打开成功但读到 0 字节 ≠ 固件没在发**。实测遇到过重试一次就收到 10KB 的情况，`monitor.ps1` 已内置自动重试一轮。别在第一次空结果就让用户去查接线。

## 关于串口与 VOFA+

- 板子 UART 需要独立的 USB-TTL 适配器接电脑；**独立 ST-Link V2 探针不带虚拟串口**，不会出现 COM 口
- VOFA+ 用 **Firewater** 协议（纯文本 `%.3f,%.3f,%.3f\n`），Claude 可直接读日志文件；JustFloat 是二进制，需要额外解码
- VOFA+ 能一边实时画波形一边把原始数据录到文件 —— 用户看波形调参，Claude 事后读文件分析，互不抢串口
- 串口同一时间只能一个程序占用，`monitor.ps1` 读完会自动释放。从数据流中间开始读会截到半帧，首行残缺是正常的，别当成数据错误
- **波形"看起来不对"没法验证假设，先跑统计**：`analyze-telemetry.ps1 -Path run.log -GroupBy 0` 给出各设定值下的均值/标准差/极值。判据很有用——**标准差随转速膨胀说明是测量在抖，均值偏离设定值才是控制问题**。定位时序类 bug 时这一步几乎是决定性的
- 调参前后各采一段存成文件，用 `-Compare old.log` 对比，比反复盯实时波形靠谱
