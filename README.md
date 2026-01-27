# cfan
A minimal fan speed controller written in C.
# Features
- Zero dependencies: directly uses linux sysfs
# Building
```
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
# cfan
