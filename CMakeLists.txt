project(csp_modem LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SATELLITE_CONFIG_CPP "sample_cfg.cpp" CACHE STRING "Configuration file to be used")

option(SUPPORT_PORTHOUSE "Compile with porthouse tracking support" OFF)
option(SUPPORT_RIGCTL "Compile with rigctl tracking support" OFF)

option(OUTPUT_RAW_FRAMES "Open additional ZMQ socket for raw frames" OFF)


include(CMakeFindDependencyMacro)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
 
add_compile_options(-g -Wall -Wextra)

file(GENERATE OUTPUT debug.txt CONTENT "${SATELLITE_CONFIG_CPP}")

# Setup csp_modem executable
add_executable(csp_modem
    csp_modem.cpp
    csp_suo_adapter.cpp
    csp_if_zmq_server.cpp
    randomizer.cpp
    ${SATELLITE_CONFIG_CPP})


# Setup Suo library
find_package(Suo REQUIRED)
target_include_directories(csp_modem PUBLIC ${Suo_INCLUDE_DIRS})
target_link_libraries(csp_modem PUBLIC ${Suo_LIBRARIES})

# Setup CSP library
find_package(CSP REQUIRED)
target_include_directories(csp_modem PUBLIC ${CSP_INCLUDE_DIRS})
target_link_libraries(csp_modem PUBLIC ${CSP_LIBRARIES})

# Setup ZMQ library
find_package(ZeroMQ REQUIRED)
target_include_directories(csp_modem PUBLIC ${ZeroMQ_INCLUDE_DIRS})
target_link_libraries(csp_modem PUBLIC ${ZeroMQ_LIBRARIES})


if (SUPPORT_PORTHOUSE)
    target_compile_definitions(csp_modem PRIVATE SUO_SUPPORT_AMQP USE_PORTHOUSE_TRACKER)
    target_link_libraries(csp_modem PUBLIC pthread amqpcpp)
endif()

if (SUPPORT_RIGCTL)
    target_compile_definitions(csp_modem PRIVATE USE_RIGCTL_TRACKER)
endif()

if (USE_EXTERNAL_SECRET)
    target_compile_definitions(csp_modem PRIVATE EXTERNAL_SECRET)
endif()

if (OUTPUT_RAW_FRAMES)
    target_compile_definitions(csp_modem PRIVATE OUTPUT_RAW_FRAMES)
endif()



if (0)

    # Setup GNURadio bridge executable
    add_executable(gnuradio_bridge
        csp_gnuradio_adapter.cpp
        csp_if_zmq_server.cpp
        gnuradio_bridge.cpp
        randomizer.cpp
    )

    target_link_libraries(gnuradio_bridge PUBLIC gnuradio-pmt)

    target_include_directories(gnuradio_bridge PUBLIC ${Suo_INCLUDE_DIRS})
    target_link_libraries(gnuradio_bridge PUBLIC ${Suo_LIBRARIES})

    target_include_directories(gnuradio_bridge PUBLIC ${CSP_INCLUDE_DIRS})
    target_link_libraries(gnuradio_bridge PUBLIC ${CSP_LIBRARIES})

    target_include_directories(gnuradio_bridge PUBLIC ${ZeroMQ_INCLUDE_DIRS})
    target_link_libraries(gnuradio_bridge PUBLIC ${ZeroMQ_LIBRARIES})

    target_sources(gnuradio_bridge PRIVATE 
        libfec/ccsds_tab.c
        libfec/decode_rs_8.c
        libfec/encode_rs_8.c
    )
    target_compile_definitions(gnuradio_bridge PRIVATE LIBFEC)

endif()
