################################################################################
#
# Compiler and linker options and utility functions shared across projects.
#
# Copyright (c) 2011-2013. Domagoj Saric, Little Endian Ltd.
#
################################################################################

cmake_minimum_required( VERSION 2.8.11 )


set( BOOST_TEST_3rd_party_libraries_path "P:/3rdParty" CACHE PATH "VC++ shared" )
set( BOOST_TEST_Boost_version   trunk CACHE PATH   "VC++ shared" )
set( BOOST_TEST_LibTIFF_version 4.0.3 CACHE STRING "VC++ shared" )
set( BOOST_TEST_LibPNG_version  1.6.3 CACHE STRING "VC++ shared" )
set( BOOST_TEST_LibJPEG_version 9a    CACHE STRING "VC++ shared" )

if ( MSVC )
    if ( NOT WIN32 )
        message( SEND_ERROR "LEB: unexpected configuration - MSVC detected but not Win32." )
    endif()

    set( addLibpathSwitch           /LIBPATH: )
    set( ltoCompilerSwitch          /GL       )
    set( ltoLinkerSwitch            /LTCG     )
    set( sizeOptimizationSwitch     /Os       )
    set( speedOptimizationSwitch    /Ot       )
    set( rttiOnSwitch               /GR       )
    set( rttiOffSwitch              /GR-      )
    set( exceptionsOnSwitch         /EHsc     )
    set( exceptionsOffSwitch        ""        )
    set( debugSymbolsCompilerSwitch /Zi       )
    set( debugSymbolsLinkerSwitch   /debug    )

    set( x86CompilerSwitches "/arch:SSE"                                                                )
    set( x86LinkerSwitches   "/SUBSYSTEM:WINDOWS /DYNAMICBASE:NO /INCREMENTAL:NO /LARGEADDRESSAWARE:NO" )

    set(
        CMAKE_CXX_FLAGS
        "/MP /Oi /nologo /errorReport:prompt /W4 /WX-"
        CACHE STRING "VC++ shared" FORCE
    )

    set(
        CMAKE_CXX_FLAGS_DEBUG
        "/MDd /Od /Gm- /fp:precise /EHa /DDEBUG /D_DEBUG ${CMAKE_CXX_FLAGS}"
        CACHE STRING "VC++ debug" FORCE
    )

    set(
        sharedReleaseFlags
        "/Ox /Ob2 /Oy /GF /GR- /Gm- /GS- /Gy /fp:fast /fp:except- ${CMAKE_CXX_FLAGS}"
    )

    set(
        CMAKE_CXX_FLAGS_MINSIZEREL
        "/MD ${sharedReleaseFlags}"
        CACHE STRING "VC++ test release" FORCE
    )

    set(
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        "/MTd ${sharedReleaseFlags}"
        CACHE STRING "VC++ release with asserts" FORCE
    )

    set(
        CMAKE_CXX_FLAGS_RELEASE
        "/MT ${sharedReleaseFlags}"
        CACHE STRING "VC++ release" FORCE
    )

    set(
        CMAKE_SHARED_LINKER_FLAGS
        ""
        CACHE STRING "MSVC++ link shared" FORCE
    )

    set(
        CMAKE_SHARED_LINKER_FLAGS_RELEASE
        "/OPT:REF /OPT:ICF=3"
        CACHE STRING "MSVC++ link release" FORCE
    )

