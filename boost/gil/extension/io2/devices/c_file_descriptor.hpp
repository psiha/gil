////////////////////////////////////////////////////////////////////////////////
///
/// \file c_file_descriptor.hpp
/// ---------------------------
///
/// Copyright (c) Domagoj Saric 2011.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef c_file_descriptor_hpp__6569A70D_9A8D_41EB_8152_F8D87E85D1E3
#define c_file_descriptor_hpp__6569A70D_9A8D_41EB_8152_F8D87E85D1E3
#pragma once
//------------------------------------------------------------------------------
#include "device.hpp"

#include "boost/assert.hpp"
#include "boost/cstdint.hpp"

#ifdef BOOST_HAS_UNISTD_H
    #include "sys/stat.h"
    #include "unistd.h"
#else
    #ifndef _POSIX_
        #define _POSIX_
    #endif
    #include "io.h"
#endif // BOOST_HAS_UNISTD_H
#include "fcntl.h"
#include "sys/stat.h"
#include "sys/types.h"
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

typedef int c_file_descriptor_t;

template <>
struct device<c_file_descriptor_t> : detail::device_base
{
    typedef c_file_descriptor_t handle_t;

    static bool const auto_closes = false;

    static bool is_valid( handle_t const handle )
    {
        return handle >= 0;
    }

    static void close( handle_t const handle ) { BOOST_VERIFY( /*std*/::close( handle ) == 0 ); }

    static std::size_t position( handle_t const handle )
    {
        return /*std*/::tell( handle );
    }

    static uintmax_t position_long( handle_t const handle )
    {
        #ifdef BOOST_MSVC
            return /*std*/::_telli64( handle );
        #else
            return /*std*/::tell64  ( handle );
        #endif // BOOST_MSVC
    }

    static std::size_t size( handle_t const handle )
    {
        #ifdef BOOST_MSVC
            return /*std*/::_filelength( handle );
        #else
            struct stat file_status;
            BOOST_VERIFY( ::fstat( handle, &file_status ) == 0 );
            return file_status.st_size;
        #endif // BOOST_MSVC
    }

    static uintmax_t size_long( handle_t const handle )
    {
        #ifdef BOOST_MSVC
            return /*std*/::_filelengthi64( handle );
        #else
            struct stat64 file_status;
            BOOST_VERIFY( ::fstat64( handle, &file_status ) == 0 );
            return file_status.st_size;
        #endif // BOOST_MSVC
    }

    static bool seek( seek_origin const origin, off_t offset, handle_t const handle )
    {
        return /*std*/::lseek( handle, offset, origin ) != 0;
    }

    static bool seek_long( seek_origin const origin, intmax_t offset, handle_t const handle )
    {
        #ifdef BOOST_MSVC
            return /*std*/::_lseeki64( handle, offset, origin ) != 0;
        #else
            return /*std*/::lseeko   ( handle, offset, origin ) != 0;
        #endif
    }
};


template <>
struct input_device<c_file_descriptor_t>
    :
    detail::input_device_base,
    device<c_file_descriptor_t>
{
    static handle_t open( char const * const p_file_name )
    {
        return /*std*/::open( p_file_name, BF_MSVC_SPECIFIC( O_BINARY | ) O_RDONLY, 0 );
    }

    static std::size_t read( void * p_data, std::size_t const size, handle_t const handle )
    {
        int const result( /*std*/::read( handle, p_data, size ) );
        return ( result >= 0 ) ? result : 0;
    }
};


template <>
struct output_device<c_file_descriptor_t>
    :
    detail::output_device_base,
    device<c_file_descriptor_t>
{
    static handle_t open( char const * const p_file_name )
    {
        return /*std*/::open( p_file_name, BF_MSVC_SPECIFIC( O_BINARY | ) O_CREAT | O_WRONLY, S_IREAD | S_IWRITE );
    }

    static std::size_t write( void const * p_data, std::size_t const size, handle_t const handle )
    {
        int const result( /*std*/::write( handle, p_data, size ) );
        return ( result >= 0 ) ? result : 0;
    }

    static void flush( handle_t const handle )
    {
        #ifdef BOOST_MSVC
            BOOST_VERIFY( /*std*/::_commit( handle ) == 0 );
        #else
            BOOST_VERIFY( /*std*/::fsync  ( handle ) == 0 );
        #endif
    }
};

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // c_file_descriptor_hpp
