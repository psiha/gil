/*
    Copyright 2005-2007 Adobe Systems Incorporated
   
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).

    See http://opensource.adobe.com/gil for most recent version including documentation.
*/

/*************************************************************************************************/

#ifndef GIL_PNG_IO_H
#define GIL_PNG_IO_H

/// \file
/// \brief  Support for reading and writing PNG files
///         Requires libpng and zlib! (or GDI+)
//
// We are currently providing the following functions:
// point2<std::ptrdiff_t>    png_read_dimensions(const char*)
// template <typename View>  void png_read_view(const char*,const View&)
// template <typename View>  void png_read_image(const char*,image<View>&)
// template <typename View>  void png_write_view(const char*,const View&)
// template <typename View>  struct png_read_support;
// template <typename View>  struct png_write_support;
//
/// \author Hailin Jin and Lubomir Bourdev \n
///         Adobe Systems Incorporated
/// \date   2005-2007 \n Last updated September 24, 2006

#include <cstddef>
#include <string>

#include "../../gil_config.hpp"
#include "../../utilities.hpp"

namespace boost { namespace gil {

/// \ingroup PNG_IO
/// \brief Returns the width and height of the PNG file at the specified location.
/// Throws std::ios_base::failure if the location does not correspond to a valid PNG file
template <typename Source>
point2<std::ptrdiff_t> png_read_dimensions( Source const & );

/// \ingroup PNG_IO
/// \brief Returns the width and height of the PNG file at the specified location.
/// Throws std::ios_base::failure if the location does not correspond to a valid PNG file
inline point2<std::ptrdiff_t> png_read_dimensions(const std::string& filename) {
    return png_read_dimensions(filename.c_str());
}

/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name into the given view.
/// Triggers a compile assert if the view color space and channel depth are not supported by the PNG library or by the I/O extension.
/// Throws std::ios_base::failure if the file is not a valid PNG file, or if its color space or channel depth are not 
/// compatible with the ones specified by View, or if its dimensions don't match the ones of the view.
template <typename View, typename Source>
void png_read_view( Source const &, View const & );

/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name into the given view.
template <typename View>
inline void png_read_view(const std::string& filename,const View& view) {
    png_read_view(filename.c_str(),view);
}

/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, and loads the pixels into it.
/// Triggers a compile assert if the image color space or channel depth are not supported by the PNG library or by the I/O extension.
/// Throws std::ios_base::failure if the file is not a valid PNG file, or if its color space or channel depth are not 
/// compatible with the ones specified by Image
template <typename Image, typename Source>
void png_read_image( Source const &, Image & );

/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, and loads the pixels into it.
template <typename Image>
inline void png_read_image(const std::string& filename,Image& im) {
    png_read_image(filename.c_str(),im);
}

/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name and color-converts it into the given view.
/// Throws std::ios_base::failure if the file is not a valid PNG file, or if its dimensions don't match the ones of the view.
template <typename View, typename CC, typename Source>
void png_read_and_convert_view( Source const &, View const &, CC );

/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name and color-converts it into the given view.
/// Throws std::ios_base::failure if the file is not a valid PNG file, or if its dimensions don't match the ones of the view.
template <typename View, typename Source>
void png_read_and_convert_view( Source const &, View const & );

/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name and color-converts it into the given view.
template <typename View,typename CC>
inline void png_read_and_convert_view(const std::string& filename,const View& view,CC cc) {
    png_read_and_convert_view(filename.c_str(),view,cc);
}

/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name and color-converts it into the given view.
template <typename View>
inline void png_read_and_convert_view(const std::string& filename,const View& view) {
    png_read_and_convert_view(filename.c_str(),view);
}

/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, loads and color-converts the pixels into it.
/// Throws std::ios_base::failure if the file is not a valid PNG file
template <typename Image,typename CC, typename Source>
void png_read_and_convert_image( Source const &, Image &, CC );

/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, loads and color-converts the pixels into it.
/// Throws std::ios_base::failure if the file is not a valid PNG file
template <typename Image, typename Source>
void png_read_and_convert_image( Source const &, Image & );

/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, loads and color-converts the pixels into it.
template <typename Image,typename CC>
inline void png_read_and_convert_image(const std::string& filename,Image& im,CC cc) {
    png_read_and_convert_image(filename.c_str(),im,cc);
}

/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, loads and color-converts the pixels into it.
template <typename Image>
inline void png_read_and_convert_image(const std::string& filename,Image& im) {
    png_read_and_convert_image(filename.c_str(),im);
}

/// \ingroup PNG_IO
/// \brief Saves the view to a png file specified by the given png image file name.
/// Triggers a compile assert if the view color space and channel depth are not supported by the PNG library or by the I/O extension.
/// Throws std::ios_base::failure if it fails to create the file.
template <typename View, typename Source>
void png_write_view( Source const &, View const & );

/// \ingroup PNG_IO
/// \brief Saves the view to a png file specified by the given png image file name.
template <typename View>
inline void png_write_view(const std::string& filename,const View& view) {
    png_write_view(filename.c_str(),view);
}

} }  // namespace boost::gil


#if !defined( _WIN32 )
    #undef BOOST_GIL_USE_NATIVE_IO
#endif // (un)supported platforms for native IO


#ifdef BOOST_GIL_USE_NATIVE_IO

    #ifdef _WIN32
        #include "gp_private_png_io.inl"
    #endif
    
#else // BOOST_GIL_USE_NATIVE_IO

    #include "png_io_libpng.inl"

#endif // BOOST_GIL_USE_NATIVE_IO

#endif
