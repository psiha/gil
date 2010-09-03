////////////////////////////////////////////////////////////////////////////////
///
/// \file libx_shared.hpp
/// ---------------------
///
/// Common functionality for LibPNG, LibJPEG and LibTIFF backends.
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
#ifndef libx_shared_hpp__ABC7759A_4313_4BFF_B64F_D72BBF2355E8
#define libx_shared_hpp__ABC7759A_4313_4BFF_B64F_D72BBF2355E8
//------------------------------------------------------------------------------
#include "../../../utilities.hpp"

#include "crtdefs.h"
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

class generic_vertical_roi
{
public:
    typedef std::ptrdiff_t     value_type;
    typedef point2<value_type> point_t   ;

    typedef value_type         offset_t  ;

public:
    generic_vertical_roi( value_type const start_row, value_type const end_row )
        :
        start_row_( start_row ),
        end_row_  ( end_row   )
    {}

    value_type start_row() const { return start_row_; }
    value_type end_row  () const { return end_row_  ; }

private:
    void operator=( generic_vertical_roi const & );

private:
    value_type const start_row_;
    value_type const   end_row_;
};


class c_file_guard
{
public:
    c_file_guard( char const * const file_name )
        :
        p_file_( /*std*/::fopen( file_name, "rb" ) )
    {
        io_error_if( !p_file_, "File open failure" );
    }
    ~c_file_guard() { /*std*/::fclose( p_file_ ); }

    FILE & get() const { return *p_file_; }

private:
    FILE * const p_file_;
};

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libx_shared_hpp
