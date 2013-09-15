////////////////////////////////////////////////////////////////////////////////
///
/// \file libpng/backend.hpp
/// ------------------------
///
/// Copyright (c) Domagoj Saric 2010.-2013.
///
///  Use, modification and distribution is subject to the
///  Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef backend_hpp__9E0C99E6_EAE8_4D71_844B_93518FFCB5CE
#define backend_hpp__9E0C99E6_EAE8_4D71_844B_93518FFCB5CE
#pragma once
//------------------------------------------------------------------------------
#include "backend.hpp"
#include "detail/platform_specifics.hpp"
#include "detail/io_error.hpp"
#include "detail/libx_shared.hpp"
#include "detail/shared.hpp"

#include "boost/scoped_array.hpp"

#include "png.h"

#ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    #include <csetjmp>
#endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
#include <cstdlib>
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

template <typename Pixel, bool isPlanar>
struct gil_to_libpng_format : mpl::integral_c<unsigned int, backend_base::unsupported_format> {};

/// \todo Add bgr-layout formats as LibPNG actually supports them through
/// png_set_bgr().
///                                           (19.09.2010.) (Domagoj Saric)
template <> struct gil_to_libpng_format<rgb8_pixel_t  , false> : mpl::integral_c<unsigned int, PNG_COLOR_TYPE_RGB       | (  8 << 16 )> {};
template <> struct gil_to_libpng_format<rgba8_pixel_t , false> : mpl::integral_c<unsigned int, PNG_COLOR_TYPE_RGB_ALPHA | (  8 << 16 )> {};
template <> struct gil_to_libpng_format<gray8_pixel_t , false> : mpl::integral_c<unsigned int, PNG_COLOR_TYPE_GRAY      | (  8 << 16 )> {};
template <> struct gil_to_libpng_format<rgb16_pixel_t , false> : mpl::integral_c<unsigned int, PNG_COLOR_TYPE_RGB       | ( 16 << 16 )> {};
template <> struct gil_to_libpng_format<rgba16_pixel_t, false> : mpl::integral_c<unsigned int, PNG_COLOR_TYPE_RGB_ALPHA | ( 16 << 16 )> {};
template <> struct gil_to_libpng_format<gray16_pixel_t, false> : mpl::integral_c<unsigned int, PNG_COLOR_TYPE_GRAY      | ( 16 << 16 )> {};


template <typename Pixel, bool IsPlanar>
struct libpng_is_supported : mpl::bool_<gil_to_libpng_format<Pixel, IsPlanar>::value != -1> {};

template <typename View>
struct libpng_is_view_supported : libpng_is_supported<typename View::value_type, is_planar<View>::value> {};


typedef mpl::vector6
<
    image<rgb8_pixel_t  , false>,
    image<rgba8_pixel_t , false>,
    image<gray8_pixel_t , false>,
    image<rgb16_pixel_t , false>,
    image<rgba16_pixel_t, false>,
    image<gray16_pixel_t, false>
> libpng_supported_pixel_formats;


typedef generic_vertical_roi libpng_roi;


struct libpng_view_data_t
{
    typedef unsigned int format_t;

    template <class View>
    /*explicit*/ libpng_view_data_t( View const & view, libpng_roi::offset_t const offset = 0 )
        :
        format_( gil_to_libpng_format<typename View::value_type, is_planar<View>::value>::value ),
        buffer_( backend_base::get_raw_data( view ) ),
        offset_( offset                   ),
        height_( view.height()            ),
        width_ ( view.width ()            ),
        stride_( view.pixels().row_size() ),
        number_of_channels_( num_channels<View>::value )
    {
        BOOST_STATIC_ASSERT( libpng_is_view_supported<View>::value );
    }

    void set_format( unsigned int const format ) { format_ = format; }

    format_t             /*const*/ format_;
    png_byte *             const   buffer_;
    libpng_roi::offset_t   const   offset_;

