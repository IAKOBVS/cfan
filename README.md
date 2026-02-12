# cfan
A minimal fan speed controller written in C.
# Features
- Zero dependencies: directly uses sysfs from Linux.
- Silent: does not ramp up for short spikes, as in opening a browser, unless it is already too hot.
- Custom temperatures: provide your own temperature files or C functions in table-temp.def.h.
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
