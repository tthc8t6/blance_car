# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

STM32F103C8T6 (ARM Cortex-M3, 72MHz) two-wheel **self-balancing** car. Started as a 2S LiPo
battery monitor, grew into closed-loop wheel-speed control, and since **2026-09-23 it balances
on the ground** using angle + angular-rate loops with inverse dynamics on top of the motor loop;
since **2026-09-29 a velocity loop** holds it roughly in place; since **2026-09-30 a turn loop and
Bluetooth remote control** (ECB02 **BLE** transparent module on USART3) drive it — driven successfully
from Chrome (PC) and Bluefy (iPhone) via the web remote (2026-09-30).
The IMU is an **MPU6500** (not MPU6050 — see the MPU6500 section).

Battery monitoring / LEDs are interrupt-driven. Everything else runs as polled tasks in `main()`.

## Working With This User (learning project)

- This is **learning code** that follows a video course, which **finished on 2026-10-01**. The user
  decides what to add next (velocity-loop windup when lifted, RC timeout, gyro calibration…) and
  prefers to write the key logic themselves — review and fix, don't add features unasked.
- The course author's complete reference firmware is at **`E:\完整源码\bal\`** (`user/` app code,
  `my_lib/` software I2C + `task.h`). It uses StdPeriph + bit-banged I2C and its files are named
  `mpu6050`, but it drives the same MPU6500. When the user asks to "match the teacher", compare
  against it. Differences from the course notes are listed in the self-balancing section.
- The user chose **no WHO_AM_I check** (same as the teacher) — don't re-add it.

## Build / Flash / Debug

Use the **`stm32-toolchain` skill** (Keil CLI build, OpenOCD flash, GDB, serial). Keil project:
`MDK-ARM/blance_car.uvprojx`, output `MDK-ARM/blance_car/blance_car.hex`.
ARMCC v5.06, -O3, stack 0x400, heap 0x200. Unused functions are dropped by the linker, so an
unchanged Code size after editing an uncalled function is normal.

Sources are UTF-8 (no BOM). Own sources (`app_*`, `pid`, `quick_math`, `task.h`, `test/`) are all
**CRLF** since 2026-09-30 (`.editorconfig`); CubeMX-generated files keep their own endings — preserve
whatever a file already uses when patching. Keil build outputs (`MDK-ARM/blance_car/`, `build.log`,
`.uvguix.*`, `.lst`) are git-ignored and untracked.

Comment convention: every function has a header block (`// @简介：` / `// @参数：` / `// @返回值：` /
`// @注意：`, same as `quick_math.c`); keep step comments, unit comments and variable descriptions,
drop anything that just restates the code. Float literals must carry an `f` suffix — an unsuffixed
`0.005` is a `double` and pulls in software double-precision (Cortex-M3 has no FPU; removing them cut
Code by 1.7 KB). File-local variables are `static`, constants `static const`. Function docs live only
in the `.c` files; headers just declare. Tuning constants are named macros at the top of the file that
uses them (`DEG2RAD`, `OMEGA_REF_MAX`, `ENCODER_PPR`, `GEAR_RATIO`, `SAMPLE_PERIOD`, `COMP_ALPHA`,
`RC_SCALE`, `MOVE_SPEED_MAX`, `TURN_SPEED_MAX`; `PI`/`HALF_PI`/`IDX_PER_RAD` in `quick_math.c`).

A clean build has 0 errors, 0 warnings. Flash ~30 KB of 64 KB, RAM ~2.7 KB of 20 KB, so memory is
not a constraint; don't spend effort shaving bytes.

**Repository.** Remote `tthc8t6/blance_car`, single branch **`main`**. A `class_study` branch existed
2026-09-29/30 and was merged into `main` and deleted — don't recreate it; tag a stable version instead.
`AGENTS.md` was deleted on purpose (unused), don't re-add it. Commits end with the
`Co-Authored-By: Claude …` line. Commit source and project files only; Keil outputs are ignored.
`README.md` is the public-facing description (English); keep its parameter tables and roadmap in
sync when loops, gains or known issues change.

Bring-up tests live in `test/` (`encoder_test`, `PWM_test`, `mpu6500_test`, `pid_motor_test`,
`qmath_test`); they are called from `main.c` only when uncommented, and each contains its own
infinite loop (the main `while(1)` never runs while a test is active).

