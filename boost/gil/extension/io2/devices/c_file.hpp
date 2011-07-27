////////////////////////////////////////////////////////////////////////////////
///
/// \file c_file.hpp
/// ----------------
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
#ifndef c_file_hpp__BFE0FCB4_C999_4CBF_A897_7075F3CF0E27
#define c_file_hpp__BFE0FCB4_C999_4CBF_A897_7075F3CF0E27
#pragma once
//------------------------------------------------------------------------------
#include "device.hpp"

#include "boost/assert.hpp"
#include "boost/cstdint.hpp"

#include <cstdio>
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

template <>
struct device<FILE *> : detail::device_base
{
    typedef FILE * handle_t;

    static bool const auto_closes = false;

    static handle_t    transform    ( handle_t const handle ) { return handle; }
    static bool        is_valid     ( handle_t const handle ) { return handle != 0; }
    static void        close        ( handle_t const handle ) { BOOST_VERIFY( /*std*/::fclose( handle ) == 0 ); }
    static std::size_t position     ( handle_t const handle ) { return std::ftell( handle ); }
    static uintmax_t   position_long( handle_t const handle )
    {
        #ifdef BOOST_MSVC
            return /*std*/::_ftelli64( handle );
        #else
            return /*std*/::ftell64  ( handle );
        #endif // BOOST_MSVC
    }

    static std::size_t size( handle_t const handle )
    {
        return device<c_file_descriptor_t>::size( /*std*/::_fileno( handle ) );
    }

    static uintmax_t size_long( handle_t const handle )
    {
        return device<c_file_descriptor_t>::size_long( /*std*/::_fileno( handle ) );
    }

    static bool seek( seek_origin const origin, off_t const offset, handle_t const handle )
    {
        return std::fseek( handle, offset, origin ) != 0;
    }

    static bool seek_long( seek_origin const origin, intmax_t const offset, handle_t const handle )
    {
        #ifdef BOOST_MSVC
            return /*std*/::_fseeki64( handle, offset, origin ) != 0;
        #else
            return /*std*/::fseeko   ( handle, offset, origin ) != 0;
        #endif
    }
};


template <>
struct input_device<FILE *>
    :
    detail::input_device_base,
    device<FILE *>
{
    input_device( handle_t /*handle*/ ) {}

    static handle_t open( char const * const file_name )
    {
        return /*std*/::fopen( file_name, "rb" );
    }

    static std::size_t read( void * const p_data, std::size_t const size, handle_t const handle )
    {
        return std::fread( p_data, 1, size, handle );
    }
};


template <>
struct output_device<FILE *>
    :
    detail::output_device_base,
    device<FILE *>
{
    output_device( handle_t /*handle*/ ) {}

    static handle_t open( char const * const file_name )
    {
        return /*std*/::fopen( file_name, "wb" );
    }

    static std::size_t write( void const * const p_data, std::size_t const size, handle_t const handle )
    {
        return std::fwrite( p_data, 1, size,  handle );
    }

    static void flush( handle_t const handle )
    {
        BOOST_VERIFY( std::fflush( handle ) == 0 );
    }
};

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libx_shared_hpp
