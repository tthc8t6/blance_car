# 排错手册

以下全部是在这台机器上实测踩过的坑,不是理论推测。

## 0. 【最优先】查不到 ST-Link / 串口时的分诊

"设备查不到"至少有三个完全不同的原因,**排查方向互相矛盾**,猜错就是一路白跑。同一次会话里三种都遇到过。

### 先问一句,别急着猜

> **"你那边设备管理器 / VOFA+ 里看得到吗?"**

这一句话就能把三选一劈成两半,比自己换着法子查快得多:

| 用户看得到? | 结论 | 下一步 |
|---|---|---|
| 看得到,你查不到 | **沙箱过滤** | 加 `dangerouslyDisableSandbox: true` 重跑 |
| 用户也看不到 | **设备真的不在**(拔了/线松了/枚举失败) | 让用户插回去,或走第 2、3 条 |

### 原因 A:用户把设备拔了 —— 最容易被忽略,却最常见

**烧录完成 ≠ 探针可以拔**。用户很自然会觉得"烧完了,探针没用了"就顺手拔掉,而你接下来可能还要 GDB 读变量、或改完再烧一次。本次会话连续踩了两次:

- 第一次:烧录成功后用户拔了探针,我却接着去起 gdbserver,得到 `Error: open failed`,又去查驱动、查残留进程,绕了好几轮才想到查设备在不在
- 第二次:再次烧录前 `VID_0483` 又查不到,问了一句才知道又拔了

**预防**:烧录成功后如果后面还要调试,主动说一句"探针先别拔"。
**识别**:`Get-PnpDevice -PresentOnly | Where { $_.InstanceId -match 'VID_0483' }` 返回空 = 设备物理上不在,不是驱动问题、不是沙箱问题,直接让用户插回去。

> 沙箱**不会**屏蔽 ST-Link 的 PnP 枚举(实测沙箱下照样查得到 VID_0483)。
> 所以 **ST-Link 查不到基本就是真的没插**,别往沙箱上想。

### 原因 B:沙箱过滤 —— 只影响串口,症状酷似硬件故障

