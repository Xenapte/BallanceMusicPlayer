cmake_minimum_required(VERSION 3.20)

project(BallanceMusicPlayer)
set(PROJECT_NAME_NOPREFIX MusicPlayer)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(BML REQUIRED)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/miniaudio.cpp
    src/BallSpeedTracker.cpp
    src/BallSpeedTracker.hpp
    src/FilePicker.hpp
    src/MusicPlayerCore.hpp
    src/Common.hpp
    src/SecondaryConfig.hpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
               ${BML_DIR}/../../../include
               ${VirtoolsSDK_DIR}/../include
               ${CMAKE_CURRENT_SOURCE_DIR}/deps/miniaudio)

target_link_libraries(${PROJECT_NAME}
                      PRIVATE
                      BML
                      CK2
                      VxMath
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /Zc:strictStrings-
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME_NOPREFIX}")
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".bmodp")