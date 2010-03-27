////////////////////////////////////////////////////////////////////////////////
///
/// \file gp_private_png_io.ipp
/// ---------------------------
///
/// Base PNG IO interface GDI+ implementation.
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
#include "gp_private_io.hpp"
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------

template <typename Source>
inline point2<std::ptrdiff_t> png_read_dimensions( Source const & source ) {
    return detail::read_dimensions( source );
}


template <typename View, typename Source>
inline void png_read_view( Source const & source, View const & view ) {
    detail::read_view( source, view );
}


template <typename Image, typename Source>
inline void png_read_image( Source const & source, Image & gil_image ) {
    detail::read_image( source, gil_image );
}


template <typename View,typename CC, typename Source>
inline void png_read_and_convert_view( Source const & source, View const & view, CC cc ) {
    detail::read_and_convert_view( source, view, cc );
}


template <typename Image,typename CC, typename Source>
inline void png_read_and_convert_image( Source const & source, Image & gil_image, CC cc ) {
    detail::read_and_convert_image( source, gil_image, cc );
}

template <typename Image, typename Source>
inline void png_read_and_convert_image( Source const & source, Image & gil_image ) {
    detail::read_and_convert_image( source, gil_image );
}


//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