Claude 的 Bash / PowerShell 工具**默认在沙箱里运行**,沙箱会过滤掉串口设备。实测症状:

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()                        # → 空
Get-PnpDevice -PresentOnly | Where-Object { $_.Class -eq 'Ports' }  # → 空
Get-CimInstance Win32_SerialPort                                    # → 空
$sp.Open()                                                          # → "The port 'COM6' does not exist."
```

四种方法互相印证、全部查不到,任谁都会得出"适配器没插好/驱动没装"的结论。

**但同一时刻用户的 VOFA+ 里 COM6 好端端列着**(显示 `USB 串行设备, Microsoft, FX2348N`)。加上 `dangerouslyDisableSandbox: true` 重跑,COM6 立刻出现,打开正常,读到 10KB 数据。

实测边界(别过度推广):

| 操作 | 沙箱下 |
|---|---|
| 串口枚举 / 打开 / 读取 | ❌ 完全不可见,**必须**禁用沙箱 |
| `Get-PnpDevice` 查 ST-Link(VID_0483) | ✅ 可用 |
| OpenOCD 烧录(libusb 访问探针) | ✅ 可用 |

所以:**碰串口的命令一律禁用沙箱;ST-Link 相关的不用。**

### 原因 C:设备在但枚举/驱动异常

`Get-PnpDevice` 查得到但 `Status` 不是 `OK`,或出现 `VID_0000&PID_0002` / "设备描述符请求失败" → 见第 2、3 条。

### 一条命令跑完整套分诊

```powershell
& "$HOME\.claude\skills\stm32-toolchain\scripts\check-hw.ps1"
```

同时报告探针、串口、残留进程,并在"探针和串口同时为空"时提示这更像沙箱(因为真拔的话通常只拔一个)。

### 代价

两个方向都实测过误判代价:往沙箱猜错时让用户白查了几轮 USB-TTL 适配器和驱动(适配器一直是好的);往硬件猜错时则是探针早被拔了,却一直在查 OpenOCD 和驱动。**先问那一句,比两边都猜要省事。**

## 1. ST-Link 驱动互斥:Keil 和 OpenOCD 不能同时用一个探针

同一个 ST-Link,驱动只能二选一:

| 用途 | 需要的驱动 |
|---|---|
| Keil uVision 调试/下载 | ST 官方驱动(STLinkUSBDriver) |
| OpenOCD / 本 skill 的脚本 | WinUSB(用 Zadig 安装) |

Windows 一个设备只能绑一个驱动,**装了 WinUSB 后 Keil 就认不出这个探针了**,反之亦然。

- 切到 WinUSB:管理员运行 Zadig(`D:\Zadig\zadig-2.9.exe`)→ Options 勾 "List All Devices" → 选中 ST-Link(VID_0483 PID_3748)→ 右侧选 WinUSB → Install/Replace Driver
- 切回 Keil:设备管理器 → 该设备 → 更新驱动程序 → 手动指向 ST-Link 官方驱动重装

**根治办法**:准备两个探针,一个刷 WinUSB 专给 OpenOCD,一个留官方驱动给 Keil。

老款 ST-Link/V2(PID_3748)只有单接口,无法共存;V2-1/V3 配合 ST-LINK Server 才有可能多路复用。

## 2. `Error: open failed` —— 不是 OpenOCD 版本问题

这条错误是 libusb 打开 USB 设备失败,跟 OpenOCD 新旧无关,**升级 OpenOCD 解决不了**。

排查顺序:
1. 探针在不在线?`Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'VID_0483' }`
2. 在线但 Status 不是 OK → 驱动没绑成 WinUSB,用 Zadig 处理
3. 压根查不到 → 见下一条
4. 有没有别的进程占着探针?`Get-Process openocd` / Keil 是否正在调试

## 3. `未知 USB 设备(设备描述符请求失败)` / `VID_0000&PID_0002` —— 硬件问题,不是驱动问题

Windows 连设备描述符都读不到,说明 USB 枚举彻底失败。**这种状态下 Zadig 装 WinUSB 毫无意义**,给一个没枚举成功的设备装驱动救不回来;Keil 同样用不了。

排查顺序(按命中率):
1. **换一根能传数据的 USB 线** —— 很多线是纯充电线,没有数据线芯,这是最常见原因
2. 换 USB 口,直插主机后面板,不要走 hub / 扩展坞
3. 拔掉目标板排线,只单独插探针(排除目标板短路把探针拉垮)
4. 以上都无效 → 探针硬件或固件损坏,换一个

> 本次会话最终确认就是探针本身坏了,换新探针后一切正常。

## 4. 查 USB 设备必须加 `-PresentOnly`

不加的话会查到**早已拔掉的历史遗留记录**,状态显示 `Unknown`,看起来设备还在,实际上根本没插。本次会话曾因此误判过一次方向(以为是驱动没绑,其实设备压根不在线)。

```powershell
# 正确
Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -match 'VID_0483' }
```

## 5. CubeMX 重新生成会抹掉 Keil 工程里手动添加的所有文件

实测:重新生成后 `.uvprojx` 里**只剩下 CubeMX 自己那 7 个文件**:

```
main.c  gpio.c  adc.c  i2c.c  tim.c  usart.c  stm32f1xx_it.c
```

其余手动添加的源文件全部从工程中消失(某项目实测丢了 12 个 `.c`)。**文件本身还在硬盘上**,但要一个个重新加回工程。

- `main.c` 里 `/* USER CODE BEGIN */ ... /* USER CODE END */` 之间的代码**会被保留**
- 但工程的文件列表**不会**被保留

**建议**:优先直接改 `.ioc` 文本 + 手动同步对应初始化代码,而不是走完整重新生成。必须重新生成时,生成前先记录文件列表:

```powershell
Select-String -Path *.uvprojx -Pattern '<FileName>'
```

生成后再把手动文件加回去(直接编辑 `.uvprojx` XML 即可,在对应 `<Group>` 的 `<Files>` 里加 `<File>` 节点,`<FileType>1</FileType>` 是 .c,`5` 是 .h)。

## 6. 看编译错误要看第一条,不要看数量

ARMCC 遇到头文件语法错误会解析崩溃,并**连锁污染后续所有头文件**。

实测案例:`app_motor.h` 第 1 行 `#ifndef` 少写了一个 `#`,导致:
1. 该行被当作 C 代码解析 → 语法错误
2. 解析器状态错乱 → `stm32f103xb.h` 里的 `typedef enum` 跟着解析失败
3. `IRQn_Type` 类型没定义成功
4. `core_cm3.h` 和 `stm32f1xx_hal_cortex.h` 里每个用到 `IRQn_Type` 的函数各报一个错