elseif ( CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER MATCHES clang ) #...mrmlj...clean up this GCC-Clang overlap

    set( addLibpathSwitch            -L                     )
    set( ltoCompilerSwitch          "-flto -fwhole-program" )
    set( ltoLinkerSwitch             -flto                  )
    set( rttiOnSwitch                -frtti                 )
    set( rttiOffSwitch               -fno-rtti              )
    set( exceptionsOnSwitch          -fexceptions           )
    set( exceptionsOffSwitch         -fno-exceptions        )
    set( debugSymbolsCompilerSwitch  -g                     )
    set( debugSymbolsLinkerSwitch                           )
    set( speedOptimizationSwitch "-O3 -finline-limit=200 -ftracer" ) #-funroll-loops
    set( sizeOptimizationSwitch  "-Os -ftree-vectorize -ftree-vectorizer-verbose=1 -finline-functions -finline-limit=100 -finline-small-functions -fpartial-inlining -findirect-inlining -falign-functions -falign-jumps -falign-loops -fprefetch-loop-arrays -freorder-blocks -fpredictive-commoning -fgcse-after-reload -fvect-cost-model -ftree-vrp -ftree-partial-pre" )

    set(
        cppSharedFlags
        "-std=gnu++11 -fstrict-aliasing -fconstant-cfstrings -mno-ieee-fp -fno-honor-infinities -fno-honor-nans -fno-math-errno -ffinite-math-only -fobjc-call-cxx-cdtors -fobjc-direct-dispatch -D__NO_MATH_INLINES -fstrict-enums -ffast-math -ffp-contract=fast -fvisibility=hidden -fvisibility-inlines-hidden -fno-threadsafe-statics -fno-common -fno-ident -Wreorder -Winvalid-pch -Wall -Wno-unused-function -Wno-multichar -Wno-unknown-pragmas"
    )

    set(
        cppSharedDebugFlags
        "${cppSharedDebugFlags} -fsanitize=address-full,integer,thread,memory,undefined"
    )

    set(
        cppSharedReleaseFlags
        "-D_FORTIFY_SOURCE=0 -fomit-frame-pointer -ffunction-sections -fdata-sections -fsection-anchors -freorder-blocks-and-partition -fmerge-all-constants -fdelete-null-pointer-checks -fsched-spec-load -fsched2-use-superblocks -fsched-stalled-insns -fmodulo-sched -fsched-pressure -fgcse-sm -fgcse-las -fipa-pta -fweb -frename-registers -fbranch-target-load-optimize2 -fno-enforce-eh-specs -fnothrow-opt -fno-objc-exceptions -mcx16 -mrecip -mno-align-stringops -mno-align-long-strings -minline-all-stringops -mtls-direct-seg-refs"
    )

    set(
        CMAKE_SHARED_LINKER_FLAGS
        ""
        CACHE STRING "g++ link release" FORCE
    )

    set(
        CMAKE_SHARED_LINKER_FLAGS_DEBUG
        "-funwind-tables"
        CACHE STRING "g++ link debug" FORCE
    )

    # OS X (10.8) ld does not seem to support --gc-sections
    # OS X (10.8) ld warns that -s is obsolete and ignored even though it is
    # not and makes a difference
    set(
        CMAKE_SHARED_LINKER_FLAGS_RELEASE
        "-s"
        CACHE STRING "g++ link release" FORCE
    )

    set(
        CMAKE_SHARED_LINKER_FLAGS_RELEASE
        "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -no_eh_labels -dead_strip -dead_strip_dylibs"
        CACHE STRING "Clang link release" FORCE
    )

    set( speedOptimizationSwitch "${speedOptimizationSwitch} -mllvm -unroll-allow-partial"                              )
    set( sizeOptimizationSwitch  "${sizeOptimizationSwitch}  -mllvm -unroll-allow-partial -mllvm -inline-threshold=-20" )

    set( x86CompilerSwitches "-mmmx -mfpmath=both -mllvm -vectorize -mllvm -bb-vectorize-aligned-only" )

    set( CMAKE_XCODE_ATTRIBUTE_PER_ARCH_CFLAGS_i386   "-msse3 -march=prescott -mtune=core2" )
    set( CMAKE_XCODE_ATTRIBUTE_PER_ARCH_CFLAGS_x86_64 "-mssse3 -march=core2 -mtune=corei7"  )

    set( CMAKE_XCODE_ATTRIBUTE_GCC_C_LANGUAGE_STANDARD   GNU11   CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_CXX_LANGUAGE_STANDARD GNU++11 CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_C++_LANGUAGE_STANDARD GNU++11 CACHE STRING "" FORCE )

    set( CMAKE_XCODE_ATTRIBUTE_GCC_AUTO_VECTORIZATION         YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_ENABLE_SSE3_EXTENSIONS     YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN     YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_THREADSAFE_STATICS         NO  CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI_DEBUG      NO  CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_FAST_MATH                  YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_STRICT_ALIASING            YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_GCC_THUMB_SUPPORT              YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_STRIP_INSTALLED_PRODUCT        YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING            YES CACHE STRING "" FORCE )
    set( CMAKE_XCODE_ATTRIBUTE_LLVM_LTO_RELEASE               YES CACHE STRING "" FORCE )

    set(
        CMAKE_CXX_FLAGS
        "${cppSharedFlags}"
        CACHE STRING "g++ shared" FORCE
    )

    set(
        CMAKE_CXX_FLAGS_DEBUG
        "-O0 -DDEBUG=1 -D_DEBUG=1 -fstack-protector-all ${cppSharedFlags} ${cppSharedDebugFlags}"
        CACHE STRING "g++ debug" FORCE
    )

    set(
        CMAKE_CXX_FLAGS_MINSIZEREL
        "${cppSharedFlags} ${cppSharedReleaseFlags}"
        CACHE STRING "g++ test release" FORCE
    )

    set(
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        "${cppSharedFlags} ${cppSharedReleaseFlags}"
        CACHE STRING "g++ release with asserts" FORCE
    )

    set(
        CMAKE_CXX_FLAGS_RELEASE
        "${cppSharedFlags} ${cppSharedReleaseFlags}"
        CACHE STRING "g++ release" FORCE
    )

