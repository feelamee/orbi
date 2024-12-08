### BUILD

#### CONFIGURE
```sh
cmake -S [any cmake project, e.g. example/triangle]
      -B build
      --toolchain [any cmake/*.toolchain.cmake]
      -C cmake/common.cmake
      -DORBI_SANITIZER=[ON | OFF(default)]
      -DORBI_PEDANTIC=[ON | OFF(default)]
```

#### COMPILE
```sh
cmake --build build -j
```
