////////////////////////////////////////////////////////////////////////////////
///
/// \file extern_lib_guard.hpp
/// --------------------------
///
/// Copyright (c) Domagoj Saric 2010.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#pragma once
#ifndef extern_lib_guard_hpp__5C845C6C_F8AE_422B_A041_BA78815FCDD3
#define extern_lib_guard_hpp__5C845C6C_F8AE_422B_A041_BA78815FCDD3
//------------------------------------------------------------------------------
#include "extern_lib.hpp"
#include "io_error.hpp"

#include <boost/preprocessor/comparison/greater.hpp>

#include "objbase.h"
#ifdef NOMINMAX
    // ...yuck... hacks needed because of GDIPlus...
    #define max(a, b) (((a) > (b)) ? (a) : (b))
    #define min(a, b) (((a) < (b)) ? (a) : (b))
#endif // NOMINMAX
#include "gdiplus.h"
#ifdef NOMINMAX
    #undef  max
    #undef  min
#endif // NOMINMAX
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------

#ifndef BOOST_GIL_USE_NATIVE_IO

    struct gil_io_lib_guard {};

#else  // BOOST_GIL_USE_NATIVE_IO

    #ifndef BOOST_GIL_EXTERNAL_LIB
        //#define BOOST_GIL_EXTERNAL_LIB DynamicLinkAutoLoadAutoInitialize
        #define BOOST_GIL_EXTERNAL_LIB ( BOOST_LIB_LINK_RUNTIME_AUTO_LOAD, BOOST_LIB_LOADING_RELOADABLE, BOOST_LIB_INIT_AUTO )
    #endif // BOOST_GIL_EXTERNAL_LIB


    #if BOOST_LIB_LINK( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_LINK_LOADTIME_OR_STATIC

        #pragma comment( lib, "gdiplus.lib" )
        //...consider adding specific version support via embedded manifests like this:
        //#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.GdiPlus' version='1.1.6001.22170' processorArchitecture='X86' publicKeyToken='6595b64144ccf1df' language='*'\"" )

        struct gp_guard_base {};

        namespace detail
        {
            typedef dummy_lib_guard gil_io_lib_guard_base;
        }

    #else // BOOST_LIB_LINK( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_LINK_LOADTIME_OR_STATIC

    } // namespace gil

    inline void win32_lib_handle::ensure() const
    {
        gil::io_error_if( !lib_handle(), "Boost.GIL failed to load external library." );
    }

    } // namespace boost

    namespace Gdiplus
    {
    namespace DllExports
    {

    #define ALWAYS 1

    #if GDIPVER >= 0x0110
        #define GP1_1 1
    #else
        #define GP1_1 0
    #endif

    #define GP_FUNCTION( function_name ) Gdiplus::GpStatus, WINGDIPAPI, function_name

    BOOST_DELAYED_EXTERN_LIB_GUARD
    (
        gdiplus.dll,
        gp_guard_base,
        BOOST_GIL_EXTERNAL_LIB,
        (( ALWAYS, Gdiplus::Status, WINAPI, GdiplusStartup     , ( ULONG_PTR * )( const Gdiplus::GdiplusStartupInput * )( Gdiplus::GdiplusStartupOutput * )                                                    ))
        (( ALWAYS, VOID           , WINAPI, GdiplusShutdown    , ( ULONG_PTR )                                                                                                                                 ))
        (( ALWAYS, GP_FUNCTION( GdipCreateBitmapFromStreamICM ), ( IStream * )( Gdiplus::GpBitmap ** )                                                                                                         ))
        (( ALWAYS, GP_FUNCTION( GdipCreateBitmapFromFileICM   ), ( GDIPCONST WCHAR* )( Gdiplus::GpBitmap ** )                                                                                                  ))
        (( ALWAYS, GP_FUNCTION( GdipDisposeImage              ), ( Gdiplus::GpImage * )                                                                                                                        ))
        (( ALWAYS, GP_FUNCTION( GdipGetImageDimension         ), ( Gdiplus::GpImage * )( Gdiplus::REAL * )( Gdiplus::REAL * )                                                                                  ))
        (( ALWAYS, GP_FUNCTION( GdipGetImagePixelFormat       ), ( Gdiplus::GpImage * )( Gdiplus::PixelFormat * )                                                                                              ))
        (( ALWAYS, GP_FUNCTION( GdipBitmapLockBits            ), ( Gdiplus::GpBitmap* )( GDIPCONST Gdiplus::GpRect* )( UINT )( Gdiplus::PixelFormat )( Gdiplus::BitmapData* )                                  ))
        (( ALWAYS, GP_FUNCTION( GdipBitmapUnlockBits          ), ( Gdiplus::GpBitmap* )( Gdiplus::BitmapData* )                                                                                                ))
        (( ALWAYS, GP_FUNCTION( GdipSaveImageToFile           ), ( Gdiplus::GpImage * )( GDIPCONST WCHAR* )( GDIPCONST CLSID* )( GDIPCONST Gdiplus::EncoderParameters* )                                       ))
        (( ALWAYS, GP_FUNCTION( GdipCreateBitmapFromScan0     ), ( INT )( INT )( INT )( Gdiplus::PixelFormat )( BYTE * )( Gdiplus::GpBitmap** )                                                                ))
        (( GP1_1 , GP_FUNCTION( GdipInitializePalette         ), ( OUT Gdiplus::ColorPalette * )( Gdiplus::PaletteType )( INT )( BOOL )( Gdiplus::GpBitmap * )                                                 ))
        (( GP1_1 , GP_FUNCTION( GdipBitmapConvertFormat       ), ( IN Gdiplus::GpBitmap * )( Gdiplus::PixelFormat )( Gdiplus::DitherType )( Gdiplus::PaletteType )( Gdiplus::ColorPalette * )( Gdiplus::REAL ) ))
    );

    #undef ALWAYS
    #undef GP1_1
    #undef GP_FUNCTION

    }

    // ...argh...GDI+ puts all functions in the DllExports namespace (hides the
    // 'flat API') except the following two. The BOOST_DELAYED_EXTERN_LIB_GUARD
    // macro does not support that level of customization so we must trivially
    // reimplement these two here...
    extern "C" Status WINAPI GdiplusStartup 
    (
        OUT ULONG_PTR * const token,
        const GdiplusStartupInput * const input,
        OUT GdiplusStartupOutput * const output
    )
    {
        return DllExports::GdiplusStartup( token, input, output );
    }

    extern "C" VOID WINAPI GdiplusShutdown( ULONG_PTR const token )
    {
        return DllExports::GdiplusShutdown( token );
    }

    }

    namespace boost
    {
    namespace gil
    {
    namespace detail
    {
        typedef Gdiplus::DllExports::gp_guard_base gp_guard_base;

        #if BOOST_LIB_LINK( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_LINK_RUNTIME_ASSUME_LOADED
            // The user/gil_io_lib_guard loads the lib.
            typedef
                BOOST_PP_IF
                (
                    BOOST_PP_GREATER( BOOST_LIB_LOADING( BOOST_GIL_EXTERNAL_LIB ), BOOST_LIB_LOADING_SINGLE ),
                    win32_reloadable_lib_guard,
                    win32_delayloaded_lib_guard
                ) gil_io_lib_guard_base_helper;

            class gil_io_lib_guard_base : gil_io_lib_guard_base_helper
            {
            public:
                template <typename Char>
                gil_io_lib_guard_base( Char const * const lib_name )
                    : gil_io_lib_guard_base_helper( lib_name )
                {
                    #if BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_ASSUME
                    // The user/gil_io_lib_guard inits the lib so it also has to
                    // init the function pointers (required for initialization).
                        gp_guard_base::init_functions( lib_handle() );
                    #endif // BOOST_LIB_INIT_ASSUME
                }
            };

        #else // BOOST_LIB_LINK_RUNTIME_AUTO_LOAD
            // GIL loads the lib automatically every time.
            typedef dummy_lib_guard gil_io_lib_guard_base;

        #endif // BOOST_LIB_LINK( BOOST_GIL_EXTERNAL_LIB )

    } // namespace detail

    #endif // BOOST_LIB_LINK( BOOST_GIL_EXTERNAL_LIB ) != BOOST_LIB_LINK_LOADTIME_OR_STATIC

    namespace detail
    {
        class gp_initialize_guard : noncopyable
        {
        public:
            gp_initialize_guard();
            ~gp_initialize_guard();
        private:
            ULONG_PTR gp_token_;
        };
    } // namespace detail

    class gil_io_lib_guard
        :
        detail::gil_io_lib_guard_base
        #if BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_ASSUME
            ,detail::gp_initialize_guard
        #endif // BOOST_LIB_INIT_ASSUME
    {
    public:
        gil_io_lib_guard() : detail::gil_io_lib_guard_base( L"gdiplus.dll" ) {}
    };

    #undef BOOST_GIL_GUARD_HELPER

#endif // BOOST_GIL_USE_NATIVE_IO

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // extern_lib_guard_hpp