endif()


set(
    CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} ${debugSymbolsCompilerSwitch}"
    CACHE STRING "compiler debug" FORCE
)


if ( WIN32 )
    set( CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS}           ${x86CompilerSwitches}" )
    set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${x86LinkerSwitches}"   )
endif()


set( disableAsserts NDEBUG )
if ( APPLE )
    set( disableAsserts ${disableAsserts} NS_BLOCK_ASSERTIONS=1 )
endif()
#...mrmlj...GLOBAL does not work but DIRECTORY does...
set_property( DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG          ${enableAsserts}  )
set_property( DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELWITHDEBINFO ${enableAsserts}  )
set_property( DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_MINSIZEREL     ${disableAsserts} )
set_property( DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELEASE        ${disableAsserts} )


set( CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL     ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} CACHE STRING "link minsizerel"     FORCE )
set( CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} CACHE STRING "link relwithdebinfo" FORCE )
set( CMAKE_MODULE_LINKER_FLAGS                ${CMAKE_SHARED_LINKER_FLAGS}         CACHE STRING "link module"         FORCE )
set( CMAKE_MODULE_LINKER_FLAGS_RELEASE        ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} CACHE STRING "link module release" FORCE )
set( CMAKE_EXE_LINKER_FLAGS                   ${CMAKE_SHARED_LINKER_FLAGS}         CACHE STRING "link exe"            FORCE )
set( CMAKE_EXE_LINKER_FLAGS_RELEASE           ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} CACHE STRING "link exe release"    FORCE )


if ( APPLE )
    set( CMAKE_C_FLAGS            "-std=gnu11"                      CACHE STRING "C shared"               FORCE )
else()
    set( CMAKE_C_FLAGS            ""                                CACHE STRING "C shared"               FORCE )
endif()
set( CMAKE_C_FLAGS_DEBUG          ${CMAKE_CXX_FLAGS_DEBUG}          CACHE STRING "C debug"                FORCE )
set( CMAKE_C_FLAGS_MINSIZEREL     ${CMAKE_CXX_FLAGS_MINSIZEREL}     CACHE STRING "C test release"         FORCE )
set( CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} CACHE STRING "C release with asserts" FORCE )
set( CMAKE_C_FLAGS_RELEASE        ${CMAKE_CXX_FLAGS_RELEASE}        CACHE STRING "C release"              FORCE )

add_definitions(
    -DBOOST_MPL_LIMIT_STRING_SIZE=8
)


################################################################################
#   appendProperty()
################################################################################
#http://www.kwwidgets.org/Bug/view.php?id=12342&nbn=2

function( appendProperty target property value )
    get_property( tmp_prop TARGET ${target} PROPERTY ${property} )
    set_property( TARGET ${target} PROPERTY ${property} "${tmp_prop} ${value}" )
endfunction()

function( appendLinkDirectory target targetFlags directory )
    appendProperty( ${target} LINK_FLAGS_${targetFlags} "${addLibpathSwitch}${directory}" )
endfunction()


################################################################################
#   addPCH()
################################################################################
#http://stackoverflow.com/questions/148570/using-pre-compiled-headers-with-cmake
#http://cmake.3232098.n2.nabble.com/Adding-pre-compiled-header-support-easily-add-generated-source-td4589986.html

function( addPCH target pchFile )
    if ( MSVC )
        appendProperty( ${target} COMPILE_FLAGS "/FI\"${pchFile}.hpp\" /Yu\"${pchFile}.hpp\"" )
        set_source_files_properties( "${pchFile}.cpp" PROPERTIES COMPILE_FLAGS "/Yc\"${pchFile}.hpp\"" )
    elseif ( APPLE )
        set_property( TARGET ${target} PROPERTY XCODE_ATTRIBUTE_GCC_PREFIX_HEADER            "${pchFile}.hpp" )
        set_property( TARGET ${target} PROPERTY XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES"            )
    endif()
endfunction()


################################################################################
#   addPrefixHeader()
################################################################################

function( addPrefixHeader target prefixHeaderFile )
    if ( MSVC )
        appendProperty( ${target} COMPILE_FLAGS "/FI\"${prefixHeaderFile}\"" )
    elseif ( APPLE )
        set_property( TARGET ${target} PROPERTY XCODE_ATTRIBUTE_GCC_PREFIX_HEADER            "${prefixHeaderFile}" )
        set_property( TARGET ${target} PROPERTY XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "NO"                  )
    endif()
endfunction()