**错误总数从 4 个涨到 33 个,但根因只是一个字符**。补上 `#` 后 33 个错误一起消失。

所以:先修第一条错误再重新编译,不要被数量吓到,也不要逐条去修那些连锁产生的幻影错误。

## 7. 读寄存器前必须先暂停 CPU

CPU 运行中读寄存器会报 `Error: Could not read register 'pc'`。先 `halt`,或更可靠的 `reset halt`(复位后停在复位向量,状态确定)。

如果 `halt` 报 `target was in unknown state when halt was requested`,说明芯片当前状态异常(可能没有效固件在跑),用 `reset halt` 代替。

调试结束记得 `resume` 或 `reset run` 让板子恢复运行,并确保 OpenOCD 进程退出释放探针。

## 8. VOFA+ 数据格式

| 协议 | 格式 | 适用 |
|---|---|---|
| **Firewater** | 纯文本 ASCII,`1.23,4.56,7.89\n` 逗号分隔+换行 | **推荐**,Claude 可直接读日志文件,无需解码 |
| JustFloat | 二进制 float32 数组 + 帧尾 `00 00 80 7F` | 通道多/频率高时更省带宽,但需要额外解码脚本 |

MCU 侧 Firewater 发送(已实测可用):

```c
char buf[64];
sprintf(buf, "%.3f,%.3f,%.3f\n", v1, v2, v3);
HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), 100);
```

VOFA+ 可以在实时画波形的同时把原始数据录制到文件,Claude 事后读该文件分析即可,不需要抢占串口。

## 9. 串口独占

一个 COM 口同一时间只能被一个程序打开。`monitor.ps1` 读取时若 VOFA+ / 串口助手开着会失败,反之亦然。脚本读完会自动 `Close()` 释放。

另外注意:**独立的 ST-Link V2 探针不带虚拟串口**,不会出现 COM 口;板子的 UART 需要单独的 USB-TTL 适配器接到电脑。

补充:**串口打开成功但读到 0 字节,不等于固件没在发**。实测遇到过第一次采集 0 字节、原样重试一次就收到 10KB 的情况。`monitor.ps1` 现在会自动重试一轮,别在第一次空结果就让用户去查接线。

## 10. SWD 握手失败 ≠ USB 打不开:三类错误要分清

OpenOCD 连不上有两个完全不同的层次,处理方向相反:

| 错误 | 层次 | 方向 |
|---|---|---|
| `Error: open failed` | USB 层,探针都没打开 | 查探针在不在线、驱动、残留进程(第 2 条) |
| `Error: init mode failed (unable to connect to the target)` | SWD 层,探针是好的 | 查 SWD 信号 |
| `Fail reading CTRL/STAT register` + `DP initialisation failed` + `AP write error, reset will not halt` | 同上 | 同上 |

### 一眼分辨的判据

看日志里有没有这一行:

```
Info : Target voltage: 3.227038
```

**有这行 = 探针打开成功、VCC 检测线接好、目标板有电**。此时再去查 USB 线、驱动、Zadig 全是白费——问题在 SWD 数据线或信号质量上。

### 实测原因:电机运转时的 PWM 开关噪声

本次会话几次 SWD 失败**全部发生在轮子转着的时候**,而电机停止时同样的命令一次就过。TB6612 的 PWM 开关噪声会串到 SWD 线上。

处理顺序:

1. **直接重试一次** —— 经常就过了(本次实测:第一次 `DP initialisation failed`,原样重试即成功)
2. 仍失败 → 让用户按按钮停机再烧
3. 还不行 → 查 SWD 排线,插拔 USB 时容易把排线带松

> **建议养成习惯:烧录前先让电机停下来。** 既避免 SWD 干扰,也顺带避免烧录瞬间的意外动作。

注:试过在 `-f interface/stlink.cfg` 后面加 `-c "adapter speed 100"` 降速,但会被后面的 `target/stm32f1x.cfg` 覆盖回 950kHz(日志里能看到 `clock speed 950 kHz`),所以那次成功其实是重试的功劳,不是降速。要真降速得放在 target cfg **之后**。

