////////////////////////////////////////////////////////////////////////////////
///
/// \file windows_shared_istreams.hpp
/// ---------------------------------
///
/// Helper IStream implementations for in-memory and FILE base IO.
///
/// Copyright (c) Domagoj Saric 2010.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#pragma once
#ifndef windows_shared_istreams_hpp__A8D022F0_BBFA_4496_8252_8FD1F6A28DF7
#define windows_shared_istreams_hpp__A8D022F0_BBFA_4496_8252_8FD1F6A28DF7
//------------------------------------------------------------------------------
#include "platform_specifics.hpp"

#include "boost/gil/utilities.hpp"

#include "boost/range/iterator_range.hpp"

#include "objbase.h"
#include "objidl.h"
#include "unknwn.h"
//------------------------------------------------------------------------------
namespace boost
{

//...zzz...duplicated from memory_mapping.hpp...clean this up...
typedef iterator_range<unsigned char const *> memory_chunk_t;
typedef iterator_range<unsigned char       *> writable_memory_chunk_t;

//------------------------------------------------------------------------------
namespace gil
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
#ifndef NDEBUG
protected:
    StreamBase() : ref_cnt_( 1 ) {}
    ~StreamBase() { assert( ref_cnt_ == 1 ); }
#endif // NDEBUG

protected:
    static HRESULT not_implemented()
    {
        assert( !"Should not get called!" );
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
        assert( pstatstg );

        assert( grfStatFlag & STATFLAG_NONAME );

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
            HRESULT hresult = Seek( seek_position, SEEK_END, &pstatstg->cbSize ); assert( hresult == S_OK );
                    hresult = Seek( seek_position, SEEK_SET, 0                 ); assert( hresult == S_OK );
            assert( pstatstg->cbSize.QuadPart );
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

    void * operator new   ( size_t );
    void   operator delete( void * );

#ifndef NDEBUG
    int ref_cnt_;
#endif // NDEBUG
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class FileStreamBase
/// ---------------------
///
////////////////////////////////////////////////////////////////////////////////

class __declspec( novtable ) FileStreamBase : public StreamBase
{
protected:
    FileStreamBase( FILE & file ) : file_( file ) {}

private:
    HRESULT STDMETHODCALLTYPE Seek( LARGE_INTEGER const dlibMove, DWORD const dwOrigin, ULARGE_INTEGER * const plibNewPosition ) override
    {
        bool const x64( sizeof( long ) == sizeof( dlibMove.QuadPart ) );
        assert( x64 || ( dlibMove.HighPart == 0 ) || ( dlibMove.HighPart == -1 ) );
        long const offset( x64 ? static_cast<long>( dlibMove.QuadPart ) : dlibMove.LowPart );

        BOOST_STATIC_ASSERT( SEEK_SET == STREAM_SEEK_SET );
        BOOST_STATIC_ASSERT( SEEK_CUR == STREAM_SEEK_CUR );
        BOOST_STATIC_ASSERT( SEEK_END == STREAM_SEEK_END );

        assert( ( dwOrigin >= SEEK_SET ) && ( dwOrigin <= SEEK_END ) );

        int const result
        (
            x64
                ? /*std*/::_fseeki64( &file_, offset, dwOrigin )
                : /*std*/::fseek    ( &file_, offset, dwOrigin )
        );
        if ( plibNewPosition )
            plibNewPosition->QuadPart =
                x64
                    ? /*std*/::_ftelli64( &file_ )
                    : /*std*/::ftell    ( &file_ );

        bool const success( result == 0 );
        return success ? S_OK : S_FALSE;
    }

protected:
    FILE & file_;
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class FileReadStream
/// ---------------------
///
////////////////////////////////////////////////////////////////////////////////

class FileReadStream : public FileStreamBase
{
public:
    FileReadStream( FILE & file ) : FileStreamBase( file ) {}

private:
    HRESULT STDMETHODCALLTYPE Read( void * const pv, ULONG const cb, ULONG * const pcbRead ) override
    {
        assert( pv );
        size_t const size_read( /*std*/::fread( pv, 1, cb, &file_ ) );
        if ( pcbRead )
            *pcbRead = size_read;
        return ( size_read == cb ) ? S_OK : S_FALSE;
    }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class FileWriteStream
/// ----------------------
///
////////////////////////////////////////////////////////////////////////////////

class FileWriteStream : public FileStreamBase
{
public:
    FileWriteStream( FILE & file ) : FileStreamBase( file ) {}

private:
    HRESULT STDMETHODCALLTYPE Write( void const * const pv, ULONG const cb, ULONG * const pcbWritten ) override
    {
        assert( pv );
        size_t const size_written( /*std*/::fwrite( pv, 1, cb, &file_ ) );
        if ( pcbWritten )
            *pcbWritten = size_written;
        return ( size_written == cb ) ? S_OK : S_FALSE;
    }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class FileHandleStreamBase
/// ---------------------------
///
////////////////////////////////////////////////////////////////////////////////

class __declspec( novtable ) FileHandleStreamBase : public StreamBase
{
protected:
    FileHandleStreamBase( HANDLE const file ) : file_( file ) {}

private:
    HRESULT STDMETHODCALLTYPE Seek( LARGE_INTEGER const dlibMove, DWORD const dwOrigin, ULARGE_INTEGER * const plibNewPosition ) override
    {
        BOOST_STATIC_ASSERT( SEEK_SET == STREAM_SEEK_SET );
        BOOST_STATIC_ASSERT( SEEK_CUR == STREAM_SEEK_CUR );
        BOOST_STATIC_ASSERT( SEEK_END == STREAM_SEEK_END );

        assert( ( dwOrigin >= SEEK_SET ) && ( dwOrigin <= SEEK_END ) );

        BOOL const success( ::SetFilePointerEx( file_, dlibMove, reinterpret_cast<PLARGE_INTEGER>( plibNewPosition ), dwOrigin ) );
        return success ? S_OK : S_FALSE;
    }

protected:
    HANDLE const file_;
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class FileHandleReadStream
/// ---------------------------
///
////////////////////////////////////////////////////////////////////////////////

class FileHandleReadStream : public FileHandleStreamBase
{
public:
    FileHandleReadStream( HANDLE const file ) : FileHandleStreamBase( file ) {}

private:
    HRESULT STDMETHODCALLTYPE Read( void * const pv, ULONG const cb, ULONG * const pcbRead ) override
    {
        BOOL const success( ::ReadFile( file_, pv, cb, pcbRead, NULL ) );
        return success ? S_OK : S_FALSE;
    }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class FileWriteStream
/// ----------------------
///
////////////////////////////////////////////////////////////////////////////////

class FileHandleWriteStream : public FileHandleStreamBase
{
public:
    FileHandleWriteStream( HANDLE const file ) : FileHandleStreamBase( file ) {}

private:
    HRESULT STDMETHODCALLTYPE Write( void const * const pv, ULONG const cb, ULONG * const pcbWritten ) override
    {
        BOOL const success( ::WriteFile( file_, pv, cb, pcbWritten, NULL ) );
        return success ? S_OK : S_FALSE;
    }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class MemoryStreamBase
/// -----------------------
///
////////////////////////////////////////////////////////////////////////////////

class __declspec( novtable ) MemoryStreamBase : public StreamBase
{
protected:
    MemoryStreamBase( writable_memory_chunk_t const & in_memory_image )
        :
        pCurrentPosition_( in_memory_image.begin() ),
        memory_chunk_    ( in_memory_image         )
    {
        assert( in_memory_image );
    }

#ifndef NDEBUG
    ~MemoryStreamBase() { assert( pCurrentPosition_ >= memory_chunk_.begin() && pCurrentPosition_ <= memory_chunk_.end() ); }
#endif // NDEBUG

    std::size_t adjust_size_for_remaining_space( std::size_t const size ) const
    {
        return std::min<size_t>( size, memory_chunk_.end() - pCurrentPosition_ );
    }

private:
    HRESULT STDMETHODCALLTYPE Seek( LARGE_INTEGER const dlibMove, DWORD const dwOrigin, ULARGE_INTEGER * const plibNewPosition ) override
    {
        bool const x64( sizeof( long ) == sizeof( dlibMove.QuadPart ) );
        assert( x64 || ( dlibMove.HighPart == 0 ) || ( dlibMove.HighPart == -1 ) );
        long const offset( x64 ? static_cast<long>( dlibMove.QuadPart ) : dlibMove.LowPart );

        unsigned char * const pOrigin( pointer_for_origin( dwOrigin ) );
        unsigned char * const pTarget( pOrigin + offset );
        bool const success( pTarget >= memory_chunk_.begin() && pTarget <= memory_chunk_.end() );
        if ( success )
            pCurrentPosition_ = pTarget;

        if ( plibNewPosition )
            plibNewPosition->QuadPart = pCurrentPosition_ - memory_chunk_.begin();

        return success ? S_OK : S_FALSE;
    }

    unsigned char * pointer_for_origin( DWORD const origin )
    {
        switch ( static_cast<STREAM_SEEK>( origin ) )
        {
            case STREAM_SEEK_SET: return memory_chunk_.begin();
            case STREAM_SEEK_CUR: return pCurrentPosition_    ;
            case STREAM_SEEK_END: return memory_chunk_.end  ();
            default: BF_UNREACHABLE_CODE
        }
    }

protected:
    unsigned char           *       pCurrentPosition_;
    writable_memory_chunk_t   const memory_chunk_    ;
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class MemoryReadStream
/// -----------------------
///
////////////////////////////////////////////////////////////////////////////////

class MemoryReadStream : public MemoryStreamBase
{
public:
    MemoryReadStream( memory_chunk_t const & in_memory_image )
        :
        MemoryStreamBase( *gil_reinterpret_cast_c<writable_memory_chunk_t const *>( &in_memory_image ) ) {}

private:
    HRESULT STDMETHODCALLTYPE Read( void * const pv, ULONG const cb, ULONG * const pcbRead ) override
    {
        assert( pv );
        size_t const size_read( adjust_size_for_remaining_space( cb ) );
        std::memcpy( pv, pCurrentPosition_, size_read );
        pCurrentPosition_ += size_read;
        if ( pcbRead )
            *pcbRead = size_read;
        return ( size_read == cb ) ? S_OK : S_FALSE;
    }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class MemoryWriteStream
/// ------------------------
///
////////////////////////////////////////////////////////////////////////////////

class MemoryWriteStream : public MemoryStreamBase
{
public:
    MemoryWriteStream( writable_memory_chunk_t const & in_memory_image )
        :
        MemoryStreamBase( in_memory_image ) {}

private:
    HRESULT STDMETHODCALLTYPE Write( void const * const pv, ULONG const cb, ULONG * const pcbWritten ) override
    {
        assert( pv );
        size_t const size_written( adjust_size_for_remaining_space( cb ) );
        std::memcpy( pCurrentPosition_, pv, size_written );
        pCurrentPosition_ += size_written;
        if ( pcbWritten )
            *pcbWritten = size_written;
        return ( size_written == cb ) ? S_OK : S_FALSE;
    }
};


#pragma warning( pop )

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // windows_shared_istreams_hpp
