# ======================================================================
#  icartdaq main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/artdaq-demo/ups/setup_for_development
#  buildtool
# ======================================================================


# use cmake 2.8 or later
cmake_minimum_required (VERSION 2.8)


project(bernfebdrv)
# cetbuildtools contains our cmake modules
SET ( CETBUILDTOOLS_VERSION $ENV{CETBUILDTOOLS_VERSION} )
IF (NOT CETBUILDTOOLS_VERSION)
    MESSAGE (FATAL_ERROR "ERROR: setup cetbuildtools to get the cmake modules")
ENDIF()

set(CMAKE_MODULE_PATH 
        $ENV{ART_DIR}/Modules
        $ENV{CETBUILDTOOLS_DIR}/Modules
        ${CMAKE_MODULE_PATH})


include(CetCMakeEnv)
cet_cmake_env()

cet_set_compiler_flags(DIAGS VIGILANT
 WERROR
 NO_UNDEFINED
 EXTRA_FLAGS -pedantic -Wall -fPIC -DLINUX -D_LINUX -std=c++11
 )

cet_report_compiler_flags()

# these are minimum required versions, not the actual product versions
find_ups_product( cetbuildtools v3_03_00 )
find_ups_product( gcc v4_9_2 )

# macros for art_dictionary and simple_plugin
#include(BuildPlugins)
#include(CetTest)

#include_directories($ENV{CAENCOMMDIR}/include)
#link_directories($ENV{CAENCOMMDIR}/lib/x64)


#source
add_subdirectory(bernfebdrv)

# ups - table and config files
add_subdirectory(ups)

# packaging utility
include(UseCPack)
