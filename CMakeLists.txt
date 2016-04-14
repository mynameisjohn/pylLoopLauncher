cmake_minimum_required(VERSION 2.8)

project(pylLoopLauncher)

set(CMAKE_CXX_FLAGS "-std=c++14 -Wall")

# Python libraries for pyliaison
if (WIN32)
	set(PYTHON_LIBRARY C:/Python35/libs/python35_d.lib)
	set(PYTHON_INCLUDE_DIR C:/Python35/include)
	add_definitions(-DGLEW_STATIC)
else(WIN32)
	set(PYTHON_LIBRARY /usr/local/lib/libpython3.5m.a)
	set(PYTHON_INCLUDE_DIR /usr/local/include/python3.5m)
endif(WIN32)

# SFML, we need window, system, audio (for now)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Modules")
find_package(SFML COMPONENTS audio window system)

# Source files, include files, scripts
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.py)

# Pyliaison, which has its own folder and source file]
file(GLOB PYL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/pyl/*.cpp)
file(GLOB PYL_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/pyl/*.h)
add_library(PyLiaison ${PYL_SOURCES} ${PYL_HEADERS})
target_include_directories(PyLiaison PUBLIC ${PYL_HEADERS} ${PYTHON_INCLUDE_DIR})

# Add the LoopLauncher executable, which depends on source, include, and scripts
add_executable(LoopLauncher ${SOURCES} ${HEADERS} ${SCRIPTS})

# Make sure it gets its include paths
target_include_directories(LoopLauncher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${PYTHON_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/pyl ${SFML_INCLUDE_DIR})
target_link_libraries(LoopLauncher LINK_PUBLIC PyLiaison ${PYTHON_LIBRARY} ${SFML_LIBRARIES})
