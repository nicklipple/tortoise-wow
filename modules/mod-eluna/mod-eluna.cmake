if(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "DISCOVERY")
  set(ELUNA_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/Eluna")

  if(NOT IS_DIRECTORY "${ELUNA_SOURCE_DIR}")
    message(FATAL_ERROR "mod-eluna requires the Eluna submodule at ${ELUNA_SOURCE_DIR}.")
  endif()

  include(FetchContent)
  FetchContent_Declare(
    mod_eluna_lua
    URL https://www.lua.org/ftp/lua-5.1.5.tar.gz
    URL_HASH SHA256=2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333
  )
  FetchContent_GetProperties(mod_eluna_lua)
  if(NOT mod_eluna_lua_POPULATED)
    FetchContent_Populate(mod_eluna_lua)
  endif()

  file(GLOB ELUNA_LUA_SOURCES CONFIGURE_DEPENDS
    "${mod_eluna_lua_SOURCE_DIR}/src/*.c")
  list(REMOVE_ITEM ELUNA_LUA_SOURCES
    "${mod_eluna_lua_SOURCE_DIR}/src/lua.c"
    "${mod_eluna_lua_SOURCE_DIR}/src/luac.c")

  add_library(mod_eluna_lua STATIC ${ELUNA_LUA_SOURCES})
  target_include_directories(mod_eluna_lua PUBLIC "${mod_eluna_lua_SOURCE_DIR}/src")
  set_target_properties(mod_eluna_lua PROPERTIES POSITION_INDEPENDENT_CODE ON)

  if(UNIX)
    target_compile_definitions(mod_eluna_lua PUBLIC LUA_USE_LINUX)
    target_link_libraries(mod_eluna_lua PUBLIC ${CMAKE_DL_LIBS} m)
  elseif(WIN32)
    target_compile_definitions(mod_eluna_lua PRIVATE _CRT_SECURE_NO_WARNINGS LUA_USE_WINDOWS)
  endif()

  if(BUILD_TESTING)
    include(CTest)
    add_executable(mod_eluna_handle_tests EXCLUDE_FROM_ALL
      "${CMAKE_CURRENT_LIST_DIR}/src/ElunaHandleRegistry.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/tests/ElunaHandleRegistryTest.cpp")
    target_include_directories(mod_eluna_handle_tests PRIVATE
      "${CMAKE_CURRENT_LIST_DIR}/src")
    set_target_properties(mod_eluna_handle_tests PROPERTIES FOLDER "modules/tests")
    add_test(NAME mod_eluna_handle_tests COMMAND mod_eluna_handle_tests)
  endif()
elseif(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "POST_TARGETS")
  if(NOT TORTOISE_CURRENT_MODULE_LINKAGE STREQUAL "static")
    message(FATAL_ERROR "mod-eluna currently supports static linkage only.")
  endif()

  target_link_libraries(modules PUBLIC mod_eluna_lua)

  add_custom_command(TARGET modules POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lua_scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_LIST_DIR}/lua_scripts/eluna_poc.lua"
      "${CMAKE_BINARY_DIR}/lua_scripts/eluna_poc.lua")

  install(FILES "${CMAKE_CURRENT_LIST_DIR}/lua_scripts/eluna_poc.lua"
    DESTINATION lua_scripts)
endif()
