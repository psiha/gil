////////////////////////////////////////////////////////////////////////////////
///
/// \file windows_shared_istreams.hpp
/// ---------------------------------
///
/// Windows IStream implementation(s) for the GIL.IO2 device concept.
///
/// Copyright (c) Domagoj Saric 2010.-2011.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef windows_shared_istreams_hpp__A8D022F0_BBFA_4496_8252_8FD1F6A28DF7
#define windows_shared_istreams_hpp__A8D022F0_BBFA_4496_8252_8FD1F6A28DF7
#pragma once
//------------------------------------------------------------------------------
#include "platform_specifics.hpp"
#include "boost/gil/extension/io2/devices/device.hpp"

#include "boost/gil/utilities.hpp"

#include "boost/range/iterator_range.hpp"

#ifndef COM_NO_WINDOWS_H
    #define COM_NO_WINDOWS_H
#endif // COM_NO_WINDOWS_H
#include "objbase.h"
#include "objidl.h"
#include "unknwn.h"
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

#pragma warning( push )
#pragma warning( disable: 4373 )  // previous versions of the compiler did not override when parameters only differed by const/volatile qualifiers
#pragma warning( disable: 4481 )  // nonstandard extension used: override specifier 'override'

////////////////////////////////////////////////////////////////////////////////
///
/// \class StreamBase
/// -----------------
///
////////////////////////////////////////////////////////////////////////////////

class __declspec( novtable ) StreamBase : public IStream, noncopyable
{
public:
    static void * operator new( std::size_t, void * const p_placeholder ) { return p_placeholder; }

#ifndef NDEBUG
protected:
     StreamBase() : ref_cnt_( 1 ) {}
    ~StreamBase() { BOOST_ASSERT( ref_cnt_ == 1 ); }
#endif // NDEBUG

protected:
    static HRESULT not_implemented()
    {
        BOOST_ASSERT( !"Should not get called!" );
        return E_NOTIMPL;
    }

private: // Not implemented/not required methods.
    HRESULT STDMETHODCALLTYPE SetSize       ( ULARGE_INTEGER /*libNewSize*/                                                                            ) override { return not_implemented(); }
    HRESULT STDMETHODCALLTYPE CopyTo        ( IStream * /*pstm*/, ULARGE_INTEGER /*cb*/, ULARGE_INTEGER * /*pcbRead*/, ULARGE_INTEGER * /*pcbWritten*/ ) override { return not_implemented(); }
    HRESULT STDMETHODCALLTYPE Commit        ( DWORD /*grfCommitFlags*/                                                                                 ) override { return not_implemented(); }
    HRESULT STDMETHODCALLTYPE Revert        (                                                                                                          ) override { return not_implemented(); }
    HRESULT STDMETHODCALLTYPE LockRegion    ( ULARGE_INTEGER /*libOffset*/, ULARGE_INTEGER /*cb*/, DWORD /*dwLockType*/                                ) override { return not_implemented(); }
    HRESULT STDMETHODCALLTYPE UnlockRegion  ( ULARGE_INTEGER /*libOffset*/, ULARGE_INTEGER /*cb*/, DWORD /*dwLockType*/                                ) override { return not_implemented(); }
    HRESULT STDMETHODCALLTYPE Clone         ( IStream ** /*ppstm*/                                                                                     ) override { return not_implemented(); }
    //HRESULT STDMETHODCALLTYPE QueryInterface( REFIID /*iid*/, void ** /*ppvObject*/                                                                    ) override { return not_implemented(); }

private: // Not implemented or 'possibly' implemented by derived classes.
    HRESULT STDMETHODCALLTYPE Read          ( void       * /*pv*/, ULONG /*cb*/, ULONG * /*pcbRead*/                                                   ) override { return not_implemented(); }
    HRESULT STDMETHODCALLTYPE Write         ( void const * /*pv*/, ULONG /*cb*/, ULONG * /*pcbWritten*/                                                ) override { return not_implemented(); }

private:
    HRESULT STDMETHODCALLTYPE Stat( STATSTG * const pstatstg, DWORD const grfStatFlag ) override
    {
        BOOST_ASSERT( pstatstg );

        BOOST_ASSERT( grfStatFlag & STATFLAG_NONAME );

        std::memset( pstatstg, 0, sizeof( *pstatstg ) );

        if ( !( grfStatFlag & STATFLAG_NONAME ) )
        {
            static OLECHAR const name[] = OLESTR( "Boost.GIL" );
            pstatstg->pwcsName = static_cast<OLECHAR *>( ::CoTaskMemAlloc( sizeof( name ) ) );
            if ( !pstatstg->pwcsName )
                return STG_E_INSUFFICIENTMEMORY;
            std::memcpy( pstatstg->pwcsName, name, sizeof( name ) );
        }

        pstatstg->type = STGTY_STREAM;

        {
            LARGE_INTEGER const seek_position = { 0, 0 };
            HRESULT hresult = Seek( seek_position, SEEK_END, &pstatstg->cbSize ); BOOST_ASSERT( hresult == S_OK );
                    hresult = Seek( seek_position, SEEK_SET, 0                 ); BOOST_ASSERT( hresult == S_OK );
            BOOST_ASSERT( pstatstg->cbSize.QuadPart );
        }

        //pstatstg->grfMode : http://msdn.microsoft.com/en-us/library/ms891273.aspx

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void ** ppvObject ) override
    {
        BOOST_ASSERT( ppvObject );
        BOOST_ASSERT( iid != __uuidof( IUnknown          ) );
        BOOST_ASSERT( iid != __uuidof( IStream           ) );
        BOOST_ASSERT( iid != __uuidof( ISequentialStream ) );

        ignore_unused_variable_warning( iid       );
        ignore_unused_variable_warning( ppvObject );

        return E_NOINTERFACE; 
    }


private: // Dummy reference counting for stack based objects.
    ULONG STDMETHODCALLTYPE AddRef () override
    {
        #ifdef NDEBUG
            return 0;
        #else
            BOOST_ASSERT( ref_cnt_ > 0 );
            return ++ref_cnt_;
        #endif // NDEBUG
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        #ifdef NDEBUG
            return 0;
        #else
            BOOST_ASSERT( ref_cnt_ > 1 );
            return --ref_cnt_;
        #endif // NDEBUG
    }

