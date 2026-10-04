# sertos_stm32g070

Demo firmware showing [SerTOS](https://github.com/Prynce-Seitenfus) running on an
STM32G070RB (Cortex-M0+, LQFP64) target. The project is STM32CubeMX-generated and
built with CMake/Ninja against the pre-compiled SerTOS Cortex-M0+ static library,
and can run on real hardware or inside the Renode emulator.

## What it does

The demo implements a simple producer/consumer pipeline on top of SerTOS:

- **Producer task** (`app/sertos_task_producer.c`) polls the user button (PC13).
  Each loop it pushes an incrementing sequence number onto a statically allocated
  SerTOS queue (`app/sertos_demo_queue.c`), and debounced button presses toggle a
  paused state that stops new items from being enqueued.
- **Consumer task** (`app/sertos_task_consumer.c`) blocks on the same queue and
  drives the green LED based on the received sequence value.
- **Profiler task** (`app/sertos_task_profiler.c`, backed by the `modules/profiler`
  submodule) captures scheduler/function instrumentation events and streams
  commands/results over USART2 (PA2/PA3, 115200-8-N-1).

## Repository layout

| Path                     | Contents                                                              |
|---------------------------|------------------------------------------------------------------------|
| `Core/`                  | STM32CubeMX-generated HAL/CMSIS startup, `main.c`, and board config     |
| `Drivers/`               | STM32 HAL and CMSIS driver sources pulled in by CubeMX                  |
| `app/`                   | Application tasks: producer, consumer, profiler, and SerTOS port layer |
| `modules/profiler`       | Git submodule with the instrumentation-based profiler module           |
| `cmake/`                 | Toolchain file and STM32CubeMX-generated CMake target                  |
| `renode/`                | Renode platform script and `run.ps1` launcher for emulated runs         |
| `sertos_stm32g070.ioc`   | STM32CubeMX project file (pin/clock configuration)                      |
| `CMakeLists.txt`         | Top-level build: links the app against the SerTOS library and profiler  |

## Prerequisites

- A sibling checkout of the `sertos` repository (expected at `../sertos` relative
  to this project) with the Cortex-M0+ library already built at
  `sertos/lib/arm/libsertos_cortex_m0plus.a`.
- The `modules/profiler` submodule initialized (`git submodule update --init --recursive`).
- `arm-none-eabi-gcc`, CMake 3.22+, and Ninja.
- Optional: [Renode](https://renode.io/) for emulated runs without hardware.

## Building

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Use the `Release` preset for an optimized build. Build output is placed under
`build/<Config>/sertos_stm32g070.elf`.

## Running on hardware

Flash `build/<Config>/sertos_stm32g070.elf` to an STM32G070RB board with your
preferred tool (e.g. STM32CubeProgrammer or OpenOCD), then observe the demo over
USART2 (PA2/PA3, 115200-8-N-1) and the green LED. PC13 toggles the paused state
of the producer task.

## Running in Renode

```powershell
.\renode\run.ps1 -Config Debug
```

This launches Renode with the STM32G0 platform description, opens a UART
analyzer attached to USART2, and loads the built ELF. Pass `-Headless` for a
console-only run, or `-Elf <path>` to point at a different firmware image. See
`renode\run.ps1 -?` for all parameters.