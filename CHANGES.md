# Changelog

## 2026-07-06

### Fixed 'Bad file descriptor' crash on init failure

- **`cfan.c` `c_init()`**: Moved `c_fan_fds[i] = -1` initialization before temp sensor open loop. If a temp sensor failed to open, `c_cleanup()` was called while `c_fan_fds` was still zero-initialized (static storage) — the `== -1` guard didn't catch fd 0 (stdin), causing `pwrite(0, ...)` → EBADF.
- **`cfan.c` `c_init()`**: Fixed temp sensor retry loop — `DIE_GRACEFUL()` was called on the first failure (line 313 inside the `if (open fails)` block), making the 5-retry loop a no-op. Restructured to `for (;;)` with `break` on success and `DIE_GRACEFUL()` only after all retries exhausted.
- **`test.c`**: Added 5 tests (`test_fd_guard_zero_init`, `test_fd_guard_minus_one_init`, `test_fd_guard_mixed`, `test_fd_guard_all_valid`, `test_retry_loop_multiple_attempts`) covering the cleanup fd guard and retry loop patterns.

### Fixed O_CREAT missing mode argument

- **`cfan.c` `c_puts_len()`**: Added `mode_t mode` parameter to fix `O_CREAT` calls missing the required mode argument in `open()`. Without it, the created files had undefined permissions (e.g. setuid bit set, causing green `u` icon in lf file manager).
- **`cfan.c` `c_mode_setup()`**: Pass proper mode (`0600` for lock, `0644` for curve) to the updated `c_puts_len()` calls.
- **`cfan.c` `c_temp_cpu_init()`**: Added missing mode `0644` to the direct `open()` call with `O_CREAT`.

## 2026-07-04

### Fixed lock file cleanup

- **`macros.h`**: Changed `DIE()` macro from `abort()` to `c_exit(EXIT_FAILURE)` — ensures lock files are cleaned up on all assertion-failure exit paths.
- **`cfan.c` `c_mode_cleanup()`**: Made resilient by ignoring `unlink`/`rmdir` errors (`(void)`). No longer calls `exit()` on failure, so all files are attempted even if one fails.
- **`cfan.c` `c_mode_setup()`**: Changed `exit(EXIT_FAILURE)` to `c_exit(EXIT_FAILURE)` for curve file and chmod failures after lock creation — ensures lock file is cleaned up.
- **`cfan.c` `c_sig_setup()`**: Added `SIGHUP` handler alongside `SIGTERM`/`SIGINT`.
