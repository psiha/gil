////////////////////////////////////////////////////////////////////////////////
///
/// \file io_error.hpp
/// ------------------
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
#ifndef io_error_hpp__71ED3406_D77C_41FB_9981_A94A1D3BDC8A
#define io_error_hpp__71ED3406_D77C_41FB_9981_A94A1D3BDC8A
//------------------------------------------------------------------------------
#include "platform_specifics.hpp"

#include "boost/throw_exception.hpp"

#include <exception>
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

inline BF_NOTHROWNOALIAS void io_error( char const * const description )
{
    #ifdef _MSC_VER
        throw_exception( std::exception( description , 0 ) ); // Assumes the description string is static/non-temporary
    #else
        throw_exception( std::exception( description     ) );
    #endif // _MSC_VER
}

inline void io_error_if( bool const expression, char const * const description = "" )
{
    if ( expression )
        io_error( description );
}

inline void io_error_if_not( bool const expression, char const * const description = "" )
{
    io_error_if( !expression, description );
}

inline void io_error_if_not( void const * const pointer, char const * const description = "" )
{
    io_error_if( !pointer, description );
}

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // io_error_hpp
