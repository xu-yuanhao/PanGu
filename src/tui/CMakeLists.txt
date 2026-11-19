# TUI component build: standalone (scope) and integrated (root) entry.
# Sources are platform-split: shared logic in src/tui/*.c, adapters appended
# per platform by the caller.

set(PANGU_TUI_SOURCES
  src/tui/keys.c
  src/tui/buffer.c
  src/tui/completion.c
  src/tui/view.c
  src/tui/editor.c
  src/tui/chat.c
)

if(WIN32)
  list(APPEND PANGU_TUI_SOURCES src/tui/terminal_win32.c)
else()
  list(APPEND PANGU_TUI_SOURCES src/tui/terminal.c)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(TUI_UTF8PROC REQUIRED IMPORTED_TARGET libutf8proc)
pkg_check_modules(TUI_UV REQUIRED IMPORTED_TARGET libuv)
add_library(pangu_tui ${PANGU_TUI_SOURCES})
target_link_libraries(pangu_tui PUBLIC PkgConfig::TUI_UTF8PROC PkgConfig::TUI_UV)
add_library(PanGu::tui ALIAS pangu_tui)
set_target_properties(pangu_tui PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO EXPORT_NAME tui)
target_include_directories(pangu_tui PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
if(MSVC)
  target_compile_options(pangu_tui PRIVATE /W4)
else()
  target_compile_options(pangu_tui PRIVATE -Wall -Wextra -Wpedantic -Werror)
  find_package(Threads REQUIRED)
  target_link_libraries(pangu_tui PUBLIC Threads::Threads)
endif()

# Keep the static plugin available to TUI-only compile checks. Such builds
# have no installable Agent/Services closure, so only export/install it when
# the parent selects the runtime. Root includes this file before Agent targets
# are created; use the parent's selection rather than TARGET at this point.
add_library(pangu_tui_plugin STATIC "${CMAKE_CURRENT_LIST_DIR}/plugin.c")
add_library(PanGu::tui_plugin ALIAS pangu_tui_plugin)
target_link_libraries(pangu_tui_plugin PUBLIC PanGu::tui PanGu::core)
set_target_properties(pangu_tui_plugin PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO EXPORT_NAME tui_plugin)
if(UNIX)
  target_compile_definitions(pangu_tui_plugin PRIVATE _POSIX_C_SOURCE=200809L)
endif()
if(PANGU_BUILD_AGENT OR PANGU_AGENT_TUI)
  # plugin.h exposes Agent session and Services host ABI; plugin.c directly
  # calls Services, Core and libuv, while session operations use a service table.
  target_link_libraries(pangu_tui_plugin PUBLIC PanGu::agent PanGu::services)
  install(TARGETS pangu_tui_plugin EXPORT PanGuTUIPresentationTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT TUIPresentation)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/pangu/tui/plugin.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pangu/tui COMPONENT TUIPresentation)
  install(EXPORT PanGuTUIPresentationTargets FILE PanGuTUIPresentationTargets.cmake
    NAMESPACE PanGu:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PanGuTUIPresentation
    COMPONENT TUIPresentation)
endif()

# Config files and third-party notices are installed by the root owner.
# Keep plugin.h out of the independent terminal/editor SDK.
set_target_properties(pangu_tui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
install(TARGETS pangu_tui EXPORT PanGuTUITargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT TUI
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT TUI
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT TUI)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/pangu/tui/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pangu/tui COMPONENT TUI
  FILES_MATCHING PATTERN "*.h" PATTERN "plugin.h" EXCLUDE)
install(EXPORT PanGuTUITargets FILE PanGuTUITargets.cmake NAMESPACE PanGu::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PanGuTUI COMPONENT TUI)

