# Solo
solo is a simple script used to implement a shell singleton. By wrapping a
command invocation with solo, the program is guaranteed to be the only one
among others wrapped with solo.

Usage is as follows:
```
solo COMMAND
solo [-l|--lockfile] LOCKFILE COMMAND
```

## Building
To build, you need `meson` and `ninja`, which should be available in recent
Linux distros. Once these are installed, do the following:
```
mkdir build # or wherever you want your artifacts to go
cd build
meson ..
ninja
```
