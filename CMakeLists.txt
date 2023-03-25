cmake_minimum_required (VERSION 3.5)

project (v8)

add_definitions(-D_XOPEN_SOURCE=600)
add_definitions(-D_DARWIN_C_SOURCE)
add_definitions(-DV8_TARGET_ARCH_X64)
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=YES)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fms-extensions -fno-strict-aliasing")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-extensions -fno-strict-aliasing")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

file(GLOB DTOA_SRC
  src/dtoa-config.c
)

add_library(dtoa STATIC ${DTOA_SRC})
target_compile_definitions(dtoa PRIVATE ENABLE_LOGGING_AND_PROFILING ENABLE_DISASSEMBLER DEBUG)
set_target_properties(dtoa PROPERTIES COMPILE_FLAGS "-ansi -Werror -fno-rtti -fno-exceptions")
target_link_libraries(dtoa PRIVATE Threads::Threads)

add_custom_command(
  OUTPUT libraries.cc
  COMMAND python3
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/js2c.py
    ${CMAKE_CURRENT_BINARY_DIR}/libraries.cc
    ${CMAKE_CURRENT_BINARY_DIR}/ibraries-empty.cc
    CORE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/v8natives.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/array.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/string.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uri.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/math.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/messages.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/apinatives.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug-delay.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mirror-delay.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/date-delay.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/regexp-delay.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json-delay.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/macros.py
)

file(GLOB V8_SRC
  src/accessors.cc
  src/allocation.cc
  src/api.cc
  src/assembler.cc
  src/ast.cc
  src/bootstrapper.cc
  src/builtins.cc
  src/checks.cc
  src/code-stubs.cc
  src/codegen.cc
  src/compilation-cache.cc
  src/compiler.cc
  src/contexts.cc
  src/conversions.cc
  src/counters.cc
  src/dateparser.cc
  src/debug.cc
  src/debug-agent.cc
  src/disassembler.cc
  src/execution.cc
  src/factory.cc
  src/flags.cc
  src/frame-element.cc
  src/frames.cc
  src/func-name-inferrer.cc
  src/global-handles.cc
  src/handles.cc
  src/hashmap.cc
  src/heap.cc
  src/ic.cc
  src/interpreter-irregexp.cc
  src/jsregexp.cc
  src/jump-target.cc
  src/log.cc
  src/log-utils.cc
  src/mark-compact.cc
  src/messages.cc
  src/objects.cc
  src/oprofile-agent.cc
  src/parser.cc
  src/property.cc
  src/regexp-macro-assembler.cc
  src/regexp-macro-assembler-irregexp.cc
  src/regexp-stack.cc
  src/register-allocator.cc
  src/rewriter.cc
  src/runtime.cc
  src/scanner.cc
  src/scopeinfo.cc
  src/scopes.cc
  src/serialize.cc
  src/snapshot-common.cc
  src/spaces.cc
  src/string-stream.cc
  src/stub-cache.cc
  src/token.cc
  src/top.cc
  src/unicode.cc
  src/usage-analyzer.cc
  src/utils.cc
  src/v8-counters.cc
  src/v8.cc
  src/v8threads.cc
  src/variables.cc
  src/version.cc
  src/virtual-frame.cc
  src/zone.cc

  # PLATFORM_DEPENDENT_SOURCES
  src/x64/assembler-x64.cc
  src/x64/builtins-x64.cc
  src/x64/codegen-x64.cc
  src/x64/cpu-x64.cc
  src/x64/disasm-x64.cc
  src/x64/debug-x64.cc
  src/x64/frames-x64.cc
  src/x64/ic-x64.cc
  src/x64/jump-target-x64.cc
  src/x64/macro-assembler-x64.cc
  src/x64/register-allocator-x64.cc
  src/x64/stub-cache-x64.cc
  src/x64/virtual-frame-x64.cc

  # for macos
  src/platform-macos.cc
  src/platform-posix.cc
)

add_executable(mksnapshot 
  ${V8_SRC}
  ${CMAKE_CURRENT_BINARY_DIR}/libraries.cc
  src/mksnapshot.cc
  src/snapshot-empty.cc)
target_include_directories(mksnapshot PRIVATE src)
target_link_libraries(mksnapshot dtoa)

if (USE_SNAPSHOT)
  add_custom_command(
    OUTPUT snapshot.cc
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/mksnapshot
      snapshot.cc
    DEPENDS mksnapshot
  )
  add_library(v8 STATIC ${V8_SRC}
    ${CMAKE_CURRENT_BINARY_DIR}/libraries.cc
    ${CMAKE_CURRENT_BINARY_DIR}/snapshot.cc
  )
else()
  add_library(v8 STATIC ${V8_SRC}
    ${CMAKE_CURRENT_BINARY_DIR}/libraries.cc
    src/snapshot-empty.cc
  )
endif()

target_include_directories(v8 PRIVATE src)
target_link_libraries(v8 dtoa)

add_executable(v8_test test/main.cc)
target_include_directories(v8_test PRIVATE include)
target_link_libraries(v8_test v8)
target_compile_features(v8_test PUBLIC cxx_std_11)

add_executable(v8_shell samples/shell.cc)
target_include_directories(v8_shell PRIVATE include)
target_link_libraries(v8_shell v8)

add_custom_command(
  OUTPUT d8-js.cc
  COMMAND python3
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/js2c.py
    ${CMAKE_CURRENT_BINARY_DIR}/d8-js.cc
    ${CMAKE_CURRENT_BINARY_DIR}/d8-js-empty.cc
    D8
    ${CMAKE_CURRENT_SOURCE_DIR}/src/d8.js
)
add_executable(d8 src/d8.cc src/d8-debug.cc src/d8-posix.cc
  ${CMAKE_CURRENT_BINARY_DIR}/d8-js.cc
)
target_include_directories(d8 PRIVATE src)
target_link_libraries(d8 v8)

add_executable(v8_cctest
  test/cctest/cctest.cc
  test/cctest/test-alloc.cc
  test/cctest/test-api.cc
  test/cctest/test-ast.cc
  test/cctest/test-compiler.cc
  test/cctest/test-conversions.cc
  test/cctest/test-debug.cc
  test/cctest/test-decls.cc
  test/cctest/test-flags.cc
  test/cctest/test-func-name-inference.cc
  test/cctest/test-hashmap.cc
  test/cctest/test-heap.cc
  test/cctest/test-list.cc
  test/cctest/test-lock.cc
  test/cctest/test-log.cc
  test/cctest/test-log-utils.cc
  test/cctest/test-mark-compact.cc
  test/cctest/test-regexp.cc
  test/cctest/test-serialize.cc
  test/cctest/test-sockets.cc
  test/cctest/test-spaces.cc
  test/cctest/test-strings.cc
  test/cctest/test-threads.cc
  test/cctest/test-utils.cc
  test/cctest/test-version.cc

  # for MacOS
  test/cctest/test-platform-macos.cc
)
target_include_directories(v8_cctest PRIVATE src)
target_link_libraries(v8_cctest v8)
