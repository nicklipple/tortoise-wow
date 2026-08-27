if(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "DISCOVERY")
  set(ELUNA_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/Eluna")
  set(ELUNA_ADAPTER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

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

  set(ELUNA_SOURCES
    "${ELUNA_SOURCE_DIR}/LuaEngine.cpp"
    "${ELUNA_SOURCE_DIR}/ElunaConfig.cpp"
    "${ELUNA_SOURCE_DIR}/ElunaEventMgr.cpp"
    "${ELUNA_SOURCE_DIR}/ElunaMgr.cpp"
    "${ELUNA_SOURCE_DIR}/ElunaCompat.cpp"
    "${ELUNA_ADAPTER_SOURCE_DIR}/ElunaLoader.cpp"
    "${ELUNA_ADAPTER_SOURCE_DIR}/ElunaMethods.cpp"
    "${ELUNA_ADAPTER_SOURCE_DIR}/ElunaHooks.cpp"
    "${ELUNA_ADAPTER_SOURCE_DIR}/ElunaUtility.cpp"
    "${ELUNA_ADAPTER_SOURCE_DIR}/ElunaModule.cpp"
    "${ELUNA_ADAPTER_SOURCE_DIR}/ElunaModule.h"
    "${ELUNA_ADAPTER_SOURCE_DIR}/TortoiseElunaIncludes.h")

  foreach(ELUNA_SOURCE ${ELUNA_SOURCES})
    TW_ADD_SCRIPT("${ELUNA_SOURCE}")
  endforeach()

  TW_ADD_SCRIPT("${ELUNA_SOURCE_DIR}/hooks/Hooks.h")
  TW_ADD_SCRIPT("${ELUNA_SOURCE_DIR}/hooks/HookHelpers.h")

  CopyModuleConfig("${CMAKE_CURRENT_LIST_DIR}/conf/mod_eluna.conf.dist")
elseif(TORTOISE_MODULE_CMAKE_PHASE STREQUAL "POST_TARGETS")
  if(NOT TORTOISE_CURRENT_MODULE_LINKAGE STREQUAL "static")
    message(FATAL_ERROR "mod-eluna currently supports static linkage only.")
  endif()

  target_link_libraries(modules PUBLIC mod_eluna_lua)
  target_include_directories(modules PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/Eluna"
    "${CMAKE_CURRENT_LIST_DIR}/Eluna/hooks")

  set_source_files_properties(${ELUNA_SOURCES}
    PROPERTIES
      COMPILE_DEFINITIONS "ELUNA_MANGOS;ELUNA_EXPANSION=0")

  if(MSVC)
    set_source_files_properties(${ELUNA_SOURCES}
      PROPERTIES COMPILE_OPTIONS "/FI${CMAKE_CURRENT_LIST_DIR}/src/TortoiseElunaIncludes.h")
  else()
    set_source_files_properties(${ELUNA_SOURCES}
      PROPERTIES COMPILE_OPTIONS "-include${CMAKE_CURRENT_LIST_DIR}/src/TortoiseElunaIncludes.h")
  endif()

  add_custom_command(TARGET modules POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lua_scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_LIST_DIR}/lua_scripts/eluna_poc.lua"
      "${CMAKE_BINARY_DIR}/lua_scripts/eluna_poc.lua")

  install(FILES "${CMAKE_CURRENT_LIST_DIR}/lua_scripts/eluna_poc.lua"
    DESTINATION lua_scripts)
endif()
