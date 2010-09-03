////////////////////////////////////////////////////////////////////////////////
///
/// \file wic_extern_lib_guard.hpp
/// ------------------------------
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
#ifndef wic_extern_lib_guard_hpp__A28436C0_3FEA_4850_9287_656BAAAD6259
#define wic_extern_lib_guard_hpp__A28436C0_3FEA_4850_9287_656BAAAD6259
//------------------------------------------------------------------------------
#include "extern_lib.hpp"
#include "io_error.hpp"

#include <boost/preprocessor/comparison/greater.hpp>

#define ATLENSURE ATLVERIFY
#include "atlcomcli.h"
#if defined( _STDINT ) && !defined( _INTSAFE_H_INCLUDED_ )
    #define _INTSAFE_H_INCLUDED_
#endif
#include "wincodec.h"
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------
namespace detail
{
//------------------------------------------------------------------------------

template <int dummy = 0>
class wic_factory
{
public:
    static IWICImagingFactory & singleton()
    {
        BOOST_ASSERT( p_imaging_factory_ && "WIC not initialized!" );
        return *p_imaging_factory_;
    }

    class creator : noncopyable
    {
    public:
        creator()
        {
            HRESULT hr( ::CoInitializeEx( 0, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY ) );
            if
            (
                ( ( hr != S_OK ) && ( hr != S_FALSE ) ) ||
                ( ( hr = wic_factory<>::p_imaging_factory_.CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER ) ) != S_OK )
            )
                io_error( "Boost.GIL failed to load external library." ); //...zzz...duplicated...
            BOOST_ASSERT( wic_factory<>::p_imaging_factory_ );
        }
        ~creator()
        {
            wic_factory<>::p_imaging_factory_.Release();
            ::CoUninitialize();
        }
    };

private:
    static CComPtr<IWICImagingFactory> p_imaging_factory_;
};

template<>
CComPtr<IWICImagingFactory> wic_factory<>::p_imaging_factory_;


#if BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_ASSUME

    typedef wic_factory<>::creator wic_user_guard;

    struct                         wic_base_guard {};

#else // BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_AUTO

    struct                         wic_user_guard {};

    typedef wic_factory<>::creator wic_base_guard;

#endif // BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_AUTO

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // wic_extern_lib_guard_hpp
