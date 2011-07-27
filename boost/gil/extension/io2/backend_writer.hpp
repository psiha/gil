////////////////////////////////////////////////////////////////////////////////
///
/// \file backend_writer.hpp
/// ------------------------
///
/// Base CRTP class for backend writers.
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
#ifndef backend_writer_hpp__D65DA8BE_0605_4A62_AC73_B557F7A94DA6
#define backend_writer_hpp__D65DA8BE_0605_4A62_AC73_B557F7A94DA6
#pragma once
//------------------------------------------------------------------------------
#include "format_tags.hpp"
#include "detail/platform_specifics.hpp"
#include "detail/io_error.hpp"

#include "boost/gil/typedefs.hpp"

#include "boost/static_assert.hpp"
#include "boost/type_traits/is_same.hpp"
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

////////////////////////////////////////////////////////////////////////////////
/// \class formatted_image_traits
/// ( forward declaration )
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
struct formatted_image_traits;


namespace detail
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class backend_writer
///
////////////////////////////////////////////////////////////////////////////////

template <class Backend>
class backend_writer
{
protected:
    typedef typename formatted_image_traits<Backend>::view_data_t view_data_t;

private:
    // MSVC++ (8,9,10) generates code to check whether this == 0.
    typedef typename Backend::native_writer Impl;
    Impl       & impl()       { BF_ASSUME( this != 0 ); return static_cast<Impl       &>( *this ); }
    Impl const & impl() const { BF_ASSUME( this != 0 ); return static_cast<Impl const &>( *this ); }

public: // Utility 'quick-wrappers'...
    template <class Target, class View>
    static void write( Target & target, View const & view )
    {
        typedef typename writer_for<Target>::type writer_t;
        // The backend does not know how to write to the specified target type.
        BOOST_STATIC_ASSERT(( !is_same<writer_t, mpl::void_>::value ));
        writer_t( target, view ).write_default();
    }

    template <class Target, class View>
    static void write( Target * p_target, View const & view )
    {
        typedef typename writer_for<Target const *>::type writer_t;
        // The backend does not know how to write to the specified target type.
        BOOST_STATIC_ASSERT(( !is_same<writer_t, mpl::void_>::value ));
        writer_t( p_target, view ).write_default();
    }

    template <typename char_type, class View>
    static void write( std::basic_string<char_type> const & file_name, View const & view )
    {
        write( file_name.c_str(), view );
    }

    template <typename char_type, class View>
    static void write( std::basic_string<char_type>       & file_name, View const & view )
    {
        write( file_name.c_str(), view );
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
#endif // backend_writer_hpp
