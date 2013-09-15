################################################################################
#
# GIL::IO2 test(s) project
#
# Copyright (c) 2013. Domagoj Saric
#
################################################################################


cmake_minimum_required( VERSION 2.8.11 )


project( "GIL IO2 Testing" CXX )

include( ${CMAKE_CURRENT_SOURCE_DIR}/build_options.cmake )


string( REPLACE . "" dotless_libpng_version ${BOOST_TEST_LibPNG_version} )
string( SUBSTRING ${dotless_libpng_version} 0 2 libpng_lib_suffix )
set( BOOST_TEST_libpng_binary_subdirectory "projects/vstudio" CACHE PATH "" )
mark_as_advanced( BOOST_TEST_libpng_binary_subdirectory )


set( boost_path   "${BOOST_TEST_3rd_party_libraries_path}/boost/${BOOST_TEST_Boost_version}"          )
set( libtiff_path "${BOOST_TEST_3rd_party_libraries_path}/libtiff/tiff-${BOOST_TEST_LibTIFF_version}" )
set( libpng_path  "${BOOST_TEST_3rd_party_libraries_path}/libpng/lpng${dotless_libpng_version}"       )
set( libjpeg_path "${BOOST_TEST_3rd_party_libraries_path}/libjpeg/jpeg-${BOOST_TEST_LibJPEG_version}" )


include_directories(
    AFTER
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." # include this/sandboxed GIL before the "official" Boost
    "${BOOST_TEST_3rd_party_libraries_path}/boost/${BOOST_TEST_Boost_version}"
    "${BOOST_TEST_3rd_party_libraries_path}/boost/${BOOST_TEST_Boost_version}/boost/gil" # required for the few modified root (non IO) GIL headers in the sandbox version
    "${libtiff_path}/libtiff"
    "${libpng_path}"
    "${libjpeg_path}"
)

link_directories(
    "${libtiff_path}/libtiff/${CMAKE_CFG_INTDIR}"
    "${libpng_path}/${BOOST_TEST_libpng_binary_subdirectory}/${CMAKE_CFG_INTDIR} Library"
    "${libjpeg_path}/${CMAKE_CFG_INTDIR}"
)

add_definitions(
    -DBOOST_TEST_GIL_IO_IMAGES_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/images\"
)

set( CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG}          ${rttiOnSwitch}" )
set( CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE}        ${sizeOptimizationSwitch} ${ltoCompilerSwitch} ${rttiOffSwitch} ${exceptionsOnSwitch}" )
set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${sizeOptimizationSwitch} ${ltoCompilerSwitch} ${rttiOffSwitch} ${exceptionsOnSwitch}" )
set( CMAKE_C_FLAGS_RELEASE          "${CMAKE_C_FLAGS_RELEASE}          ${sizeOptimizationSwitch} ${ltoCompilerSwitch}" )
set( CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO}   ${sizeOptimizationSwitch} ${ltoCompilerSwitch}" )

set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${ltoLinkerSwitch}" CACHE STRING "link exe release"    FORCE )


set( public_headers_path "${CMAKE_CURRENT_SOURCE_DIR}/../../../../boost/gil/extension/io2" )

set( headers_libjpeg
    ${public_headers_path}/backends/libjpeg/backend.hpp
    ${public_headers_path}/backends/libjpeg/reader.hpp
    ${public_headers_path}/backends/libjpeg/writer.hpp
)
source_group( "LibJPEG" FILES ${headers_libjpeg} )

set( headers_libpng
    ${public_headers_path}/backends/libpng/backend.hpp
    ${public_headers_path}/backends/libpng/reader.hpp
    ${public_headers_path}/backends/libpng/writer.hpp
)
source_group( "LibPNG" FILES ${headers_libpng} )

set( headers_libtiff
    ${public_headers_path}/backends/libtiff/backend.hpp
    ${public_headers_path}/backends/libtiff/reader.hpp
    ${public_headers_path}/backends/libtiff/writer.hpp
)
source_group( "LibTIFF" FILES ${headers_libtiff} )

if ( WIN32 )
set( headers_gp
    ${public_headers_path}/backends/gp/backend.hpp
    ${public_headers_path}/backends/gp/reader.hpp
    ${public_headers_path}/backends/gp/writer.hpp
)
source_group( "GDI+" FILES ${headers_gp} )

set( headers_wic
    ${public_headers_path}/backends/wic/backend.hpp
    ${public_headers_path}/backends/wic/reader.hpp
    ${public_headers_path}/backends/wic/writer.hpp
)
source_group( "WIC" FILES ${headers_wic} )
endif()

add_executable( gio_io2_tester
    test.cpp
    ${headers_libjpeg}
    ${headers_libpng}
    ${headers_libtiff}
    ${headers_gp}
    ${headers_wic}
)

#addPCH( gio_io2_tester precompiled_headers )

target_link_libraries( gio_io2_tester

    libtiff
    jpeg
    libpng${libpng_lib_suffix}
)

if ( WIN32 )
    target_link_libraries( gio_io2_tester ole32.lib )
endif()
