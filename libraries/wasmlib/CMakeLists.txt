
file(GLOB HEADERS "include/*.hpp")

add_library( wasmlib SHARED wasmlib.cpp
        name.cpp
        types.cpp

        wasm_interface.cpp
        wasm_gas_table.cpp
        wasm_validation.cpp
        wasm_injection.cpp
        wasm_context.cpp
        wavm.cpp

        ${HEADERS}
        )

if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling ${NODE_EXECUTABLE_NAME} with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

if(UNIX)
  if(APPLE)
    set(whole_archive_flag "-force_load")
    set(no_whole_archive_flag "")
    set(build_id_flag "")
  else()
    set(whole_archive_flag "--whole-archive")
    set(no_whole_archive_flag "--no-whole-archive")
    set(build_id_flag "--build-id")
  endif()
else()
  set(whole_archive_flag "--whole-archive")
  set(no_whole_archive_flag "--no-whole-archive")
  set(build_id_flag "")
endif()

set(Boost_USE_STATIC_LIBS TRUE)
find_package(Boost 1.67 REQUIRED COMPONENTS
        date_time
        filesystem
        system
        chrono
        iostreams
        unit_test_framework)

# This will become unnecessary once we update to websocketpp which fixes upstream issue #395
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEBSOCKETPP_STRICT_MASKING")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ASIO_HAS_STD_CHRONO")
IF (APPLE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ASIO_HAS_STD_STRING_VIEW")
ENDIF (APPLE)

target_link_libraries( wasmlib
        PRIVATE -Wl,${build_id_flag}
        PRIVATE Logging IR WAST WASM Runtime softfloat builtins ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

target_include_directories( wasmlib
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../wasm-jit/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../softfloat/source/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../builtins"
        "${Boost_INCLUDE_DIR}"
        )
