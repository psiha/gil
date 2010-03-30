////////////////////////////////////////////////////////////////////////////////
///
/// \file gp_private_io.hpp
/// -----------------------
///
/// Base IO interface GDI+ implementation.
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
#ifndef gp_private_io_hpp__7BCE38D9_64FA_4C63_8BAE_FE220717DBB9
#define gp_private_io_hpp__7BCE38D9_64FA_4C63_8BAE_FE220717DBB9
//------------------------------------------------------------------------------
#include "gp_private_base.hpp"
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

inline point2<std::ptrdiff_t> read_dimensions( detail::gp_image const & image ) {
    return image.dimensions();
}

inline point2<std::ptrdiff_t> read_dimensions( char const * const filename ) {
    return read_dimensions( gp_image( filename ) );
}


template <typename View>
inline void read_view( gp_image const & image, View const & view ) {
    image.copy_to_view( view );
}

template <typename View>
inline void read_view( char const * const filename, View const & view ) {
    read_view( gp_image( filename ), view );
}


template <typename Image>
inline void read_image( gp_image const & gp_image, Image & gil_image ) {
    gil_image.recreate( gp_image.get_dimensions(), sizeof( Gdiplus::ARGB ) );
    gp_image.copy_to_prepared_view( view( gil_image ) );
}

template <typename Image>
inline void read_image(const char* filename,Image& im) {
    read_image( gp_image( filename ), im );
}

template <typename Image>
inline void read_image( wchar_t const * const filename, Image & im ) {
    read_image( gp_image( filename ), im );
}


template <typename View,typename CC>
inline void read_and_convert_view( gp_image const & image, View const & view, CC cc ) {
    image.convert_to_view( view, cc );
}

template <typename View,typename CC>
inline void read_and_convert_view( char const * const filename, View const & view, CC cc ) {
    read_and_convert_view( gp_image( filename ), view, cc );
}


template <typename View>
inline void read_and_convert_view( gp_image const & image, View const & view ) {
    image.convert_to_view( view );
}

template <typename View>
inline void read_and_convert_view( const char* filename, const View& view ) {
    read_and_convert_view( gp_image( filename ), view );
}


template <typename Image,typename CC>
inline void read_and_convert_image( gp_image const & gp_image, Image & gil_image, CC cc ) {
    gil_image.recreate( gp_image.get_dimensions(), sizeof( Gdiplus::ARGB ) );
    gp_image.convert_to_prepared_view( view( gil_image ), cc );
}

template <typename Image,typename CC>
inline void read_and_convert_image(const char* filename,Image& im,CC cc) {
    read_and_convert_image( gp_image( filename ), im, cc );
}

template <typename Image,typename CC>
inline void read_and_convert_image( wchar_t const * const filename, Image & im, CC const & cc ) {
    read_and_convert_image( gp_image( filename ), im, cc );
}


template <typename Image>
inline void read_and_convert_image( gp_image const & gp_image, Image & gil_image ) {
    gil_image.recreate( gp_image.dimensions(), sizeof( Gdiplus::ARGB ) );
    gp_image.convert_to_prepared_view( view( gil_image ) );
}

template <typename Image>
inline void read_and_convert_image(const char* filename,Image& im) {
    read_and_convert_image( gp_image( filename ), im );
}

template <typename Image>
inline void read_and_convert_image( wchar_t const * const filename, Image & im ) {
    read_and_convert_image( gp_image( filename ), im );
}

template <typename Image>
inline void read_and_convert_image( FILE * pFile, Image & im ) {
    read_and_convert_image( gp_FILE_image( pFile ), im );
}

template <typename Image>
inline void read_and_convert_image( memory_chunk_t const & in_memory_image, Image & im ) {
    read_and_convert_image( gp_memory_image( in_memory_image ), im );
}


template <typename View>
inline void png_write_view(const char* filename,const View& view) {
    detail::gp_image const m( view );
    m.save_to_png( filename );
}


//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // gp_private_io_hpp
