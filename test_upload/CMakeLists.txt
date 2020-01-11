cmake_minimum_required(VERSION 3.5.1)
project(test_upload)
include_directories(usr/local/include/)
link_directories(usr/local/lib)
set(CMAKE_CXX_STANDARD 14)
OPTION(ARM_LINUX "Option description" ON)
IF(ARM_LINUX)
  #  include_directories(include_directories(/opt/openwrt_tools/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/include))
#    INCLUDE_DIRECTORIES("/opt/libcurl-lib/lib/")
    link_directories("/opt/libcurl-lib/lib/")
    link_directories(/opt/libevent_openwrt/lib)


    SET(CMAKE_SYSTEM_NAME Linux)


#    SET(CMAKE_C_COMPILER "mipsel-openwrt-linux-gcc")
    SET(CMAKE_CXX_COMPILER "mipsel-openwrt-linux-g++")

    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_EXE_LINKER_FLAGS "-static")
    add_executable(test_upload main.cpp cJSON.cpp cJSON.h)
    target_link_libraries( test_upload  -lc curl  m pthread  -lgcc_eh)

ELSE()
        add_executable(test_upload main.cpp cJSON.cpp cJSON.h )
        target_link_libraries( test_upload curl  m pthread )

ENDIF()



