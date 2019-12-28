# bow
Mirror of https://www.cs.cmu.edu/~mccallum/bow/

## Compiling

Bow uses GNU extensions to ANSI C89. It should be compiled as follows:

```
$ mkdir build
$ cd build
$ CC='gcc -std=gnu89' ../configure
$ make
```

## Security

The function `sprintf` is widely used in this codebase and should
at a minimum be replaced with `snprintf`.
