cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

project(mcl)

#--------------------------------------------------------------------------------------------------
# Configurable options:

option(STATIC_BINARIES "Link binaries statically." ON)
option(USE_SORELEASE   "Use SORELEASE in shared library filename." ON)

#--------------------------------------------------------------------------------------------------
# Library version:

set(MCL_SOMAJOR   1)
set(MCL_SOMINOR   0)
set(MCL_SORELEASE 0)

# Compute VERSION and SOVERSION:
if(USE_SORELEASE)
  set(MCL_VERSION ${MCL_SOMAJOR}.${MCL_SOMINOR}.${MCL_SORELEASE})
else()
  set(MCL_VERSION ${MCL_SOMAJOR}.${MCL_SOMINOR})
endif()
set(MCL_SOVERSION ${MCL_SOMAJOR})

#--------------------------------------------------------------------------------------------------
# Dependencies:

include_directories(${minisat_SOURCE_DIR})
include_directories(${mcl_SOURCE_DIR})

#--------------------------------------------------------------------------------------------------
# Build Targets:

set(MCL_LIB_SOURCES
    mcl/ReTime.cc
    mcl/Aiger.cc
    mcl/Matching.cc
    mcl/Normalization.cc
    mcl/Hardware.cc
    mcl/DagShrink.cc
    mcl/Smv.cc
    mcl/CircPrelude.cc
    mcl/Flops.cc
    mcl/Equivs.cc
    mcl/SatSweep.cc
    mcl/Circ.cc )

add_library(mcl-lib-static STATIC ${MCL_LIB_SOURCES})
add_library(mcl-lib-shared SHARED ${MCL_LIB_SOURCES})

target_link_libraries(mcl-lib-shared minisat-lib-shared)
target_link_libraries(mcl-lib-static minisat-lib-static)

set_target_properties(mcl-lib-static PROPERTIES OUTPUT_NAME "mcl")
set_target_properties(mcl-lib-shared
  PROPERTIES
    OUTPUT_NAME "mcl" 
    VERSION ${MCL_VERSION}
    SOVERSION ${MCL_SOVERSION})

#--------------------------------------------------------------------------------------------------
# Installation targets:

install(TARGETS mcl-lib-static mcl-lib-shared
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY mcl
        DESTINATION include/mcl
        FILES_MATCHING PATTERN "*.h")
