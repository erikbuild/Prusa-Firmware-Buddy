# indx_head

This is the firmware for the **INDX Head** board, running on the STM32C092
MCU, interfaced over RS485 MODBUS. Responsibilities owned by this firmware:

- Drive the induction coil and regulate nozzle temperature.
- Detect nozzle presence.
- Drive print fan and heatbreak fan.
- Drive status LEDs.
- Sample the accelerometer and loadcell and stream them back to the parent system.
- Measure board/MCU temperature and input voltage.
- Report status, accept configuration, and stream sensor data over MODBUS.

## Runtime architecture

FreeRTOS with three tasks:

- `modbus_task`
  - services RS485 requests from the parent system
  - must not block other MODBUS servers
  - reads are served from cache
  - writes drop new setpoints into shared atomics
  - high-rate sensor data are sent over shared FIFO
- `app_task`
  - the control loop
  - runs the induction heater regulator at ~300 Hz
  - updates fans and LEDs at ~10 Hz
  - owns nozzle-presence debouncing
  - calls `panic` on out-of-range temperatures or stuck thermopile
- `spi_task`
  - handles SPI peripherals: accelerometer and loadcell
  - mostly sleeping, wakes on SPI-related IRQs
  - pulls samples from the peripherals
  - pushes them into queues to be drained by `modbus_task`

## Induction heating

The power stage is an LC tank driven by a MOSFET. A single firing pulse excites
the tank, and the tank rings down through the nozzle - the nozzle is the load.
The firmware's job is to keep firing pulses at the right phase, amplitude, and
cadence so that energy goes into the nozzle and not into the MOSFET or the TVS.

### Why we measure before we heat

Before (and periodically during) heating, the firmware stops the driver, fires
one calibration pulse, and samples the ringdown with the ADC. From that waveform
it extracts two things:

- **Oscillation period** - used to time subsequent firing pulses so the MOSFET
  switches at zero voltage crossing. Getting this wrong burns the MOSFET.
  The timing offsets per power level were found experimentally on a scope;
  don't tune them blindly.
- **Decay ratio** - how fast successive peaks shrink. A nozzle present on the
  coil damps the oscillation strongly; an absent nozzle barely damps it at all.
  The decay is the nozzle-presence signal. A shallow decay also means the tank
  voltage can climb to levels that kill the MOSFET/TVS.

Measurements run frequently when idle (to catch nozzle removal fast)
and sparsely while heating (to avoid losing duty cycle to instrumentation).
A sanity check on the measured period rejects wildly out-of-band or
rapidly-changing intervals.

### Control strategy

The regulator is a three-mode controller selected by distance to target:

- **TURBO** - far below target: fire at full power to heat up quickly.
- **LIMITED** - within a few degrees of target: fire at a reduced cap.
  A small overshoot here is desirable because it drives heat from the
  coil region into the nozzle core.
- **PID** - around/above target: PID output, clamped to the limited
  power cap.

A PID is kept running in all active modes so transfer between modes is
bumpless. Power is ramped in single steps via a timer interrupt rather
than jumping, because large jumps in pulse energy disturb the LC response.

## Safety features

- Every FreeRTOS task runs under a per-subsystem software watchdog; the hardware
  watchdog is only kicked if every registered subsystem has kicked in time.
- `panic` drives the board to a safe state (heater off, fans on),
  latches a fault bit for later inspection and waits for reset.
- The heater refuses to pulse whenever the last ringdown analysis is invalid
  or the nozzle is not detected.

## Debugging

Debugging capabilities are severely limited on the STM32C092 running at 48 MHz.
All MCU pins are used for actual peripherals. You can use the SWD interface
of the MCU, but you need to solder a connector, such as `DS1031-08-2-5P8BS-4-1`.

You need to use ST's fork of OpenOCD until they merge their changes upstream.
```
# openocd -f interface/stlink-dap.cfg -f target/stm32c0x.cfg
```

### Profiling

After OpenOCD starts listening for connections on port 4444, you can obtain
a crude profile with a command like
```
# echo 'profile 10 gmon.out 0x08000000 0x08040000' | nc localhost 4444
```
Rebuild OpenOCD with a larger `MAX_PROFILE_SAMPLE_NUM` if you need more samples.
To read the collected profile, use a command like
```
# gprof build-vscode-buddy/indx_head-build/firmware gmon.out
```

An excerpt from the latest profile is attached as
[idle.nozzle.picked.txt](./idle.nozzle.picked.txt).
When developing, please be mindful of performance.
Aim to keep `prvIdleTask` above 25% so the system tolerates jitter.

### Logging via RTT

The firmware contains limited support for logging via RTT. RTT is disabled in
release builds by default, because logging can negatively impact performance.
See [rtt.hpp](./src/rtt.hpp) for details.

### Logging via MODBUS

If you don't have access to the SWD interface, as a last resort, you can hack
the existing MODBUS interface to add more registers/FIFOs as needed. There is
no built-in support for this, however, because the bandwidth is very limited
and such hacking will most likely impact print quality and reliability.

## Peripheral code

The `.ioc` file is the CubeMX project used to generate low-level code; it is
kept for reference only and most of the generated code was already rewritten.
STM32 HAL is too heavy and the MCU doesn't have much computing power to spare.

## Characteristic times

For your convenience, here are some characteristic times:
```
CPU cycle @ 48 MHz                      21 ns
ISR exit                               250 ns
ringdown sample                        290 ns
ISR entry                              310 ns
SPI byte @ 6 MHz                      1300 ns    1.3 us
32-bit int division                               ~2 us
UART byte @ 230 400 baud, 8N1                     43 us
ringdown capture (256 samples)                    75 us
accelerometer sample period                      625 us
FreeRTOS tick                                   1000 us      1 ms
loadcell sample period                          2700 us    2.7 ms
heater control loop period                      3300 us    3.3 ms
MODBUS transaction                                          ~3 ms
software watchdog expiry                                   100 ms
ringdown analysis period (idle)                            100 ms
ringdown analysis period (heating)                         500 ms
hardware watchdog expiry                                  1000 ms      1 s
TPiS invalid data timeout                                 2000 ms      2 s
```
