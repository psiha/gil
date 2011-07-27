////////////////////////////////////////////////////////////////////////////////
///
/// \file reader_for.hpp
/// --------------------
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
#ifndef reader_for_hpp__1733426F_1D8C_4F4E_A966_BDB6426AA88C
#define reader_for_hpp__1733426F_1D8C_4F4E_A966_BDB6426AA88C
#pragma once
//------------------------------------------------------------------------------
#include "boost/gil/extension/io2/format_tags.hpp"
#include "boost/gil/extension/io2/devices/device.hpp"
#include "boost/gil/extension/io2/detail/platform_specifics.hpp"
#include "boost/gil/extension/io2/detail/io_error.hpp"

#include "boost/gil/typedefs.hpp"

#include <boost/static_assert.hpp>
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

namespace detail
{
    template <class Reader, typename Source>
    class reader_extender
        :
        public input_device<Source>,
        public Reader
    {
    public:
        reader_extender( Source const source )
            :
            input_device<Source>( source                                    ),
            Reader              ( input_device<Source>::transform( source ) )
        {}
    };
} // namespace detail


////////////////////////////////////////////////////////////////////////////////
/// \class backend_traits
/// ( forward declaration )
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
struct backend_traits;


template <class Backend, typename Source>
struct reader_for
{
    typedef typename mpl::has_key<typename backend_traits<Backend>::native_sources, Source>::type supported_by_native_reader_t;

    // The backend does not seem to provide a reader for the specified target...
    BOOST_STATIC_ASSERT
    ((
        supported_by_native_reader_t::value ||
        !unknown_input_device<Source>::value
    ));

    typedef typename mpl::if_
    <
        supported_by_native_reader_t,
                                typename Backend::native_reader,
        detail::reader_extender<typename Backend::device_reader, Source>
    >::type type;
};


template <class Backend>
struct reader_for<Backend, char *> : reader_for<Backend, char const *> {};

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // reader_for_hpp
