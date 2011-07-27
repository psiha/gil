////////////////////////////////////////////////////////////////////////////////
///
/// \file device.hpp
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
#ifndef device_hpp__0B0D753B_4DAB_4EAF_8F73_780FF1D1F4D6
#define device_hpp__0B0D753B_4DAB_4EAF_8F73_780FF1D1F4D6
#pragma once
//------------------------------------------------------------------------------
#include "detail/io_error.hpp"

#include "boost/type_traits/is_convertible.hpp"
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

template <typename Handle>
struct input_device : mpl::false_ {};

template <typename Handle>
struct output_device : mpl::false_ {};

template <typename Device>
struct unknown_input_device : is_convertible<input_device<Device>, mpl::false_> {};

template <typename Device>
struct unknown_output_device : is_convertible<output_device<Device>, mpl::false_> {};


namespace detail
{
    struct device_base
    {
        enum seek_origin { beginning = SEEK_SET, current_position = SEEK_CUR, end = SEEK_END };
    };

    struct input_device_base : device_base
    {
        void ensure( bool const condition )
        {
            io_error_if_not( condition, "Boost.GIL.IO failed to open input file." );
        }
    };

    struct output_device_base : device_base
    {
        void ensure( bool const condition )
        {
            io_error_if_not( condition, "Boost.GIL.IO failed to open output file." );
        }
    };
}


//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // device_hpp
