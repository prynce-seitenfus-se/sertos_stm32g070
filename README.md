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

On Windows, `build.bat` supports the `Debug`, `Release`, and `Renode` presets
(`Debug` is the default):

```powershell
.\build.bat
.\build.bat Release
.\build.bat Renode
.\build.bat -i
.\build.bat Release -c
.\build.bat Renode -i
.\build.bat -c -i
```

`-c` cleans the selected preset's build outputs before building. `-i` builds
and links the instrumented Cortex-M0+ SerTOS library (instrumentation is
limited to the SerTOS API translation units under `src/`) and enables
`-finstrument-functions` for `app/sertos_demo_queue.c` and the producer,
consumer, and profiler task files; it is off by default. Instrumented builds
also use 1024-byte producer/consumer stacks to accommodate profiler hook stack
usage. The profiler port implementation (`app/profiler_port.c`) remains
uninstrumented to prevent recursive profiler callbacks.
Internal scheduler and dependency helpers are marked
`__attribute__((no_instrument_function))`, keeping the dump focused on
application-used SerTOS APIs.

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

### Capturing a profiler dump over TCP

For a host-accessible profiler connection, start Renode with the USART2 socket
terminal enabled. The Renode preset selects the interrupt-driven, non-DMA UART
backend because Renode's STM32G0 model does not complete the DMA path:

```powershell
cmake --preset Renode
cmake --build --preset Renode
.\renode\run.ps1 -Config Renode -ProfilerSocketPort 3456
```

In a second PowerShell terminal, request and save one complete PROF-BIN v2
packet:

```powershell
.\renode\capture-profiler-dump.ps1 -Port 3456 -OutputFile .\renode-profiler.bin
python ..\profiler\parse_prof_dump.py .\renode-profiler.bin --elf .\build\Renode\sertos_stm32g070.elf
```

The capture script sends `prof-dump`, skips any earlier USART2 text output,
and saves exactly one binary packet. The default run mode continues to use the
UART analyzer without opening a TCP port. The profiler socket terminal is
unauthenticated; use it only on a trusted machine/network.