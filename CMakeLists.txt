# $Id: CMakeLists.txt 2018-11-05 D.J. Salvat
# for building with cmake. Based upon geant4 basic/B1 example. To install:
# $ mkdir g4simple-build
# $ cd g4simple-build
# $ cmake -DCMAKE_INSTALL_PREFIX=/path/to/desired/install/ /path/to/g4simple
# $ make
# $ make install
#----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(g4simple)

#----------------------------------------------------------------------------
# Find Geant4 package, activating all available UI and Vis drivers by default
# You can set WITH_GEANT4_UIVIS to OFF via the command line or ccmake/cmake-gui
# to build a batch mode only executable
#
option(WITH_GEANT4_UIVIS "Build with Geant4 UI and Vis drivers" ON)
if(WITH_GEANT4_UIVIS)
  find_package(Geant4 REQUIRED ui_all vis_all)
else()
  find_package(Geant4 REQUIRED)
endif()

#----------------------------------------------------------------------------
# Setup Geant4 include directories and compile definitions
# Setup include directory for this project
#
include(${Geant4_USE_FILE})

#----------------------------------------------------------------------------
# Locate sources and headers for this project
# NB: headers are included so they will show up in IDEs
# DJS: In this particular case we have no headers,
# but I leave it here for generality
file(GLOB sources ${PROJECT_SOURCE_DIR}/*.cc)

#----------------------------------------------------------------------------
# Add the executable, and link it to the Geant4 libraries
#
add_executable(g4simple g4simple.cc ${sources} ${headers})
target_link_libraries(g4simple ${Geant4_LIBRARIES})

#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
install(TARGETS g4simple DESTINATION bin)
