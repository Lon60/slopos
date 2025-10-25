# Repository Guidelines

## Project Structure & Module Organization
Kernel sources are split by subsystem: `boot/` holds 32→64-bit entry code and early C shims, `mm/` covers paging and allocators, `drivers/` manages serial/APIC/PIC, `sched/` implements cooperative task switching, and `video/` provides framebuffer helpers. Shared headers live beside their modules; include paths are already handled in `meson.build`. Generated artifacts stay in `builddir/`, while `scripts/` contains automation helpers and `third_party/` caches Limine and OVMF assets.

## Build, Test, and Development Commands
Run `git submodule update --init --recursive` after cloning to sync `third_party/limine`. Then execute `meson setup builddir --cross-file=metal.ini` once per checkout (pass `--wipe` if you need to regenerate). Compile the freestanding kernel with `meson compile -C builddir`, which emits `builddir/kernel.elf`. Use `scripts/build_iso.sh` to bundle a bootable `builddir/slop.iso`, fetching Limine binaries on demand. Launch the kernel under QEMU UEFI with `scripts/run_qemu_ovmf.sh builddir/slop.iso 0` only for human-interactive sessions; AI agents should pass a non-zero timeout such as `scripts/run_qemu_ovmf.sh builddir/slop.iso 15`, which auto-exits and streams serial output to `test_output.log` (see `--help` for options).

## Coding Style & Naming Conventions
All kernel C code targets C11, built freestanding with `-Wall -Wextra` and no runtime. Match the existing four-space indentation, brace-on-same-line style, and keep headers self-contained. Prefer static helpers when scope is local and prefix cross-module APIs with their subsystem (e.g., `mm_`, `sched_`). Assembly sources use AT&T syntax (`*.s`) and should document register contracts in comments.

## Testing Guidelines
There are no unit tests yet; rely on QEMU boot verification. Before sending changes, rebuild the ISO and run `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` so the run terminates automatically and logs to `test_output.log`; inspect the log for `SlopOS Kernel Started!` or legacy `KERN` markers plus any warnings. When touching graphics, prefer `scripts/run_qemu_ovmf_video.sh` (with a timeout) to verify the framebuffer visually. Note any observed regressions or warnings in your PR description.

## Interrupt Test Configuration
- Build defaults come from Meson options: set `-Dinterrupt_tests_default=true` to run during boot, adjust suites with `-Dinterrupt_tests_default_suite={all|basic|memory|control}`, verbosity via `-Dinterrupt_tests_default_verbosity={quiet|summary|verbose}`, and timeout with `-Dinterrupt_tests_default_timeout=<ms>` (0 disables the guard).
- Runtime overrides are parsed from the Limine command line: use `itests=on|off|basic|memory|control`, `itests.suite=...`, `itests.verbosity=quiet|summary|verbose`, and `itests.timeout=<ms>` (optional `ms` suffix accepted).
- Boot logs now summarize the active configuration before running tests, and the harness reports total runtime plus whether the timeout fired in `test_output.log`.
- A timeout stops execution between suites; if you need uninterrupted runs, keep it at 0.

## Commit & Pull Request Guidelines
Git history currently lacks structure; standardize on `<area>: <imperative summary>` (e.g., `mm: tighten buddy free path`) and keep subjects ≤72 chars. Add a body when explaining rationale, boot implications, or follow-ups. For PRs, include: brief motivation, testing artifacts (command + result), references to issues, and screenshots or serial excerpts when altering visible output or boot flow. Flag breaking changes and call out coordination needs with downstream scripts.

## Environment & Tooling Tips
First-time developers should run `scripts/setup_ovmf.sh` to download firmware blobs; keep them under `third_party/ovmf/`. The ISO builder auto-downloads Limine, but offline environments should pre-clone `third_party/limine` to avoid network stalls. When adding new modules, update `meson.build` source lists and ensure the linker script `link.ld` maps any new sections intentionally. Remember the active page tables come from `boot/entry32.s`/`entry64.s`; only extend them from C, never reset CR3 mid-boot.

## Safety & Execution Boundaries
Keep all work inside this repository. Do not copy kernel binaries to system paths, do not install or chainload on real hardware, and never run outside QEMU/OVMF. The scripts already sandbox execution; if you need fresh firmware or boot assets, use the provided automation instead of manual installs. Treat Limine, OVMF, and the kernel as development artifacts only and avoid touching `/boot`, `/efi`, or other host-level locations.
