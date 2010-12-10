////////////////////////////////////////////////////////////////////////////////
///
/// \file windows_shared.hpp
/// ------------------------
///
///   Common functionality for MS Windows based backends.
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
#ifndef windows_shared_hpp__9337A434_D2F6_43F2_93C8_4CE66C07B74D
#define windows_shared_hpp__9337A434_D2F6_43F2_93C8_4CE66C07B74D
//------------------------------------------------------------------------------
#include "windows_shared_istreams.hpp"

#include "boost/array.hpp"
#include "boost/assert.hpp"

#include <cstring>
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

////////////////////////////////////////////////////////////////////////////////
///
/// \class wide_path
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
// - GP and WIC want wide-char paths
// - we received a narrow-char path
// - we are using GP or WIC which means we are also using Windows
// - on Windows a narrow-char path can only be up to MAX_PATH in length:
//  http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#maxpath
// - it is therefore safe to use a fixed sized/stack buffer....
//                                            (24.07.2010.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

class wide_path
{
public:
    explicit wide_path( char const * const pFilename )
    {
        BOOST_ASSERT( pFilename );
        BOOST_ASSERT( std::strlen( pFilename ) < wideFileName_.size() );
        char    const * pSource     ( pFilename             );
        wchar_t       * pDestination( wideFileName_.begin() );
        do
        {
            *pDestination++ = *pSource;
        } while ( *pSource++ );
        BOOST_ASSERT( pDestination < wideFileName_.end() );
    }

    operator wchar_t const * () const { return wideFileName_.begin(); }

private:
    boost::array<wchar_t, MAX_PATH> wideFileName_;
};


#pragma warning( push )
#pragma warning( disable : 4355 ) // 'this' used in base member initializer list.

////////////////////////////////////////////////////////////////////////////////
///
/// \class input_FILE_for_IStream_extender
/// \internal
/// \brief Helper wrapper for classes that can construct from IStream objects.
///
////////////////////////////////////////////////////////////////////////////////

template <class IStream_capable_class>
class __declspec( novtable ) input_FILE_for_IStream_extender
    :
    private FileReadStream,
    public  IStream_capable_class
{
public:
    input_FILE_for_IStream_extender( FILE & file )
        :
        FileReadStream       ( file                            ),
        IStream_capable_class( static_cast<IStream &>( *this ) )
    {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class output_FILE_for_IStream_extender
/// \internal
/// \brief Helper wrapper for classes that can construct from IStream objects.
///
////////////////////////////////////////////////////////////////////////////////

template <class IStream_capable_class>
class __declspec( novtable ) output_FILE_for_IStream_extender
    :
    private FileWriteStream,
    public  IStream_capable_class
{
public:
    output_FILE_for_IStream_extender( FILE & file )
        :
        FileWriteStream      ( file                            ),
        IStream_capable_class( static_cast<IStream &>( *this ) )
    {}

    template <typename A2> //...zzz...
    output_FILE_for_IStream_extender( FILE & file, A2 const & a2 )
        :
        FileWriteStream      ( file                                ),
        IStream_capable_class( static_cast<IStream &>( *this ), a2 )
    {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class output_file_name_for_IStream_extender
/// \internal
/// \brief Helper wrapper for classes that can construct from IStream objects.
///
////////////////////////////////////////////////////////////////////////////////

template <class IStream_capable_class>
class __declspec( novtable ) output_file_name_for_IStream_extender
    :
    private FileHandleWriteStream,
    public  IStream_capable_class
{
public:
    output_file_name_for_IStream_extender( wchar_t const * const file_name )
        :
        FileHandleWriteStream( do_open( file_name )            ),
        IStream_capable_class( static_cast<IStream &>( *this ) )
    {}

    template <typename A2> //...zzz...
    output_file_name_for_IStream_extender( wchar_t const * const file_name, A2 const & a2 )
        :
        FileHandleWriteStream( do_open( file_name )                ),
        IStream_capable_class( static_cast<IStream &>( *this ), a2 )
    {}

    ~output_file_name_for_IStream_extender() { BOOST_VERIFY( ::CloseHandle( file_ ) ); }

private:
    static HANDLE do_open( wchar_t const * const file_name )
    {
        HANDLE const handle
        (
            ::CreateFileW
            (
                file_name,
                GENERIC_WRITE,
                0,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                NULL
            )
        );
        io_error_if_not( handle, "File open failure" );
        return handle;
    }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class memory_chunk_for_IStream_extender
/// \internal
/// \brief Helper wrapper for classes that can construct from IStream objects.
///
////////////////////////////////////////////////////////////////////////////////

template <class IStream_capable_class>
class __declspec( novtable ) memory_chunk_for_IStream_extender
    :
    private MemoryReadStream,
    public  IStream_capable_class
{
public:
    memory_chunk_for_IStream_extender( memory_chunk_t const & in_memory_image )
        :
        MemoryReadStream     ( in_memory_image                 ),
        IStream_capable_class( static_cast<IStream &>( *this ) )
    {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class writable_memory_chunk_for_IStream_extender
/// \internal
/// \brief Helper wrapper for classes that can construct from IStream objects.
///
////////////////////////////////////////////////////////////////////////////////

template <class IStream_capable_class>
class __declspec( novtable ) writable_memory_chunk_for_IStream_extender
    :
    private MemoryWriteStream,
    public  IStream_capable_class
{
public:
    writable_memory_chunk_for_IStream_extender( writable_memory_chunk_t const & in_memory_image )
        :
        MemoryWriteStream     ( in_memory_image                ),
        IStream_capable_class( static_cast<IStream &>( *this ) )
    {}
};

#pragma warning( pop )

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // windows_shared_hpp
