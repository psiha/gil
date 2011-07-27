////////////////////////////////////////////////////////////////////////////////
///
/// \file c_file_name.hpp
/// ---------------------
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
#ifndef c_file_name_hpp__328689B4_EC2F_418B_99AC_5DFE988D87AE
#define c_file_name_hpp__328689B4_EC2F_418B_99AC_5DFE988D87AE
#pragma once
//------------------------------------------------------------------------------
#include "c_file_descriptor.hpp"
#include "device.hpp"
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

template <> struct input_device<char const *>
    :
    input_device<c_file_descriptor_t>
{
    input_device( char const * const p_file_name )
        :
        file_descriptor_( input_device<c_file_descriptor_t>::open( p_file_name ) )
    {
        ensure( is_valid( file_descriptor_ ) );
    }

    ~input_device() { input_device<c_file_descriptor_t>::close( file_descriptor_ ); }

    input_device<c_file_descriptor_t>::handle_t transform( char const * /*p_file_name*/ ) const { return file_descriptor_; }

    input_device<c_file_descriptor_t>::handle_t const file_descriptor_;
};


template <> struct output_device<char const *>
    :
    output_device<c_file_descriptor_t>
{
    output_device( char const * const p_file_name )
        :
        file_descriptor_( output_device<c_file_descriptor_t>::open( p_file_name ) )
    {
        ensure( is_valid( file_descriptor_ ) );
    }

    ~output_device() { output_device<c_file_descriptor_t>::close( file_descriptor_ ); }

    output_device<c_file_descriptor_t>::handle_t transform( char const * /*p_file_name*/ ) const { return file_descriptor_; }

    output_device<c_file_descriptor_t>::handle_t const file_descriptor_;
};

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // c_file_name_hpp
