# cfan
A minimal fan speed controller written in C.
# Features
- Zero dependencies: directly uses sysfs from Linux.
- GPU temperature monitoring with NVML (optional).
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
Fan speed is configured in config.h.
