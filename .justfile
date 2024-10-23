toolchain := "llvm"
arch := arch()
working_dir := absolute_path(".")
cmake_dir := working_dir / "cmake"
linux_cmake_flags := "-DORBI_SANITIZER=ON -DORBI_PEDANTIC=ON"
windows_cmake_flags := '-DCMAKE_SYSROOT='

# [cmake project] [`dbg` | `rel`] [`linux` | `windows`]
build src="test/" build_type="dbg" platform=os() rebuild="false":
    #!/usr/bin/sh

    export ORBI_ARCH="{{arch}}"
    export ORBI_PLATFORM="{{platform}}"
    export ORBI_TOOLCHAIN="{{toolchain}}"
    export ORBI_BUILD_TYPE="{{ if build_type == "dbg" { "Debug" } else { "Release" } }}"
    export ORBI_EXTRA_CMAKE_FLAGS="{{ if platform != "windows" { linux_cmake_flags } else { windows_cmake_flags } }}"
    export ORBI_BUILD_DIR="build/{{src}}/${ORBI_BUILD_TYPE}/${ORBI_ARCH}/${ORBI_PLATFORM}/${ORBI_TOOLCHAIN}"

    cmake -S "{{src}}" -B "${ORBI_BUILD_DIR}"                                                         \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"                                                          \
          --toolchain "{{cmake_dir}}/${ORBI_ARCH}-${ORBI_PLATFORM}-${ORBI_TOOLCHAIN}.toolchain.cmake" \
          -G Ninja                                                                                    \
          ${ORBI_EXTRA_CMAKE_FLAGS}                                                                   \
          {{ if rebuild == "true" { "--fresh" } else { "" } }}

    cmake --build "${ORBI_BUILD_DIR}" -j

    cp "${ORBI_BUILD_DIR}/compile_commands.json" {{justfile_directory()}}


rebuild src="test/" build_type="dbg" platform=os(): (build src build_type platform "true")


