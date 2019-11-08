cmake_minimum_required(VERSION 3.5)
project(fractal_wasm)

set(CMAKE_CXX_STANDARD 14)
set(BASE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/../run")
set(CMAKE_INSTALL_FULL_LIBDIR "${BASE_BINARY_DIR}/lib")
set(CMAKE_INSTALL_FULL_INCLUDEDIR "${BASE_BINARY_DIR}/include")

IF (C_COMPILER MATCHES "gcc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wno-format-truncation")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wno-format-truncation")
ELSE (C_COMPILER MATCHES "gcc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC ")
ENDIF (C_COMPILER MATCHES "gcc")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

add_subdirectory(builtins)
add_subdirectory(softfloat)
add_subdirectory(wasm-jit EXCLUDE_FROM_ALL)
add_subdirectory(wasmlib)
