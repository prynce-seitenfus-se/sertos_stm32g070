# sewcet — static WCET vs. profiler measurements

Project configuration and results for the static WCET analysis of
`sertos_stm32g070`. The tools, the Cortex-M0+ timing model and the OTAWA
toolchain live in the sewcet workspace (`C:\github\sewcet`, see its README
section 5.5); this folder holds only what is specific to this firmware.

## Run

```powershell
.\sewcet\run_wcet.ps1                     # m0plus model (default from JSON)
.\sewcet\run_wcet.ps1 -Model trivial      # fixed cycles per instruction
.\sewcet\run_wcet.ps1 -Model generic      # OTAWA etime pipeline/memory model
.\sewcet\run_wcet.ps1 -Elf .\build\Release\sertos_stm32g070.elf
```

The wrapper calls `<sewcet>\tools\run_wcet.ps1 -Config .\sewcet\wcet_targets.json`,
with `<sewcet>` = `$env:SEWCET_DIR` or `..\sewcet` next to this repository.
Inside WSL: `bash /mnt/c/github/sewcet/tools/run_wcet.sh sewcet/wcet_targets.json [--model generic]`.

## Files

| File | Purpose |
|---|---|
| `wcet_targets.json` | Target functions, ELF, capture paths, timing model, justified flow facts (loop bounds, indirect call targets), response-time bound specs |
| `run_wcet.ps1` | Wrapper around the sewcet tools for this config |
| `1w.csv` | WCET per function in profiler-CSV columns (generated) |
| `comparison.csv` | Target / Renode / WCET comparison (generated) |
| `work/` | Generated `.ff`, `.osx`, hardware XML, `owcet` logs, built plugin (ignored by Git) |

Timing models and the config schema are documented in the sewcet README;
the M0+ parameters used here are in `timing.m0plus` of `wcet_targets.json`.

## Interpreting the results

Column and status legend: `<sewcet>/tools/README.md`.

- `1w.csv`: `calls` is 1 and `total/min/max/avg` all hold the WCET; a static
  analysis yields one upper bound per function.
- `comparison.csv` checks each WCET against the measured execution maximum
  on the target: `status` is `VIOLATION` when it exceeds `wcet_cycles`, and
  `wcet_over_exec_max` shows how pessimistic the bound is.
  `target_avg_cycles` and `renode_max_cycles` are informational (Renode is not
  cycle-accurate).
- The firmware profiler runs with one shadow stack per task and a time mode
  set by the CMake option `PROFILER_APP_TIME_MODE` (`WALL` by default,
  `ACTIVE` optional). The dump header records the mode (`parse_prof_dump.py`
  prints it). Capture both: `1.csv`/`1r.csv` (WALL) and `1a.csv`/`1ra.csv`
  (ACTIVE, presets built with `-DPROFILER_APP_TIME_MODE=ACTIVE`).
- In `WALL` captures, `*_receive` and `*_delay*` include time blocked in a
  queue wait or delay. These functions are flagged
  `measured_includes_blocking` in `wcet_targets.json` and get two checks:
  - execution: `target_active_max_cycles` (`captures.target_active`, `../1a.csv`) must not exceed `wcet_cycles` (`VIOLATION`);
  - latency: `target_max_cycles` (WALL) must not exceed `bound_cycles`, the
    response-time bound from their `blocking_bound` spec
    (`BOUND_VIOLATION`). `bound_over_target_max` shows its pessimism.

  `OK_EXEC`/`OK_BOUND` mean only one check had data; `BLOCKING` means none.
- Response-time bound:
  `R = blocked_ticks * cpu_clock_hz / tick_hz + R_post`, with
  `R_post = WCET(self) + sum(count * WCET(interference)) + extra_cycles
  + sum((ceil(R_post / period_cycles) + 1) * WCET(isr))` iterated to a fixed
  point. Task bursts (higher-priority jobs, PendSV switches) are listed per
  function with a `reason`; periodic ISRs (`SysTick`, `TIM6`, `TIM1`) are the
  `interference_functions` entries with `period_cycles`. All
  `interference_functions` are analysed by OTAWA and appended to `1w.csv`
  but not compared.
- `--time-mode active` treats `--target`/`--renode` as ACTIVE captures: every
  function gets the execution check and no bound check, e.g.
  `python ..\sewcet\tools\compare_csv.py --config sewcet\wcet_targets.json
  --target 1a.csv --renode 1ra.csv --time-mode active`.
- The script exits with 1 on `VIOLATION`, `BOUND_VIOLATION`, `NO_WCET` or
  `NO_BOUND_WCET`.
- The Debug ELF is instrumented, so profiler hooks (`__cyg_profile_func_*`)
  are analysed too (`instrumentation.include_profiler_hooks`).
- Measured cycles also include interrupt preemption (TIM6 tick, TIM1 profiler
  overflow, USART2); the WCET does not.

## After rebuilding the firmware

Loop offsets and `elf_checksum` are tied to one binary. After a rebuild:

1. Run `mkff $ELF <functions...>` (with `otawa-env.sh` sourced), then
   update `elf_checksum` and the `offset` values in `wcet_targets.json`.
2. Re-check each bound against its `reason`.
3. Re-run `run_wcet.ps1`.
