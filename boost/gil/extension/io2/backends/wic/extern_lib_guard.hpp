////////////////////////////////////////////////////////////////////////////////
///
/// \file extern_lib_guard.hpp
/// --------------------------
///
/// WIC extern lib guard
///
/// Copyright (c) Domagoj Saric 2010.-2013.
///
///  Use, modification and distribution is subject to the
///  Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef extern_lib_guard_hpp__A28436C0_3FEA_4850_9287_656BAAAD6259
#define extern_lib_guard_hpp__A28436C0_3FEA_4850_9287_656BAAAD6259
#pragma once
//------------------------------------------------------------------------------
#include "boost/gil/extension/io2/backends/detail/extern_lib_guard.hpp"
#include "boost/gil/extension/io2/detail/io_error.hpp"

#include <boost/preprocessor/comparison/greater.hpp>

#include <cstdint>

#if defined( _STDINT ) && !defined( _INTSAFE_H_INCLUDED_ )
    #define _INTSAFE_H_INCLUDED_
#endif
#ifndef COM_NO_WINDOWS_H
    #define COM_NO_WINDOWS_H
#endif // COM_NO_WINDOWS_H
#include "objbase.h"
#include "wincodec.h"
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------
namespace io
{
//------------------------------------------------------------------------------
namespace detail
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
//
//  \class com_scoped_ptr
//  ---------------------
//
////////////////////////////////////////////////////////////////////////////////

template <class COMInterface>
class com_scoped_ptr : noncopyable
{
private:
    class __declspec( novtable ) block_manual_lifetime_management_proxy : public COMInterface
    {
    private:
        ULONG STDMETHODCALLTYPE AddRef ();
        ULONG STDMETHODCALLTYPE Release();
    };

public:
    com_scoped_ptr() : baseCOMPtr_( NULL ) {}
    com_scoped_ptr( COMInterface const * const baseCOMPtr ) : baseCOMPtr_( baseCOMPtr ) { if ( baseCOMPtr  ) baseCOMPtr ->AddRef(); }
    com_scoped_ptr( com_scoped_ptr const & source ) : baseCOMPtr_( source.baseCOMPtr_ ) { if ( baseCOMPtr_ ) baseCOMPtr_->AddRef(); }
    com_scoped_ptr( IUnknown & source )
    {
        // http://msdn.microsoft.com/en-us/library/ms682521(v=vs.85).aspx
        // http://blogs.msdn.com/b/oldnewthing/archive/2004/03/26/96777.aspx
        // http://c2.com/cgi/wiki?QueryInterface
        // http://msdn.microsoft.com/en-us/library/ms810016
        // http://msdn.microsoft.com/en-us/library/dd542643(VS.85).aspx
        // http://en.wikipedia.org/wiki/Catastrophic_failure
        BOOST_VERIFY
        (
            source.QueryInterface
            (
                __uuidof( COMInterface ),
                reinterpret_cast<void * *>( &baseCOMPtr_ )
            ) == S_OK
        );
    }
    ~com_scoped_ptr() { release(); }

    block_manual_lifetime_management_proxy & operator *() const { BOOST_ASSERT( baseCOMPtr_ ); return *static_cast<block_manual_lifetime_management_proxy *>( baseCOMPtr_ ); }
    block_manual_lifetime_management_proxy * operator->() const { return &operator*(); }

    COMInterface * * operator&()
    {
        BOOST_ASSERT( baseCOMPtr_ == NULL );
        return &baseCOMPtr_;
    }

    operator COMInterface *() const { return baseCOMPtr_; }

    void release()
    {
        if ( baseCOMPtr_ )
        {
            baseCOMPtr_->Release();
            baseCOMPtr_ = 0;
        }
    }

    HRESULT create_instance( CLSID const & class_id )
    {
        BOOST_ASSERT( baseCOMPtr_ == NULL );
        return ::CoCreateInstance( class_id, NULL, CLSCTX_INPROC_SERVER, __uuidof( COMInterface ), reinterpret_cast<void * *>( &baseCOMPtr_ ) );
    }

private:
    COMInterface * baseCOMPtr_;
}; // class com_scoped_ptr


////////////////////////////////////////////////////////////////////////////////
///
/// \class wic_factory
///
////////////////////////////////////////////////////////////////////////////////

class wic_factory
{
public:
    static IWICImagingFactory & singleton()
    {
        BOOST_ASSERT_MSG( p_imaging_factory_, "WIC not initialized!" );
        return *p_imaging_factory_;
    }

    class creator : noncopyable
    {
    public:
        creator()
        {
		#if BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_ASSUME
			BOOST_ASSERT_MSG( !wic_factory::p_imaging_factory_, "WIC already initialised." );
		#endif

            HRESULT hr( ::CoInitializeEx( 0, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY ) );
            if ( SUCCEEDED( hr ) )
			{
			#if BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_AUTO
				if ( wic_factory::p_imaging_factory_ )
				{
					static_cast<IWICImagingFactory *>( wic_factory::p_imaging_factory_ )->AddRef();
				}
				else
			#endif // BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_AUTO
				{
					hr = wic_factory::p_imaging_factory_.create_instance( CLSID_WICImagingFactory );
				}
			}

            if ( FAILED( hr ) )
                io_error( "Boost.GIL failed to load external library." ); //...zzz...duplicated...

            BOOST_ASSERT( hr == S_OK                      );
            BOOST_ASSERT( wic_factory::p_imaging_factory_ );
        }

        ~creator()
        {
            wic_factory::p_imaging_factory_.release();
            ::CoUninitialize();
        }
    }; // class creator

private:
    static com_scoped_ptr<IWICImagingFactory> p_imaging_factory_;
}; // class wic_factory

// Implementation note:
//   GCC supports the __declspec( selectany ) declarator for Windows targets
// so I'll avoid adding yet another template and assume that all other Windows
// enabled compilers do too.
//                                            (10.10.2010.) (Domagoj Saric)
__declspec( selectany )
com_scoped_ptr<IWICImagingFactory> wic_factory::p_imaging_factory_;


#if BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_ASSUME

    typedef wic_factory::creator wic_user_guard;

    struct                       wic_base_guard {};

#else // BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_AUTO

    struct                       wic_user_guard {};

    typedef wic_factory::creator wic_base_guard;

#endif // BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_AUTO

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // extern_lib_guard_hpp
