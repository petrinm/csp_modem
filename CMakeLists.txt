project(csp_modem LANGUAGES CXX)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CMakeFindDependencyMacro)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
 
add_compile_options(-g -Wall -Wextra)

# New executable
add_executable(csp_modem
    csp_modem.cpp
    csp_adapter.cpp
    csp_if_zmq_server.cpp
    randomizer.cpp
    fec.cpp
)

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
