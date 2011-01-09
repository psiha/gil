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
#include "io_error.hpp"
#include "memory_mapping.hpp"
#include "boost/gil/utilities.hpp"

#include "boost/assert.hpp"
#include "boost/noncopyable.hpp"

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
    explicit c_file_input_guard( char const * const file_name ) : c_file_guard( file_name, "rb" ) {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class c_file_output_guard
///
////////////////////////////////////////////////////////////////////////////////

class c_file_output_guard : public c_file_guard
{
public:
    explicit c_file_output_guard( char const * const file_name ) : c_file_guard( file_name, "wb" ) {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class mapped_input_file_guard
///
////////////////////////////////////////////////////////////////////////////////

class mapped_input_file_guard : private memory_mapped_source
{
public:
    explicit mapped_input_file_guard( char const * const file_name )
        :
        memory_mapped_source( map_read_only_file( file_name )      ),
        mutable_range_      ( memory_mapped_source::memory_range() )
    {
        io_error_if( memory_range().empty(), "File open failure" );
    }

    ~mapped_input_file_guard()
    {
        // Verify that the image class did not 'walk outside' the mapped range.
        BOOST_ASSERT( mutable_range_.begin() <= mutable_range_.end  () );
        BOOST_ASSERT( mutable_range_.begin() >= memory_range().begin() );
        BOOST_ASSERT( mutable_range_.end  () <= memory_range().end  () );
    }

    memory_chunk_t & get() { return mutable_range_; }

private:
    memory_chunk_t mutable_range_;
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

    template <typename A2> //...zzz...
    output_c_str_for_c_file_extender( char const * const file_path, A2 const & a2 )
        :
        c_file_output_guard ( file_path               ),
        c_file_capable_class( c_file_guard::get(), a2 )
    {}
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class input_c_str_for_mmap_extender
/// \internal
/// \brief Helper wrapper for classes that can construct from memory_chunk_t
/// objects.
///
////////////////////////////////////////////////////////////////////////////////
//...zzz...add another layer of utility templates to kill this duplication...
template <class mmap_capable_class>
class input_c_str_for_mmap_extender
    :
    private mapped_input_file_guard,
    public  mmap_capable_class
{
public:
    input_c_str_for_mmap_extender( char const * const file_path )
        :
        mapped_input_file_guard( file_path                      ),
        mmap_capable_class     ( mapped_input_file_guard::get() )
    {}
};



////////////////////////////////////////////////////////////////////////////////
///
/// \class seekable_input_memory_range_extender
///
////////////////////////////////////////////////////////////////////////////////

template <class in_memory_capable_class>
class seekable_input_memory_range_extender
    :
    private memory_chunk_t,
    public  in_memory_capable_class
{
public:
    explicit seekable_input_memory_range_extender( memory_chunk_t const & memory_range )
        :
        memory_chunk_t         ( memory_range                           ),
        in_memory_capable_class( static_cast<memory_chunk_t &>( *this ) )
    {}
};

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libx_shared_hpp