    unsigned int const height_;
    unsigned int const width_ ;
    unsigned int const stride_;
    unsigned int const number_of_channels_;
};


inline void throw_libpng_error()
{
    io_error( "LibPNG failure" );
}


inline void PNGAPI png_warning_function( png_structp /*png_ptr*/, png_const_charp const message )
{
    #ifdef _DEBUG
        std::puts( message );
    #else
        ignore_unused_variable_warning( message );
    #endif // _DEBUG
}


inline void PNGAPI png_error_function( png_structp const png_ptr, png_const_charp const error_message )
{
    png_warning_function( png_ptr, error_message );

    #ifdef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        throw_libpng_error();
        boost::ignore_unused_variable_warning( png_ptr );
    #else
        longjmp( png_ptr->jmpbuf, 1 );
    #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
}


//...zzz...this will blow-up at link-time when included in multiple .cpps unless it gets moved into a separate module...
#ifdef PNG_NO_ERROR_TEXT
    extern "C" void PNGAPI png_error( png_structp const png_ptr, png_const_charp const error_message )
    {
        png_error_function( png_ptr, error_message );
    }

    extern "C" void PNGAPI png_chunk_error( png_structp const png_ptr, png_const_charp const error_message )
    {
        png_error( png_ptr, error_message );
    }
#endif // PNG_NO_ERROR_TEXT

#ifdef PNG_NO_WARNINGS
    extern "C" void PNGAPI png_warning( png_structp const png_ptr, png_const_charp const error_message )
    {
        png_warning_function( png_ptr, error_message );
    }

    extern "C" void PNGAPI png_chunk_warning( png_structp const png_ptr, png_const_charp const error_message )
    {
        png_warning_function( png_ptr, error_message );
    }
#endif // PNG_NO_WARNINGS


class lib_object_t : noncopyable
{
public:
    png_struct & png_object() const
    {
        BOOST_ASSERT( p_png_ && "Call this only after ensuring creation success!" );
        png_struct & png( *p_png_ );
        BF_ASSUME( &png != 0 );
        return png;
    }

    png_info & info_object() const
    {
        BOOST_ASSERT( p_png_ && "Call this only after ensuring creation success!" );
        png_info & info( *p_info_ );
        BF_ASSUME( &info != 0 );
        return info;
    }

protected:
    lib_object_t( png_struct * const p_png, png_info  * const p_info )
        :
        p_png_ ( p_png  ),
        p_info_( p_info )
    {}

#ifndef NDEBUG
    ~lib_object_t()
    {
        BOOST_ASSERT( !p_png_ && !p_info_ && "The concrete class must do the cleanup!" );
    }
#endif // NDEBUG

    //...zzz...forced by LibPNG into either duplication or this anti-pattern...
    bool is_valid() const { return p_png_ && p_info_; }

    png_struct * & png_object_for_destruction () { return p_png_ ; }
    png_info   * & info_object_for_destruction() { return p_info_; }

private:
    png_struct * __restrict p_png_ ;
    png_info   * __restrict p_info_;
}; // class lib_object_t

//------------------------------------------------------------------------------
} // namespace detail


class libpng_image;

template <>
struct backend_traits<libpng_image>
{
    typedef detail::libpng_supported_pixel_formats supported_pixel_formats_t;
    typedef detail::libpng_roi                     roi_t;
    typedef detail::libpng_view_data_t             view_data_t;
    typedef detail::libpng_view_data_t::format_t   format_t;

    struct gil_to_native_format
    {
        template <typename Pixel, bool IsPlanar>
        struct apply : detail::gil_to_libpng_format<Pixel, IsPlanar> {};
    };

    template <typename Pixel, bool IsPlanar>
    struct is_supported : detail::libpng_is_supported<Pixel, IsPlanar> {};

