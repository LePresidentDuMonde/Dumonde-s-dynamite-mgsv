cmake_minimum_required(VERSION 3.25)
project(dinput8)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(FLATBUFFERS_BUILD_TESTS OFF)

if (ANALYZE)
    set(OPTS -fanalyzer -Werror)
endif ()

include(FetchContent)
FetchContent_Declare(
        minhook

        GIT_REPOSITORY https://github.com/unknown321/minhook.git
        GIT_TAG bd826f3090f665fa90500f7a860706286458a69b
)
FetchContent_MakeAvailable(minhook)

FetchContent_Declare(
        mINI
        GIT_REPOSITORY https://github.com/metayeti/mINI.git
        GIT_TAG 52b66e987cb56171dc91d96115cdf094b6e4d7a0
)
FetchContent_MakeAvailable(mINI)


FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG 486b55554f11c9cccc913e11a87085b2a91f706f
)
FetchContent_MakeAvailable(spdlog)


FetchContent_Declare(
        flatbuffers
        GIT_REPOSITORY https://github.com/google/flatbuffers.git
        GIT_TAG 187240970746d00bbd26b0f5873ed54d2477f9f3
)
FetchContent_MakeAvailable(flatbuffers)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamiteSyncImpl/DynamiteSyncSchema_generated.h
        COMMAND $<TARGET_FILE:flatc> --cpp -o ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamiteSyncImpl
                ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamiteSyncImpl/DynamiteSyncSchema.fbs
        DEPENDS flatc ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamiteSyncImpl/DynamiteSyncSchema.fbs
)
add_custom_target(sync_schema DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/DynamiteSyncImpl/DynamiteSyncSchema_generated.h)

include_directories(
        src/
)

SET(LUA_COMPAT_OPENLIB 1)

if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/__header.h)
    message(FATAL_ERROR "File \"${CMAKE_CURRENT_BINARY_DIR}/__header.h\" found, \
    this should never be created, remove!")
endif()

add_custom_command(
        OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/__header.h  # fake! ensure we run!
        COMMAND ${CMAKE_COMMAND} -E true
)

add_custom_target(version
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/src/Version.h
        COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/src/Version.h.in
        -D DST=${CMAKE_SOURCE_DIR}/src/Version.h
        -P ${CMAKE_SOURCE_DIR}/GenerateVersion.cmake
        ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/__header.h
)

add_library(${PROJECT_NAME} SHARED
        src/mgsvtpp_funcptr_defs.cpp
        src/mgsvtpp_funcptr_set.cpp
        src/Tpp/GameObject.cpp
        src/Tpp/TppSoldier2.cpp
        src/DynamiteSyncImpl/DynamiteSyncImpl.cpp
        src/Config.cpp
        src/Messagebox.cpp
        src/patch.cpp
        src/Tpp/TppTypes.cpp
        src/util.cpp

        src/dynamiteHooks.cpp
        src/dynamitePatches.cpp
        src/DynamiteCore.cpp
        src/dynamite.cpp
        src/DynamiteLua.cpp
        src/PartnerHorse.cpp
        src/LocalBuddy.cpp
        src/BuddyScans.cpp
        src/DogReplica.cpp
        src/DynamiteHook.cpp
        src/main.cpp
        src/DInputProxy.cpp
)

add_dependencies(${PROJECT_NAME} version)
add_dependencies(${PROJECT_NAME} sync_schema)

target_compile_options(${PROJECT_NAME} PUBLIC -fpermissive ${OPTS})

target_link_options(${PROJECT_NAME} PUBLIC -static)
target_link_libraries(${PROJECT_NAME} flatbuffers minhook mINI spdlog::spdlog_header_only)

option(DYNAMITE_BUILD_TESTS "Build offline protocol regression tests" OFF)
if(DYNAMITE_BUILD_TESTS)
    enable_testing()
    add_executable(receive_demux_tests ../tests/receive_demux.cpp)
    target_link_options(receive_demux_tests PRIVATE -static)
    add_test(NAME receive_demux_tests COMMAND receive_demux_tests)
    add_executable(session_notify_tests ../tests/session_notify.cpp)
    target_link_options(session_notify_tests PRIVATE -static)
    add_test(NAME session_notify_tests COMMAND session_notify_tests)
    add_executable(sync_tests ../tests/native_sync.cpp ../tests/native_buddy.cpp ../tests/dog_replica.cpp src/DynamiteSyncImpl/DynamiteSyncImpl.cpp src/mgsvtpp_funcptr_defs.cpp src/Tpp/TppTypes.cpp)
    add_dependencies(sync_tests sync_schema version)
    target_link_libraries(sync_tests flatbuffers minhook mINI spdlog::spdlog_header_only)
    target_compile_options(sync_tests PRIVATE -fpermissive)
    target_link_options(sync_tests PRIVATE -static)
    add_test(NAME sync_tests COMMAND sync_tests)
endif()
