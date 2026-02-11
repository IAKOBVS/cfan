# cfan
A minimal fan speed controller written in C.
# Features
- Zero dependencies: directly uses sysfs from Linux.
- Custom temperatures: add sysfs temperature files or C functions in table-temp.def.h.
- Nvidia GPU temperature with NVML (optional).
# Building
```
$ make config
$ make
$ sudo make install
```
# Usage
Sudo is required to set fan speed to sysfs.
```
$ sudo cfan
```
## Configuration
Fan speed is configured in config.h, which temperatures to use in table-temp.h.