## 11. GDB 的几个坑

### 路径必须用正斜杠

```powershell
# ❌ 反斜杠被 GDB 的命令解析器当转义吃掉
-ex "file c:\Users\tth\...\fw.axf"
#    → 报错 "c:UserstthDesktop...: No such file or directory"

# ✅
-ex "file c:/Users/tth/.../fw.axf"
```

### 连不上时 GDB 不会退出,而是打印假值

`could not connect (error 138)` 之后,GDB **继续执行后面的命令**,从 `.axf` 的静态初始化数据里读值打印出来。看起来完全像真实读数,实际上和芯片毫无关系。

**识别方法:间隔一段时间读两次同一个变量,值完全一样就是没连上。**

```powershell
# uwTick 是活的毫秒计数,两次读之间隔 2 秒必然变化；
# 若两次都是 0 → 读的是 .axf 静态值,不是芯片
```

本次会话就靠这个识破:两次读 `uwTick` 都返回 `$1 = 0`,而真机上它每毫秒 +1。

### 其他

- openocd gdbserver 起来后要等 **~2 秒**再连,否则 `could not connect`
- 不要在 gdb 的 `-ex` 里混 Unix/Windows 睡眠语法(`shell timeout 1 sleep 1` 在 Windows 上直接报参数错)。要间隔就**分两次 gdb 调用**,中间用 PowerShell `Start-Sleep`,gdbserver 保持运行
- 第一次调用后记得 `monitor resume` 让目标板继续跑,否则它停在断点上,第二次读到的还是同一个值

## 12. .ps1 脚本必须存成**带 BOM** 的 UTF-8

症状:脚本一运行就报语法错,报错行显示乱码。

```
At ...\build.ps1:38 char:33
+ ... put "`n[X] è‡´å‘½é”™è¯¯ï¼ˆå·¥ç¨‹/çŽ¯å¢ƒé—®é¢˜..." }
The string is missing the terminator: '.
```

原因:PowerShell 5.1 在**没有 BOM** 时,按系统 ANSI 码页(本机 GBK)读取 `.ps1`。UTF-8 编码的中文注释被曲解成乱码,其中的字节恰好啃掉了字符串引号,于是解析器报"字符串没有结束符"。

**跟脚本逻辑毫无关系,纯粹是文件编码问题。**

本 skill 的脚本已全部补上 BOM。新增或编辑脚本后检查:

```bash
head -c 3 script.ps1 | xxd -p     # 应输出 efbbbf
```

批量修复(纯字节操作,不重新编码,零风险):

```powershell
$bom = [byte[]](0xEF,0xBB,0xBF)
Get-ChildItem *.ps1 | ForEach-Object {
    $b = [System.IO.File]::ReadAllBytes($_.FullName)
    if ($b.Length -lt 3 -or $b[0] -ne 0xEF -or $b[1] -ne 0xBB -or $b[2] -ne 0xBF) {
        [System.IO.File]::WriteAllBytes($_.FullName, $bom + $b)
    }
}
```

> 顺带一提:如果脚本因此跑不起来又急着干活,可以绕过脚本直接调底层 exe(见 SKILL.md「底层命令」),但正确做法还是把 BOM 补上。

## 13. PS 5.1 调原生 exe 收输出的固定姿势

SKILL.md 里提到"不要用 `2>&1`",这里给出实际可用的写法。OpenOCD 把几乎所有信息(包括正常的 `Verified OK`)都输出到 **stderr**,所以必须收 stderr,但直接 `2>&1` 会被 PS 5.1 包装成 `NativeCommandError` 刷屏并把 `$?` 置假。

```powershell
$stdout = "$env:TEMP\ocd_out.log"
$stderr = "$env:TEMP\ocd_err.log"
& $openocd -f "interface/stlink.cfg" -f "target/stm32f1x.cfg" `
           -c "program $hexFwd verify reset exit" 1>$stdout 2>$stderr
Write-Output "ExitCode: $LASTEXITCODE"
Get-Content $stderr | Select-String -Pattern 'Programming|Verified|Error|Resetting|Target voltage'
```

用 `Select-String` 过滤关键行,既能确认结果,又不会被几十行 Info 淹没。判断成功看 `** Verified OK **`。