if(BUILD_TESTING)
  add_executable(pangu_tui_keys_test tests/tui/keys.c)
  target_link_libraries(pangu_tui_keys_test PRIVATE PanGu::tui)
  add_test(NAME tui_keys COMMAND pangu_tui_keys_test)
  add_executable(pangu_tui_buffer_test tests/tui/buffer.c)
  target_link_libraries(pangu_tui_buffer_test PRIVATE PanGu::tui)
  add_test(NAME tui_buffer COMMAND pangu_tui_buffer_test)
  set_tests_properties(tui_keys tui_buffer PROPERTIES LABELS tui)
  add_executable(pangu_tui_chat examples/tui/chat.c)
  target_link_libraries(pangu_tui_chat PRIVATE PanGu::tui PanGu::core)
  add_executable(pangu_tui_editor_test tests/tui/editor.c)
  target_link_libraries(pangu_tui_editor_test PRIVATE PanGu::tui)
  target_compile_definitions(pangu_tui_editor_test PRIVATE _POSIX_C_SOURCE=200809L)
  if(MSVC)
    target_compile_options(pangu_tui_editor_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(pangu_tui_editor_test PRIVATE -UNDEBUG)
  endif()
  add_test(NAME tui_editor COMMAND pangu_tui_editor_test)
  set_tests_properties(tui_editor PROPERTIES LABELS tui WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  if(UNIX AND NOT CMAKE_CROSSCOMPILING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(pangu_tui_history_test tests/tui/history.c)
    target_link_libraries(pangu_tui_history_test PRIVATE PanGu::tui)
    add_test(NAME tui_history COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/tui/history.py $<TARGET_FILE:pangu_tui_history_test>)
    set_tests_properties(tui_history PROPERTIES LABELS tui TIMEOUT 20)

    add_test(NAME tui_terminal COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/tui/terminal.py $<TARGET_FILE:pangu_tui_chat>)
    set_tests_properties(tui_terminal PROPERTIES LABELS tui TIMEOUT 40 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()
  add_custom_target(pangu_check_tui DEPENDS pangu_tui_editor_test pangu_tui_keys_test pangu_tui_buffer_test pangu_tui_chat)
  if(TARGET pangu_tui_history_test)
    add_dependencies(pangu_check_tui pangu_tui_history_test)
  endif()
  if(TARGET pangu_tui_plugin)
    add_dependencies(pangu_check_tui pangu_tui_plugin)
  endif()
endif()

# Product fixture stays in the TUI ownership area; only build this small target
# in a parent configuration that already enables the runtime and services.
if(BUILD_TESTING AND PANGU_BUILD_AGENT AND PANGU_BUILD_SERVICES AND UNIX AND TARGET pangu_tui_plugin)
  add_executable(pangu_tui_session_test tests/tui/session.c)
  target_link_libraries(pangu_tui_session_test PRIVATE PanGu::agent_app PanGu::tui_plugin PanGu::services_deepseek)
  target_compile_definitions(pangu_tui_session_test PRIVATE _POSIX_C_SOURCE=200809L)
  target_compile_options(pangu_tui_session_test PRIVATE -UNDEBUG)
  add_test(NAME tui_session COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/tui/session.py $<TARGET_FILE:pangu_tui_session_test>)
  set_tests_properties(tui_session PROPERTIES LABELS tui TIMEOUT 40)
  add_test(NAME tui_session_history COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/tui/session_history.py $<TARGET_FILE:pangu_tui_session_test>)
  set_tests_properties(tui_session_history PROPERTIES LABELS tui TIMEOUT 30)
  add_test(NAME tui_approval COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/tui/approval.py $<TARGET_FILE:pangu_tui_session_test>)
  set_tests_properties(tui_approval PROPERTIES LABELS tui TIMEOUT 30)
  add_dependencies(pangu_check_tui pangu_tui_session_test)
endif()

# Root includes TUI before declaring resource targets/options. Defer this small
# fixture until the selected services have been defined; optional builds stay
# free of resource symbols and YAML dependencies.
function(pangu_tui_add_resources_test)
  if(BUILD_TESTING AND TARGET PanGu::agent_app AND TARGET PanGu::services_resources AND UNIX)
    add_executable(pangu_tui_resources_test tests/tui/session.c)
    target_link_libraries(pangu_tui_resources_test PRIVATE PanGu::agent_app PanGu::tui_plugin PanGu::services_resources PanGu::services_deepseek)
    target_compile_definitions(pangu_tui_resources_test PRIVATE _POSIX_C_SOURCE=200809L PANGU_TUI_RESOURCES_TEST=1)
    target_compile_options(pangu_tui_resources_test PRIVATE -UNDEBUG)
    add_test(NAME tui_resources COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/tui/resources.py $<TARGET_FILE:pangu_tui_resources_test>)
    set_tests_properties(tui_resources PROPERTIES LABELS tui TIMEOUT 40)
    add_dependencies(pangu_check_tui pangu_tui_resources_test)
  endif()
endfunction()
cmake_language(DEFER CALL pangu_tui_add_resources_test)
