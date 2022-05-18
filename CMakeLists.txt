cmake_minimum_required(VERSION 3.12)
project(ffmpegcapturer VERSION 1.0.0 LANGUAGES C CXX)

# set(CMAKE_C_FLAGS_ASAN
#     "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
#     CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
#     FORCE)

# set(CMAKE_C_FLAGS_LSAN
#     "-fsanitize=leak -fno-omit-frame-pointer -g -O1"
#     CACHE STRING "Flags used by the C compiler during LeakSanitizer builds."
#     FORCE)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SAMPLE "Build sample" ON)

set (CMAKE_C_STANDARD 99)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavdevice
    libavformat
    libavcodec
    libavutil
    # libavfilter
    # libswresample
    # libswscale
)

set(SRCS
    ${CMAKE_CURRENT_LIST_DIR}/src/FFMPEGCapturer.c
)

set(INCS_DIR ${CMAKE_CURRENT_LIST_DIR}/include/)
set(INCS ${INCS_DIR}/FFMPEGCapturer.h)

add_library(${CMAKE_PROJECT_NAME} STATIC ${SRCS})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE PkgConfig::FFMPEG)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${INCS_DIR})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${INCS})
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::FFMPEG)

install(TARGETS ${CMAKE_PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})


if(${BUILD_SAMPLE})
    set(CMAKE_CXX_STANDARD 17)
    set(SAMPLE_SRCS sample/FFMPEGCapturerSample.cpp)
    add_executable(FFMPEGCapturerSample ${SAMPLE_SRCS})
    target_link_libraries(FFMPEGCapturerSample ${CMAKE_PROJECT_NAME})
    install(TARGETS FFMPEGCapturerSample
            RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
endif()
