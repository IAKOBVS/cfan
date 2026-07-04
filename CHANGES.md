# Changelog

## 2026-07-04

### Fixed lock file cleanup

- **`macros.h`**: Changed `DIE()` macro from `abort()` to `c_exit(EXIT_FAILURE)` — ensures lock files are cleaned up on all assertion-failure exit paths.
- **`cfan.c` `c_mode_cleanup()`**: Made resilient by ignoring `unlink`/`rmdir` errors (`(void)`). No longer calls `exit()` on failure, so all files are attempted even if one fails.
- **`cfan.c` `c_mode_setup()`**: Changed `exit(EXIT_FAILURE)` to `c_exit(EXIT_FAILURE)` for curve file and chmod failures after lock creation — ensures lock file is cleaned up.
- **`cfan.c` `c_sig_setup()`**: Added `SIGHUP` handler alongside `SIGTERM`/`SIGINT`.
