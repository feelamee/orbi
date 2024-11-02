toolchain := "llvm"
arch := arch()
working_dir := absolute_path(".")
cmake_dir := working_dir / "cmake"
enable_sanitizer_flags := "-DORBI_SANITIZER=ON -DORBI_PEDANTIC=ON"
build_dir := working_dir / "build"
current_config_file := build_dir / "current"

configure src="test/" build_type="dbg" platform=os() rebuild="false" sanitizer="true":
    #!/usr/bin/env bash

    set -uo pipefail

    echo '
    export ORBI_ARCH="{{arch}}"
    export ORBI_PLATFORM="{{platform}}"
    export ORBI_TOOLCHAIN="{{toolchain}}"
    export ORBI_BUILD_TYPE="{{ if build_type == "dbg" { "Debug" } else { "Release" } }}"
    export ORBI_EXTRA_CMAKE_FLAGS="{{ if platform != "windows" { if  sanitizer == "true" { enable_sanitizer_flags } else { '' } } else { '' } }}"
    export ORBI_BUILD_DIR="{{build_dir}}/{{src}}/${ORBI_BUILD_TYPE}/${ORBI_ARCH}/${ORBI_PLATFORM}/${ORBI_TOOLCHAIN}"
    export ORBI_EXTRA_CMAKE_CXX_FLAGS="{{ if platform != "windows" { '-DCMAKE_CXX_FLAGS="-fcolor-diagnostics"' } else { '' } }}"
    export ORBI_SRC="{{src}}"
    ' > "{{current_config_file}}"

    . "{{current_config_file}}"

    mkdir -p "${ORBI_BUILD_DIR}"

    cmake -S "{{src}}" -B "${ORBI_BUILD_DIR}"                                                         \
          -DCMAKE_BUILD_TYPE="${ORBI_BUILD_TYPE}"                                                     \
          --toolchain "{{cmake_dir}}/${ORBI_ARCH}-${ORBI_PLATFORM}-${ORBI_TOOLCHAIN}.toolchain.cmake" \
          -G Ninja                                                                                    \
          ${ORBI_EXTRA_CMAKE_FLAGS}                                                                   \
          {{ if rebuild == "true" { "--fresh" } else { "" } }}                                        \
          -C "{{cmake_dir}}/common.cmake"                                                             \
          ${ORBI_EXTRA_CMAKE_CXX_FLAGS}

    exit $?

_check_current_file:
    #!/usr/bin/env bash

    set -uo pipefail

    cat "{{current_config_file}}" &>/dev/null
    if [[ $? != 0 ]];
    then
        echo "*{{current_config_file}}* not found. Run ``configure`` before."
        exit 1
    fi

build: _check_current_file
    #!/usr/bin/env bash

    set -uo pipefail

    . "{{current_config_file}}"

    cmake --build "${ORBI_BUILD_DIR}" -j

    [[ $? != 0 ]] && exit 2

    cp "${ORBI_BUILD_DIR}/compile_commands.json" {{working_dir}}

    exit $?

run: build
    #!/usr/bin/env bash

    set -uo pipefail

    . "{{current_config_file}}"

    executable="${ORBI_BUILD_DIR}/$(basename ${ORBI_SRC})"
    echo "running: ${executable}"

    "${executable}"
