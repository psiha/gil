////////////////////////////////////////////////////////////////////////////////
///
/// \file shared.hpp
/// ----------------
///
/// Common functionality for all GIL::IO backends.
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
#ifndef shared_hpp__DA4F9174_EBAA_43E8_BEDD_A273BBA88CE7
#define shared_hpp__DA4F9174_EBAA_43E8_BEDD_A273BBA88CE7
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

#ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    #ifdef _MSC_VER
        #define BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    #endif
#endif

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // shared_hpp