    static void * operator new   ( std::size_t );
    //...zzz...because of placement new...static void   operator delete( void *      );

#ifndef NDEBUG
    int ref_cnt_;
#endif // NDEBUG
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class device_stream_base
/// -------------------------
///
////////////////////////////////////////////////////////////////////////////////

template <class Device>
class __declspec( novtable ) device_stream_base : public StreamBase
{
protected:
    device_stream_base( typename Device::handle_t const handle ) : handle_( handle ) {}

private:
    HRESULT STDMETHODCALLTYPE Seek( LARGE_INTEGER const dlibMove, DWORD const dwOrigin, ULARGE_INTEGER * const plibNewPosition ) override
    {
        BOOST_STATIC_ASSERT( detail::device_base::beginning        == STREAM_SEEK_SET );
        BOOST_STATIC_ASSERT( detail::device_base::current_position == STREAM_SEEK_CUR );
        BOOST_STATIC_ASSERT( detail::device_base::end              == STREAM_SEEK_END );

        BOOST_ASSERT( ( dwOrigin >= STREAM_SEEK_SET ) && ( dwOrigin <= STREAM_SEEK_END ) );

        intmax_t const offset( static_cast<intmax_t>( dlibMove.QuadPart ) );

        bool const success( Device::seek_long( static_cast<detail::device_base::seek_origin>( dwOrigin ), offset, handle_ ) );

        if ( plibNewPosition )
            plibNewPosition->QuadPart = Device::position_long( handle_ );

        return success ? S_OK : S_FALSE;
    }

protected:
    typename Device::handle_t const handle_;
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class input_device_stream
/// --------------------------
///
////////////////////////////////////////////////////////////////////////////////

template <class DeviceHandle>
class input_device_stream : public device_stream_base<input_device<DeviceHandle> >
{
public:
    input_device_stream( DeviceHandle const handle ) : device_stream_base( file ) {}

private:
    HRESULT STDMETHODCALLTYPE Read( void * const pv, ULONG const cb, ULONG * const pcbRead ) override
    {
        BOOST_ASSERT( pv );
        std::size_t const size_read( input_device<DeviceHandle>::read( pv, cb, handle_ ) );
        if ( pcbRead )
            *pcbRead = size_read;
        return ( size_read == cb ) ? S_OK : S_FALSE;
    }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class output_device_stream
/// ---------------------------
///
////////////////////////////////////////////////////////////////////////////////

template <class DeviceHandle>
class output_device_stream : public device_stream_base<output_device<DeviceHandle> >
{
public:
    output_device_stream( DeviceHandle const handle ) : device_stream_base( handle ) {}

private:
    HRESULT STDMETHODCALLTYPE Write( void const * const pv, ULONG const cb, ULONG * const pcbWritten ) override
    {
        BOOST_ASSERT( pv );
        std::size_t const size_written( output_device<DeviceHandle>::write( pv, cb, handle_ ) );
        if ( pcbWritten )
            *pcbWritten = size_written;
        return ( size_written == cb ) ? S_OK : S_FALSE;
    }
};

#pragma warning( pop )


////////////////////////////////////////////////////////////////////////////////
///
/// \class device_stream_wrapper
///
////////////////////////////////////////////////////////////////////////////////

struct __declspec( novtable ) istream_placeholder_helper : private StreamBase { void * handle; };
typedef aligned_storage<sizeof( istream_placeholder_helper ), sizeof( void *)> istream_placeholder;

#ifdef NDEBUG
    BOOST_STATIC_ASSERT( sizeof( istream_placeholder ) <= sizeof( void * ) * 2 /* vtable + handle */ );
#endif

template <template <typename Handle> class IODeviceStream, class BackendWriter>
class device_stream_wrapper
    :
    private istream_placeholder,
    public  BackendWriter
{
public:
    template <class DeviceHandle>
    device_stream_wrapper( DeviceHandle const handle )
        :
        BackendWriter( *( new ( istream_placeholder::address() ) IODeviceStream<DeviceHandle>( handle ) ) )
    {
        BOOST_STATIC_ASSERT( sizeof( IODeviceStream<DeviceHandle> ) <= sizeof( istream_placeholder ) );
    }

    template <class DeviceHandle, typename SecondParameter>
    device_stream_wrapper( DeviceHandle const handle, SecondParameter const & second_parameter )
        :
        BackendWriter
        (
            *( new ( istream_placeholder::address() ) IODeviceStream<DeviceHandle>( handle ) ),
            second_parameter
        )
    {
        BOOST_STATIC_ASSERT( sizeof( IODeviceStream<DeviceHandle> ) <= sizeof( istream_placeholder ) );
    }
};

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // windows_shared_istreams_hpp