## Architecture

**Battery monitor** (interrupt-driven):

```
TIM2 (100ms TRGO) → ADC1 injected conversion on PB0 (channel 8)
  → HAL_ADCEx_InjectedConvCpltCallback()
    → app_bat.c: scale raw value → clamp to [6.0V, 8.4V] → update LEDs (PA4/PA5/PA6)
```

**Main loop.** Init order matters: `App_Encoder_Init` raises SysTick priority for the µs
timebase; `App_Motor_Init` must precede `App_Control_Init` (which calls `App_Control_Reset`,
which writes the motor setpoints).

```
init:  App_Bat_Start → App_Encoder_Init → App_MPU6500_Init → App_Motor_Init → App_Control_Init → App_RC_Init
while(1):
  App_Button_Process()   — debounce PA11; each press: App_Control_Reset(), App_RC_Clear(), toggle App_Motor_Cmd(on)
  App_MPU6500_Process()  — PERIODIC(5): every 20th tick App_MPU6500_CheckConfig() (re-config if MPU reset)
                            → App_MPU6500_Update() (one 14-byte I2C burst)
                            → complementary filter → yaw/pitch/roll
  App_Control_Process()  — PERIODIC(5): velocity PI → atan → angle P → rate PI → inverse dynamics → ∫ → wheel ω_ref
  App_Motor_Process()    — PERIODIC(1): encoder T-method speed → PI per wheel → duty = U/vbat → PWM
  App_RC_Process()       — if a full line arrived on USART3: parse "move <turn> <speed>" → setpoints
```

No telemetry runs in the main loop right now (USART2 output exists only in test functions).

**Key source files:**

| File | Role |
|------|------|
| `Core/Src/app_control.c` | Velocity loop (PI) + angle loop (P) + rate loop (PI) + inverse dynamics + ω_ref integrator (±40 rad/s clamp) + turn loop (P on gyro Z, adds ±diff to L/R), fall guard (`App_Control_FallGuard`), `App_Control_Reset()`, `App_Control_SetMoveSpeed/SetTurnSpeed` (the only writers of the velocity/turn setpoints) |
| `Core/Src/app_rc.c` | USART3 line receiver (HAL `Receive_IT` 1 byte, re-armed in `RxCpltCallback` and `ErrorCallback`) + `move` command parser; `App_RC_Clear()` empties buffers and zeroes move/turn setpoints |
| `Core/Src/app_mpu6500.c` | MPU6500 over HW I2C1: init, burst read, getters, complementary filter (`App_MPU6500_Process`), config-loss check/restore (`App_MPU6500_CheckConfig`) |
| `Core/Src/app_motor.c` | Per-wheel PI (Kp 0.5, Ki 7), setpoints in rad/s; `App_Motor_Cmd` = STBY + reset of the two motor PIDs |
| `Core/Src/app_encoder.c` | Quadrature decode + T-method speed (EXTI); `App_GetMicroseconds()` µs timebase |
| `Core/Src/pid.c` | Generic PID. Setpoint via `pid_changesp`, **measurement** as the `pid_compute` argument (error is computed inside). Trapezoidal integral stored pre-multiplied by Ki so the output clamp also bounds it. `pid_reset` does **not** clear `sp` |
| `Core/Src/app_pwm.c` | Duty (−100..100) → H-bridge direction pins + TIM1/TIM4 CCR; `App_PWM_Cmd` only drives STBY |
| `Core/Src/app_button.c` | Debounce + toggle; on each press (falling edge only) calls `App_Control_Reset()` and `App_RC_Clear()`; keeps its own `output_state` |
| `Core/Src/app_bat.c` | Battery scaling, LEDs |
| `Core/Inc/task.h` | `PERIODIC(ms)`: `nxt = HAL_GetTick() + T` (teacher uses `nxt += T`; not changed) |

Naming convention is `App_Module_Action` (e.g. `App_Motor_Set_Speed_L`, `App_Get_Bat_Voltage`); the
generic PID module is lower-snake `pid_*` (`pid_init`, `pid_changesp`, `pid_compute`, `pid_limit_config`,
`pid_reset`); tests are `Module_Test` (e.g. `PID_Motor_Test`). Renamed 2026-09-30, so old scripts or notes
that mention `PID_Compute` / `APP_Get_Bat_Voltage` / `Pid_motor_Test` are stale.

