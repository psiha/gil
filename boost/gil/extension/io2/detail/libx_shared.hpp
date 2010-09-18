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
#include "io_error.hpp"

#include "boost/assert.hpp"
#include "boost/noncopyable.hpp"

#include "crtdefs.h"
#include <cstdio>
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
/// \class generic_vertical_roi
///
////////////////////////////////////////////////////////////////////////////////

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


////////////////////////////////////////////////////////////////////////////////
///
/// \class c_file_guard
///
////////////////////////////////////////////////////////////////////////////////

class c_file_guard : noncopyable
{
public:
    c_file_guard( char const * const file_name, char const * const mode )
        :
        p_file_( /*std*/::fopen( file_name, mode ) )
    {
        io_error_if_not( p_file_, "File open failure" );
    }
    ~c_file_guard() { BOOST_VERIFY( /*std*/::fclose( p_file_ ) == 0 ); }

    FILE & get() const { return *p_file_; }

private:
    FILE * const p_file_;
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class c_file_input_guard
///
////////////////////////////////////////////////////////////////////////////////

class c_file_input_guard : public c_file_guard
{
public:
    c_file_input_guard( char const * const file_name ) : c_file_guard( file_name, "rb" ) {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class c_file_output_guard
///
////////////////////////////////////////////////////////////////////////////////

class c_file_output_guard : public c_file_guard
{
public:
    c_file_output_guard( char const * const file_name ) : c_file_guard( file_name, "wb" ) {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class input_c_str_for_c_file_extender
/// \internal
/// \brief Helper wrapper for classes that can construct from FILE objects.
///
////////////////////////////////////////////////////////////////////////////////

template <class c_file_capable_class>
class input_c_str_for_c_file_extender
    :
    private c_file_input_guard,
    public  c_file_capable_class
{
public:
    input_c_str_for_c_file_extender( char const * const file_path )
        :
        c_file_input_guard  ( file_path           ),
        c_file_capable_class( c_file_guard::get() )
    {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class output_c_str_for_c_file_extender
/// \internal
/// \brief Helper wrapper for classes that can construct from FILE objects.
///
////////////////////////////////////////////////////////////////////////////////

template <class c_file_capable_class>
class output_c_str_for_c_file_extender
    :
    private c_file_output_guard,
    public  c_file_capable_class
{
public:
    output_c_str_for_c_file_extender( char const * const file_path )
        :
        c_file_output_guard ( file_path           ),
        c_file_capable_class( c_file_guard::get() )
    {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class cumulative_result
///
////////////////////////////////////////////////////////////////////////////////

class cumulative_result
{
public:
    cumulative_result() : result_( true ) {}

    void accumulate( bool const new_result ) { result_ &= new_result; }
    template <typename T1, typename T2>
    void accumulate_equal( T1 const new_result, T2 const desired_result ) { accumulate( new_result == desired_result ); }
    template <typename T>
    void accumulate_different( T const new_result, T const undesired_result ) { accumulate( new_result != undesired_result ); }

    void throw_if_error( char const * const description ) const { io_error_if_not( result_, description ); }

    bool & get() { return result_; }

private:
    bool result_;
};


//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libx_shared_hpp
