# Repository Guidelines

## Project Structure & Module Organization
Kernel sources are split by subsystem: `boot/` holds 32→64-bit entry code and early C shims, `mm/` covers paging and allocators, `drivers/` manages serial/APIC/PIC, `sched/` implements cooperative task switching, and `video/` provides framebuffer helpers. Shared headers live beside their modules; include paths are already handled in `meson.build`. Generated artifacts stay in `builddir/`, while `scripts/` contains automation helpers and `third_party/` caches Limine and OVMF assets.

## Build, Test, and Development Commands
Run `git submodule update --init --recursive` after cloning to sync `third_party/limine`. A convenience `Makefile` now wraps the Meson workflow: run `make setup` once per checkout, `make build` to compile (emits `builddir/kernel.elf`), and `make iso` to regenerate `builddir/slop.iso`. For quick launches use `make boot` (interactive) or `make boot-log` (non-interactive, default 15 s timeout). Both boot targets rebuild a secondary image (`builddir/slop-notests.iso`) with `itests=off` on the kernel command line so you get a clean boot without the harness; override the command line with `BOOT_CMDLINE=... make boot` if you want something different, and add `VIDEO=1` to either boot target if you want a graphical window instead of `-display none`. CI and AI agents can call `make test`, which generates `builddir/slop-tests.iso` with `itests=on itests.shutdown=on`, runs QEMU with `isa-debug-exit` enabled, and fails the target if the interrupt harness reports anything but a clean pass. If you prefer the raw Meson commands, you can still invoke them directly (e.g., `meson compile -C builddir`).

## Coding Style & Naming Conventions
All kernel C code targets C11, built freestanding with `-Wall -Wextra` and no runtime. Match the existing four-space indentation, brace-on-same-line style, and keep headers self-contained. Prefer static helpers when scope is local and prefix cross-module APIs with their subsystem (e.g., `mm_`, `sched_`). Assembly sources use AT&T syntax (`*.s`) and should document register contracts in comments.

## Testing Guidelines
There are no unit tests yet; rely on QEMU boot verification. Before sending changes, rebuild the ISO and run `make test` (non-interactive, auto-shutdown). For manual inspection use `make boot` (interactive) or `make boot-log` to capture a serial transcript in `test_output.log` (append `VIDEO=1` if you need a visible framebuffer). Inspect the output for `SlopOS Kernel Started!` or legacy `KERN` markers plus any warnings. Note any observed regressions or warnings in your PR description.

## Interrupt Test Configuration
- Build defaults come from Meson options: set `-Dinterrupt_tests_default=true` to run during boot, adjust suites with `-Dinterrupt_tests_default_suite={all|basic|memory|control}`, verbosity via `-Dinterrupt_tests_default_verbosity={quiet|summary|verbose}`, and timeout with `-Dinterrupt_tests_default_timeout=<ms>` (0 disables the guard).
- Runtime overrides are parsed from the Limine command line: use `itests=on|off|basic|memory|control`, `itests.suite=...`, `itests.verbosity=quiet|summary|verbose`, and `itests.timeout=<ms>` (optional `ms` suffix accepted).
- Toggle automatic shutdown after the harness with `itests.shutdown=on|off` (or the Meson option `-Dinterrupt_tests_default_shutdown=true`); when enabled the kernel writes to QEMU’s debug-exit port after printing the summary so the VM terminates without intervention.
- Boot logs now summarize the active configuration before running tests, and the harness reports total runtime plus whether the timeout fired in `test_output.log`.
- A timeout stops execution between suites; if you need uninterrupted runs, keep it at 0.

## Interrupt Test Harness
- The harness is enabled at build time via Meson (`meson configure -Dinterrupt_tests_default=true`); override at boot with `itests=on|off` on the Limine command line.
- When enabled, the kernel runs 13 exception and memory access probes and prints a summary banner (`Total tests`, `Passed`, `Failed`, `Exceptions caught`, `Timeout`) to both the serial console and `test_output.log`.
- Suites can be limited to `basic`, `memory`, or `control` through the `interrupt_tests_default_suite` Meson option or `itests.suite=` runtime flag.
- Use `interrupt_tests_default_verbosity=summary` (or `itests.verbosity=`) for concise boot logs; `verbose` traces each case and its handler RIP for debugging.
- Enable `itests.shutdown=on` in automation to halt/QEMU-exit once the summary banner is printed—`make test` wires this in automatically.

## Commit & Pull Request Guidelines
Git history currently lacks structure; standardize on `<area>: <imperative summary>` (e.g., `mm: tighten buddy free path`) and keep subjects ≤72 chars. Add a body when explaining rationale, boot implications, or follow-ups. For PRs, include: brief motivation, testing artifacts (command + result), references to issues, and screenshots or serial excerpts when altering visible output or boot flow. Flag breaking changes and call out coordination needs with downstream scripts.

## Environment & Tooling Tips
First-time developers should run `scripts/setup_ovmf.sh` to download firmware blobs; keep them under `third_party/ovmf/`. The ISO builder auto-downloads Limine, but offline environments should pre-clone `third_party/limine` to avoid network stalls. When adding new modules, update `meson.build` source lists and ensure the linker script `link.ld` maps any new sections intentionally. Remember the active page tables come from `boot/entry32.s`/`entry64.s`; only extend them from C, never reset CR3 mid-boot.

## Safety & Execution Boundaries
Keep all work inside this repository. Do not copy kernel binaries to system paths, do not install or chainload on real hardware, and never run outside QEMU/OVMF. The scripts already sandbox execution; if you need fresh firmware or boot assets, use the provided automation instead of manual installs. Treat Limine, OVMF, and the kernel as development artifacts only and avoid touching `/boot`, `/efi`, or other host-level locations.
