/*
    Copyright 2005-2007 Adobe Systems Incorporated
   
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).

    See http://opensource.adobe.com/gil for most recent version including documentation.
*/

/*************************************************************************************************/

#include <cstdio>
#include <string>
extern "C" {
#include "png.h"
}
#include <boost/static_assert.hpp>
#include "../../gil_config.hpp"
#include "../../utilities.hpp"
#include "io_error.hpp"
#include "png_io_private.hpp"

namespace boost { namespace gil {


/// \ingroup PNG_IO
/// \brief Returns the width and height of the PNG file at the specified location.
/// Throws std::ios_base::failure if the location does not correspond to a valid PNG file
template <typename Source>
point2<std::ptrdiff_t> png_read_dimensions( Source const & source ) {
    detail::png_reader m(source);
    return m.get_dimensions();
}

/// \ingroup PNG_IO
/// \brief Determines whether the given view type is supported for reading
template <typename View>
struct png_read_support {
    BOOST_STATIC_CONSTANT(bool,is_supported=
                          (detail::png_read_support_private<typename channel_type<View>::type,
                                                            typename color_space_type<View>::type>::is_supported));
    BOOST_STATIC_CONSTANT(int,bit_depth=
                          (detail::png_read_support_private<typename channel_type<View>::type,
                                                            typename color_space_type<View>::type>::bit_depth));
    BOOST_STATIC_CONSTANT(int,color_type=
                          (detail::png_read_support_private<typename channel_type<View>::type,
                                                            typename color_space_type<View>::type>::color_type));
    BOOST_STATIC_CONSTANT(bool, value=is_supported);
};

/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name into the given view.
/// Triggers a compile assert if the view color space and channel depth are not supported by the PNG library or by the I/O extension.
/// Throws std::ios_base::failure if the file is not a valid PNG file, or if its color space or channel depth are not 
/// compatible with the ones specified by View, or if its dimensions don't match the ones of the view.
template <typename View, typename Source>
void png_read_view( Source const & source, View const & view ) {
    BOOST_STATIC_ASSERT(png_read_support<View>::is_supported);
    detail::png_reader m(source);
    m.apply(view);
}


/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, and loads the pixels into it.
/// Triggers a compile assert if the image color space or channel depth are not supported by the PNG library or by the I/O extension.
/// Throws std::ios_base::failure if the file is not a valid PNG file, or if its color space or channel depth are not 
/// compatible with the ones specified by Image
template <typename Image, typename Source>
void png_read_image( Source const & source, Image & im ) {
    BOOST_STATIC_ASSERT(png_read_support<typename Image::view_t>::is_supported);
    detail::png_reader m(source);
    m.read_image(im);
}


/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name and color-converts it into the given view.
/// Throws std::ios_base::failure if the file is not a valid PNG file, or if its dimensions don't match the ones of the view.
template <typename View, typename CC, typename Source>
void png_read_and_convert_view( Source const & source, View const & view, CC cc ) {
    detail::png_reader_color_convert<CC> m(source,cc);
    m.apply(view);
}

/// \ingroup PNG_IO
/// \brief Loads the image specified by the given png image file name and color-converts it into the given view.
/// Throws std::ios_base::failure if the file is not a valid PNG file, or if its dimensions don't match the ones of the view.
template <typename View, typename Source>
void png_read_and_convert_view( Source const & source, View const & view ) {
    detail::png_reader_color_convert<default_color_converter> m(source,default_color_converter());
    m.apply(view);
}


/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, loads and color-converts the pixels into it.
/// Throws std::ios_base::failure if the file is not a valid PNG file
template <typename Image,typename CC, typename Source>
void png_read_and_convert_image( Source const & source, Image & im, CC cc ) {
    detail::png_reader_color_convert<CC> m(source,cc);
    m.read_image(im);
}


/// \ingroup PNG_IO
/// \brief Allocates a new image whose dimensions are determined by the given png image file, loads and color-converts the pixels into it.
/// Throws std::ios_base::failure if the file is not a valid PNG file
template <typename Image, typename Source>
void png_read_and_convert_image( Source const & source, Image & im ) {
    detail::png_reader_color_convert<default_color_converter> m(source,default_color_converter());
    m.read_image(im);
}


/// \ingroup PNG_IO
/// \brief Determines whether the given view type is supported for writing
template <typename View>
struct png_write_support {
    BOOST_STATIC_CONSTANT(bool,is_supported=
                          (detail::png_write_support_private<typename channel_type<View>::type,
                                                             typename color_space_type<View>::type>::is_supported));
    BOOST_STATIC_CONSTANT(int,bit_depth=
                          (detail::png_write_support_private<typename channel_type<View>::type,
                                                             typename color_space_type<View>::type>::bit_depth));
    BOOST_STATIC_CONSTANT(int,color_type=
                          (detail::png_write_support_private<typename channel_type<View>::type,
                                                             typename color_space_type<View>::type>::color_type));
    BOOST_STATIC_CONSTANT(bool, value=is_supported);
};

/// \ingroup PNG_IO
/// \brief Saves the view to a png file specified by the given png image file name.
/// Triggers a compile assert if the view color space and channel depth are not supported by the PNG library or by the I/O extension.
/// Throws std::ios_base::failure if it fails to create the file.
template <typename View, typename Source>
void png_write_view( Source const & source, View const & view ) {
    BOOST_STATIC_ASSERT(png_write_support<View>::is_supported);
    detail::png_writer m(source);
    m.apply(view);
}


} }  // namespace boost::gil