## Hardware Pin Map

| Pin | Function |
|-----|----------|
| PB0 | ADC1 channel 8 — battery voltage input |
| PA4 / PA5 / PA6 | Battery charge LEDs (low → high) |
| PA11 | Enable button input (pull-up, active-low) |
| PA1 | TB6612 STBY (1=enabled, 0=standby); 0 at boot |
| PA8 (TIM1_CH1) | Left motor PWM |
| PA9 / PA10 | Left motor AIN1/AIN2 (direction) |
| PB6 (TIM4_CH1) | Right motor PWM |
| PB5 / PB7 | Right motor BIN1/BIN2 (direction) |
| PB14 / PB15 | Left encoder phase A (EXTI) / phase B |
| PB3 / PB4 | Right encoder phase A (EXTI) / phase B |
| PB8 / PB9 | I2C1 (remapped) SCL / SDA — MPU6500 @ 0x68, 400 kHz |
| USART2 | Firewater/VOFA+ telemetry (only used by test functions currently) |
| PB10 / PB11 | USART3 TX / RX — ECB02 BLE module, **9600** baud, RX-only, IRQ priority 0 |

## MPU6500

**Confirmed MPU6500**: the teacher writes register `0x1D` (ACCEL_CONFIG2, which the 6050 does
not have), and the 6500 temperature formula `raw/333.87 + 21` reads a sane 27.6 °C (the 6050
formula would give 43 °C). A 6500's WHO_AM_I is **0x70**; 0x68 is its I2C address and the
6050's WHO_AM_I. Register map PDF: `MPU6500寄存器.pdf` in the project root.

Init config — identical to the teacher, checked against the register map:

| Reg | Value | Meaning |
|-----|-------|---------|
| 0x6B PWR_MGMT_1 | 0x80, wait 100 ms, then 0x01 | reset; CLKSEL=1 (auto / PLL) |
| 0x19 SMPLRT_DIV | 0x00 | ODR 1 kHz — registers refresh every 1 ms, so a 5 ms poll gets ≤1 ms-old data |
| 0x1A CONFIG | 0x02 | gyro DLPF 92 Hz, **3.9 ms** delay (6500 table; 6050 numbers differ) |
| 0x1B GYRO_CONFIG | 0x18 | ±2000 dps (16.4 LSB/°/s), FCHOICE_B=00 |
| 0x1C ACCEL_CONFIG | 0x00 | ±2 g (16384 LSB/g) |
| 0x1D ACCEL_CONFIG2 | 0x02 | accel DLPF 92 Hz (reset value would be 460 Hz) |

If `App_MPU6500_Init()` is skipped, the gyro stays at ±250 dps and every rate reads **8× too large**.

Axes (verified by shaking): pitch ← gyro X, roll ← gyro Y, yaw ← gyro Z. Pitch uses
`atan2(ay, az)` + `gx` (same as teacher); roll's overall sign is opposite to the teacher's.

Measured at rest on this board (2026-09-22):
- Gyro bias gx −0.47, **gy +9.18**, gz +0.73 °/s — stable across a shake test (Δ < 0.03), so a
  one-time calibration would be valid. With the complementary filter (τ = 0.1 s) a constant bias
  b only shifts the angle by b·τ (pitch −0.05°, roll 0.9° — verified). The **rate loop** uses
  raw gx, so the bias enters there unfiltered.
- Noise σ: gy/gz ≈ 0.095 °/s (= datasheet 0.01 °/s/√Hz × √92), **gx 0.25 °/s (2.6×)**. Harmless
  (pitch σ 0.026°). Suspected CLKSEL=1 or mounting; not investigated.
- |accel| = 0.906 g (zero-g offsets within spec). ±2 g clipped once during a hand shake.

Pitfalls learned:
- **Only `App_MPU6500_Process()` may call `App_MPU6500_Update()`**; everything else uses the
  getters. An extra unthrottled `Update()` in a loop doubled gx/gz noise (continuous I2C traffic
  couples into the MEMS) and stretched the 5 ms period.
- Burst-read timeout is 2 ms (a 14-byte read takes 0.39 ms). With 100 ms, the failure path
  (timeout + bus recovery + retry) blocked ~200 ms.

### MPU6500 config loss → high-frequency buzz (found 2026-09-30, fixed 2026-10-01)