    typedef mpl::map3
            <
                mpl::pair<memory_range_t        , detail::seekable_input_memory_range_extender<libpng_image> >,
                mpl::pair<FILE                  ,                                              libpng_image  >,
                mpl::pair<char           const *, detail::input_c_str_for_mmap_extender       <libpng_image> >
            > native_sources;

    typedef mpl::map2
            <
                mpl::pair<FILE        ,                                          detail::libpng_writer_FILE  >,
                mpl::pair<char const *, detail::output_c_str_for_c_file_extender<detail::libpng_writer_FILE> >
            > native_sinks;

    typedef mpl::vector1_c<format_tag, png> supported_image_formats;

    typedef view_data_t writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true             );
}; // struct backend_traits<libpng_image>


////////////////////////////////////////////////////////////////////////////////
///
/// \class libpng_image
///
/// \brief
///
////////////////////////////////////////////////////////////////////////////////

class libpng_image
	:
	public detail::lib_object_t,
	public detail::backend<libpng_image>
{
public:
	struct guard {};

public: /// \ingroup Information
    typedef point2<unsigned int> dimensions_t;

    dimensions_t dimensions() const
    {
        return dimensions_t
        (
            ::png_get_image_width ( &png_object(), &info_object() ),
            ::png_get_image_height( &png_object(), &info_object() )
        );
    }

    unsigned int number_of_channels() const { return ::png_get_channels ( &png_object(), &info_object() ); }
    unsigned int bit_depth         () const { return ::png_get_bit_depth( &png_object(), &info_object() ); }

    format_t format() const { return ::png_get_color_type( &png_object(), &info_object() ) | ( bit_depth() << 16 ); }

    format_t closest_gil_supported_format() const
    {
        format_t const current_format( format() );

        switch ( current_format & 0xFF )
        {
            default: return current_format;

            case PNG_COLOR_TYPE_PALETTE   : return PNG_COLOR_TYPE_RGB  | (                                   8 << 16 ); // 8-bit RGB
            case PNG_COLOR_TYPE_GRAY_ALPHA: return PNG_COLOR_TYPE_RGBA | ( ( ( current_format >> 16 ) & 0xFF ) << 16 ); // (bits of current_format) RGBA
        }
    }

    image_type_id_t current_image_format_id() const { return image_format_id( closest_gil_supported_format() ); }

    unsigned int pixel_size() const { return number_of_channels() * bit_depth() / 8; }

	static image_type_id_t image_format_id( format_t const closest_gil_supported_format )
	{
		switch ( closest_gil_supported_format )
		{
			case PNG_COLOR_TYPE_RGB       | (  8 << 16 ) : return 0;
			case PNG_COLOR_TYPE_RGB_ALPHA | (  8 << 16 ) : return 1;
			case PNG_COLOR_TYPE_GRAY      | (  8 << 16 ) : return 2;
			case PNG_COLOR_TYPE_RGB       | ( 16 << 16 ) : return 3;
			case PNG_COLOR_TYPE_RGB_ALPHA | ( 16 << 16 ) : return 4;
			case PNG_COLOR_TYPE_GRAY      | ( 16 << 16 ) : return 5;

			default:
				return unsupported_format;
		}
	}

public:
    typedef lib_object_t lib_object_t;

	lib_object_t       & lib_object()       { return *this; }
    lib_object_t const & lib_object() const { return *this; }

protected:
	libpng_image( png_struct * const p_png )
		:
		lib_object_t( p_png, ::png_create_info_struct( p_png ) )
	{}

	//...zzz...forced by LibPNG into either duplication or this anti-pattern...
	bool successful_creation() const { return is_valid(); }

	static unsigned int format_bit_depth( libpng_view_data_t::format_t const format )
	{
		return ( format >> 16 ) & 0xFF;
	}

	static unsigned int format_colour_type( libpng_view_data_t::format_t const format )
	{
		return format & 0xFF;
	}

#ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
	jmp_buf & error_handler_target() const { return png_object().jmpbuf; }
#endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
}; // class libpng_image

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // backend_hpp