**Symptom:** a ~24 Hz buzz (嗡嗡, the body tingles to the touch), PWM swinging 0↔1000, battery
reading sagging. **Cause:** the MPU6500 sat at its power-on defaults (±250 dps, DLPF off) while the
firmware scaled by 16.4 LSB/(°/s), so **every gyro rate read 8× too large** (noise ~14×) and the
turn loop effectively ran at Kp=8. The accelerometer looks normal because ±2 g is also its default.

- **Diagnostic shortcut:** standstill gyro bias ≈ 8× the 09-22 values (gx −0.47, gz +0.73) means
  the MPU has lost its config. The button does not fix it (it doesn't re-init the MPU); RESET does.
- **Misleading side effects:** the battery LEDs drop 3→2 at the same time (7.96 V at rest sits on the
  7.9 V threshold; the buzz current drags it below), and lowering the turn limit changes nothing (a
  limit sets amplitude, it can't stabilise an unstable loop).
- **Root cause — Init bug, fixed in `2fdbf9e`:** `App_MPU6500_Init()` used to return on the first
  failed write (and `main.c` ignored the result). Init runs milliseconds after power-on, while the
  MPU may need up to 100 ms, so a NACK on the first `0x80` dropped the whole config. Init is now
  `void` and writes every register after the 100 ms delay, like the teacher's soft-I2C driver
  (which ignores errors). Every observed occurrence followed a power-on; no buzz since the fix.
- **Backstop — `App_MPU6500_CheckConfig()`**, every 20 ticks (100 ms): reads back `0x1B`; if it is
  not `0x18`, rewrites 0x6B=0x01, 0x19, 0x1A, 0x1C, 0x1B, 0x1D. It skips the 0x80 reset and 100 ms
  delay (so PWM is never frozen at its last duty) and leaves the angles alone (the filter
  re-converges in ~0.1 s). Only `0x1B` is checked; a lone failure elsewhere is accepted.
- **Hardware vs the teacher's schematic** (`平衡车电路图.pdf`, project root): the teacher solders the
  chip on the board; the user's car uses a plug-in **module in a female header**. A runtime supply
  dropout is therefore possible in principle but has never been observed.
- Other driver differences are harmless (one 14-byte burst instead of 14 single reads — better; the
  0x1A/0x1B write order).
- Very likely also the cause of the 09-23 large pitch oscillation (8× gx makes rate Kp 10 act like
  80); those swings are gone since the fix. Circumstantial, not proven.

## MATLAB Simulation Scripts

All MATLAB scripts (`.m`) for this project live in **`blance_car_matlab/`** and nowhere else.
Do not create or look for this project's `.m` files under `blance_car/`, the project root,
or `D:\matlab_project\` — `D:\matlab_project\blance_car\` is a separate, unrelated
scratch area and is **not** part of this project.

| File | Role |
|------|------|
| `blance_car_matlab/Serial_pid_transfer_function.m` | Angular-rate loop closed-loop TF `phi` + dcgain/zero/pole checks. See the self-balancing section below. |

Scripts are run from the MATLAB desktop (F5 / Run). Requires **Control System Toolbox**
(`tf`, `step`, `rlocus`, `margin`, `bode`). Installed MATLAB is **R2025a** at `D:\MATLAB2025`.

Claude can drive this MATLAB instance directly through the `mcp__matlab__*` tools — it
attaches to the user's **live** MATLAB session, so executing code there mutates the user's
real workspace and opens real figure windows. Warn the user before overwriting variables.

## Self-Balancing Control Design

The control design was worked out on paper following a video course; the derivation is
transcribed in full at:

```
D:\倒立摆_参数整定\study_video-notes.md     # full derivation, all 5 loops (see below)
D:\倒立摆_参数整定\images\                  # 1600px copies embedded by the notes (141 used)
D:\倒立摆_参数整定\_parts\                  # per-batch drafts + previous versions (archive)
D:\倒立摆_参数整定\new_images1\ , new_images2\   # raw screenshots (HEIC / PNG) for the 2026-10-25 additions
E:\Sky_Notice\01-Projects\Balance-Car\倒立摆_参数整定\   # user's backup of the finished note + images/
```

The note (finished and user-verified 2026-10-25) has three parts:
- **上篇 M1–M9**: DC motor equations → Laplace / block diagram → `Gm(s)=Kt/((La·s+Ra)(Js+B)+KtKe)`
  → measured La, Ra, Ke=Kt, B, J → Bode → motor PI by pole-zero cancellation at 14.2
  (K=22.4 → "reduce for nonlinearity" → 7, i.e. Kp=0.5, Ki=7).
- **中篇 parts 0–5**: the original 45-screenshot notes (cascade, pendulum model, inverse dynamics,
  rate 10/10, angle 4).
- **下篇 parts 6–10**: output clamps, why the car drifts, `θ_ref = atan(ẍ/g)`, velocity paradox and
  the `θ̇·(lp+rw)` compensation, `Gθ(s)` (ωn 6.93, ζ 0.728), velocity PI 10/1, turn P 1.

Its top checklist (NA-/NB-/NC- items) records the teacher's numeric slips that the user accepted as
rounding (e.g. K is really 5.559 not 5.591; 1962.2 belongs to the old J). One open point: the velocity
PI 10/1 has about −14° phase margin in the linear model, yet the car is stable. Not investigated.
Edits go to the `D:\` copy; the `E:\` copy is the user's backup.

That folder is **outside this repo** and explains why the loops are shaped this way.
The teacher's firmware (`E:\完整源码\bal\user\app_control.c`) covers what the notes don't.

### Loops

| Loop | Feedback | Type | Firmware | Teacher's code | Status |
|------|----------|------|----------|----------------|--------|
| Motor (inner) | wheel ω (encoder) | PI | `Kp=0.5, Ki=7` | — | implemented (`app_motor.c`) |
| Angular rate | θ̇ = gyro X | PI | `Kp=10, Ki=10` (from notes), out ±125.7 | **`Kp=20, Ki=20`**, out ±100 | implemented |
| Angle | θ = pitch | P | `Kp=4`, out ±12.57 | `Kp=4` | implemented |
| Velocity (outermost) | (ωL+ωR)/2·rw + θ̇·(lp+rw) | PI | `Kp=10, Ki=1`, out ±0.5g, `θ_ref = qatan(out/g)`, sp 0 | same | implemented 2026-09-29 |
| Turn | gyro Z (rad/s) | P | `Kp=1`, out ±10 rad/s (remote maps to ±15), L += diff / R −= diff | `Kp=1`, out ±10 | implemented 2026-09-30 |

Body constants: firmware `Rw = 0.03`, `lp = 0.062`; teacher `rw = 0.032`, `lp = 0.062`.
Velocity feedback in firmware: `x_dot = (ω − (−θ̇·(lp+Rw)/Rw))·Rw = ω·Rw + θ̇·(lp+Rw)` — same sign as
the teacher because both firmwares send the same physical wheel command (see inverse dynamics).
It must use **θ̇**, not θ (an early version used θ — dimensionally wrong).

Motor plant (derived and measured in the notes' 上篇 M2–M7): `5.591 / ((s/14.2+1)(s/1962.2+1))`
→ motor closed loop ≈ `1/(s/40+1)`, τ = 0.025 s. Rate-loop closed loop (`phi` in the MATLAB script):

```
phi = 10*(s+1) / (0.025*s^3 + s^2 + 10*s + 10)
```

### Inverse dynamics (feedback linearization)

Between the rate PID and the motor setpoint there is an algebraic inverse of the pendulum plus
an integrator `1/(rw·s)` — not a plain cascade of PIDs. **What the working firmware uses:**

```
ẍ_ref   = (g·sinθ − lp·θ̈_ref) / cosθ          (app_control.c step 6)
ω_ref  += ẍ_ref / rw · Δt,   clamp ±40 rad/s   (step 7)
motor setpoint L = R = +ω_ref
```

The teacher writes `(lp·θ̈ − g·sinθ)/cosθ` and outputs `−ω_ref` — algebraically identical.
This sign combination **balances on the real car (verified 2026-09-23)**; the sign question is
closed. Note that the notes' inverse `(lp·θ̈ + g·sinθ)/cosθ` is *not* what the firmware uses.

### Safety structure (the control loop keeps running in standby)

`App_Control_Process()` runs whether or not STBY is on, so with the wheels stopped its integrator
winds up (measured 4252 rad/s after 35 s, before the clamp existed). Therefore:
- ω_ref is clamped to ±40.
- **Fall guard `App_Control_FallGuard(theta)`** (2026-10-01, written by the user, verified on the
  car):
  - |θ| > 80° sets `fallen = 1`. Every tick it returns 1, and the caller does
    `App_Control_Reset()` + return. The setpoints stay 0 and the motor loop keeps braking the
    wheels.
  - Recovery requires |θ| < `RECOVER_ANGLE` (10°) held **continuously** for `RECOVER_TIME_MS`
    (500 ms). `upright_tick` records the start once (`== 0` means not timing), and the timer is
    cleared whenever |θ| leaves 10°.
  - It replaced the old guard, which resumed the moment |θ| dropped below 80° and caused a
    "second fall" while a person was still lifting the car.
  - **Don't use `HAL_Delay` in the control path for this** (the user's first idea was
    `HAL_Delay(1000)`): it freezes the 1 ms motor loop, so the PWM holds its last, often
    saturated, duty while the car lies on the ground. It also doesn't wait for the human; it
    only delays.
  - Returning 1 whenever |θ| > 80° also keeps cosθ → 0 out of the integrator.
  - `fallen` is **not** cleared by `App_Control_Reset()`, which runs every tick while fallen.
    So after a button press the car still needs the 0.5 s upright hold.
- `App_Control_Reset()` clears all three control PIDs, ω_ref, **the motor setpoints** (because
  `pid_reset` keeps `sp`), and sets `last_time = 0`. `App_Control_Process()` skips the ω_ref
  integration while `last_time == 0` (the user's choice: avoids a wrong Δt after any gap;
  costs one 5 ms step) and ends with `last_time = now`. Reset is called from
  `App_Control_Init()`, the tilt guard, and **on every button press** (enable or disable) inside
  the falling-edge branch of `app_button.c` — not in the "stable > debounce" branch, which is
  true every loop and would wipe the controller continuously.
- The fall guard deliberately does **not** call `App_Motor_Cmd(0)`: that would desync
  `app_button.c`'s `output_state`, so two presses would be needed to re-enable.
- The teacher's `standingUp` state machine (`E:\完整源码\bal\user\app_control.c`) latches off after
  a fall (state 5) and has an auto stand-up (states 1–4: stop 500 ms, full power toward the fall
  side, hand over below 40°). It was his own patch, never taught.

### Pitch oscillation — large 2.1 Hz mode gone; small ~1.4 Hz wobble remains (updated 2026-10-01)

**Status 2026-10-01** (capture `verify21`, after the MPU Init fix and `CheckConfig`; the
standstill bias was gx −0.29 / gz +0.40 °/s, so the MPU config was correct):

| | 09-23 | 10-01 |
|---|---|---|
| Spontaneous swing | ±10–16° at ≈2.1 Hz | **±2–3°** (pitch σ 1.6–1.8°) at **1.3–1.5 Hz**, continuous, not decaying |
| \|gx\| | ~250 °/s | ~50 °/s |
| Recovery after a 6–10° push | swings 2–3 s, big pushes grow | **≈0.3 s** to within 3.5° (a −16.6° push took 2.2 s) |
| ω_ref | hits ±40, loop loses authority | max 10.4 while enabled |

- The large mode is regarded as **resolved**.
- The small ~1.4 Hz, ±3° wobble is **inherent to the car**: the user sees it without the SWD
  wires attached, too.
- Captures always carry some wire disturbance. The Dupont SWD wires are stiff, pull on the body,
  and rubbed a wheel during this run; that is unavoidable while sampling.
- Candidate levers for the wobble if the user wants to pursue it, none applied: rate-loop gain
  (teacher 20/20 vs 10/10), loop delay (DLPF 3.9 ms + 5 ms period + motor response — the modelled
  `phi` has no delay, so check `margin()` with one), and motor static friction or deadband (a
  typical limit-cycle source).
- The 09-23 mode was **not** leftover state and unrelated to button presses (disproven by
  capture). Don't confuse either mode with the 24 Hz buzz (small pitch, MPU config loss).

### Known issue — velocity-loop windup when lifted (parked, 2026-09-29)

Started on the ground the car stays roughly in place. **Lifted and left spinning, then put back,
it runs forward continuously**: in the air the velocity error can't be removed, so the velocity
PI integral (Ki=1) winds up to the ±0.5g output clamp; back on the ground θ_ref ≈ atan(0.5) ≈ 27°
and holding that lean requires constant forward acceleration. `pid.c` bounds the integral only
by the output limit, which prevents overflow but not saturation. **Current usage: press the
button to stop before lifting, press again after setting down** (the press resets everything).
Fix options for later (user's decision): lift detection → reset velocity integral; conditional
integration when the output saturates; a separate, smaller integral limit. The teacher's code
has no lift detection either (only the >80° fall reset). Not verified by capture yet.

### Bluetooth remote control (2026-09-30)

**Module: ECB02 BLE transparent bridge, not HC-05** (advertises as `BT_ECB0D6000206`; service
`FFF0`, write `FFF2`, notify `FFF1`). Bytes written to FFF2 come out of its UART unchanged.
BLE, so an **iPhone can drive it** (classic SPP/HC-05 would not work on iOS). BLE devices do not
appear in iOS Settings → Bluetooth — scan with LightBlue / nRF Connect. Only one central can be
connected at a time (close the PC page / LightBlue before connecting from the phone).

Remote clients:
- Teacher's Android app: `E:\app源码\` (`BalanceRC/` source, `平衡车遥控器app.apk`). Sends
  `move <steerX> <pedalY>\n` every 50 ms, values −100..100 in screen coords (down = +, so
  forward = negative speed). MuMu emulator can't use it (no Bluetooth passthrough).
- **Web remote** (same protocol/rate): source `blance_car/tools/rc_web/index.html`, published copy in
  the separate public repo `tthc8t6/blance_car_rc` → **https://tthc8t6.github.io/blance_car_rc/**.
  Web Bluetooth: PC Chrome/Edge (WASD, space = stop) or iPhone **Bluefy** (Safari has no Web BLE).
  Editing the source does **not** update the phone — copy it into `blance_car_rc` and push.
  Pitfalls: Bluefy needs the **128-bit UUID strings** (`0000fff0-0000-1000-8000-00805f9b34fb`), the
  16-bit `0xfff0` form worked in Chrome but failed in Bluefy with an error whose message is
  `undefined`; Bluefy's bottom bar may treat a typed URL as a Google search (open via a link /
  bookmark instead). Stick → speed mapping is `toMoveArgs()` (linear, like the teacher; dead zone /
  curve left to the user).
- Manual test: LightBlue → FFF2 → write **Hex** (the text box can't send `\n`), e.g.
  `6D6F766520302035300A` = `move 0 50\n`, `6D6F7665203020300A` = `move 0 0\n`.
- The firmware holds the last command until a new one arrives (no RC timeout) — a client that
  disconnects mid-drive leaves the car running that command. A "no command for N ms → stop"
  timeout would fix it; not added yet (user's call).

Command: `move <turn> <speed>\n` (ints, same order as the teacher's parser). Scaling matches the
teacher: velocity sp = `−speed·0.01·0.7` m/s, turn sp = `−turn·0.01·15` rad/s. Turn-loop sign was
checked against the teacher (user `L = ω_ref + diff`, teacher `L = −ω_ref + turn`; both send the
same physical command). Pressing the button zeroes both setpoints, so the car never starts moving
on enable. Suggested first test (wheels in the air): `move 0 50` → both wheels same direction,
`move 50 0` → opposite directions.

HAL pitfalls hit while writing it (the teacher's StdPeriph RXNE-interrupt code does not carry over):
- `HAL_UART_Receive_IT` is a **one-shot reservation**: arm once in init, and re-arm inside
  `HAL_UART_RxCpltCallback`, or only the first byte is ever received.
- On an overrun (ORE) HAL aborts reception and calls `HAL_UART_ErrorCallback` instead — re-arm
  there too, or RX stops permanently.
- Never call blocking `HAL_UART_Receive` inside the callback (and its return value is a status,
  not the byte). USART3 and SysTick are both priority 0, so a timeout inside the ISR can hang.
- The velocity setpoint must not be rewritten every tick (an early version had
  `pid_changesp(&pid_velocity, 0)` in `App_Control_Process`, which silently cancelled RC input).
- Re-arm currently happens at the *start* of the callback, before `rxData` is read; safe at 9600
  baud (~1 ms per byte), move it to the end if the baud rate is raised.

### Known caveats (already investigated — do not re-litigate)

- **Course sign slip.** The course first wrote `θ̈ = (ẍcosθ − g sinθ)/l_p`. The teacher later admitted
  that eq. (1) was missing a minus sign and corrected it to `(g sinθ − ẍcosθ)/l_p` (the green text from
  IMG_4873 on). The old A-1 "convention" explanation was removed from the notes. Doesn't affect gains.
  The firmware sign is settled — see inverse dynamics above.
- **Rounding.** `7 × 5.591 = 39.137` is rounded to 40 for hand calculation (2.2%, negligible).
- **Derivation gaps.** Not justified by the teacher: why K is cut from 22.4 to 7 ("nonlinearity"
  only), why the angle gain is 4, and why the velocity PI zero is at 0.1.

### Working with the screenshot notes

Source images are iPhone photos of a screen; **12 of the 45 carry EXIF orientation 3
(rotated 180°)**. Pillow's plain `Image.open()` does not apply EXIF — always use
`ImageOps.exif_transpose()` before feeding them to a model, or formulas get transcribed
backwards (`a − b` read as `b − a`). `images/` already has this applied.

## On-Hardware Debugging

- **Never halt the CPU (GDB / `gdb-read.ps1`) while the motors are enabled.** TIM1/TIM4 keep
  generating PWM at the last duty during a debug halt → uncontrolled drive for the whole halt.
  Halting is fine in standby (STBY=0).
- **Non-halting live sampling works** (~55 Hz, zero read errors with motors running): OpenOCD
  `init` (no `reset`/`halt`), then Tcl `read_memory <addr> 32 <n>` in a loop while the CPU runs.
  Get addresses with `D:\ARM\bin\arm-none-eabi-nm.exe -S MDK-ARM\blance_car\blance_car.axf`
  (they move between builds). `pid_typedef` is 40 bytes: `sp` @+12, `last_error` @+24,
  `integral_term` @+28. Since `last_error = sp − feedback`, the measurement is recoverable
  (e.g. wheel ω = `pid_motor_l.sp − last_error`). Also useful: GPIOA ODR `0x4001080C` bit1 = STBY,
  TIM1 CCR1 `0x40012C34`, TIM4 CCR1 `0x40000834`. Frames are not atomic — expect one torn frame
  at state transitions. Wrap reads in `catch` so a glitch doesn't abort the capture.
  Minimal script (pass `-c "set N 9000" -c "set OUT <path>"` before `-f script.tcl`):
  ```tcl
  init
  set f [open $OUT w]
  for {set i 0} {$i < $N} {incr i} {
      if {[catch { puts $f "[read_memory 0x20000000 32 33]|[read_memory 0x20000230 32 47]" } e]} { puts $f "ERR $e" }
      sleep 5
  }
  close $f
  exit
  ```
- **Faster capture:** dropping the `sleep 5` gives about 100 frames/s. For a before/after test of
  state that a reset would clear, **never reflash in between**. Capture before and after a manual
  RESET instead, or poke RAM with `mww` while running.
- **`read version failed`** on every run means the ST-Link USB is wedged, typically by an
  interrupted OpenOCD run. Unplugging and replugging the probe's USB fixes it without resetting
  the target.
- **`init mode failed` while `Target voltage: 3.2x V` is shown can simply mean the car's power
  switch is off** — the probe reports ~3.2 V even with the board unpowered. Ask before chasing SWD.
- **Blocking UART telemetry distorts control timing**: `HAL_UART_Transmit` of ~60 bytes at 115200
  (≈5 ms) stretched the 5 ms `PERIODIC` to 5.68 ms (measured via yaw drift ÷ gz bias), while the
  MPU integration assumes exactly 0.005 s. Keep telemetry short or rate-limited.

## STM32CubeMX

The peripheral configuration is managed by `blance_car.ioc`. Re-generating from CubeMX will
overwrite `Core/Src/adc.c`, `gpio.c`, `tim.c`, `i2c.c`, and `main.c` initialization blocks — back
up `app_bat.c`, `app_button.c`, `app_control.c`, `app_encoder.c`, `app_motor.c`, `app_mpu6500.c`,
`app_pwm.c`, `app_rc.c`, `pid.c`, and any custom callbacks first. Regeneration can strip manually
added sources from the `.uvprojx` (it didn't when USART3 was added on 2026-09-30); if it does,
re-add them with the skill's `add-source.ps1`.
