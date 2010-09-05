////////////////////////////////////////////////////////////////////////////////
///
/// \file libpng_image.hpp
/// ----------------------
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
#ifndef libpng_image_hpp__9E0C99E6_EAE8_4D71_844B_93518FFCB5CE
#define libpng_image_hpp__9E0C99E6_EAE8_4D71_844B_93518FFCB5CE
//------------------------------------------------------------------------------
#include "formatted_image.hpp"
#include "detail/io_error.hpp"
#include "detail/libx_shared.hpp"

#include "png.h"

#include <csetjmp>
#include <cstdlib>
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------

class libpng_image;
class libpng_image_from_char;

namespace detail
{
//------------------------------------------------------------------------------

template <typename Pixel, bool isPlanar>
struct gil_to_libpng_format : mpl::integral_c<int, -1> {};

template <> struct gil_to_libpng_format<rgb8_pixel_t  , false> : mpl::integral_c<int, PNG_COLOR_TYPE_RGB       | (  8 << 16 )> {};
template <> struct gil_to_libpng_format<rgba8_pixel_t , false> : mpl::integral_c<int, PNG_COLOR_TYPE_RGB_ALPHA | (  8 << 16 )> {};
template <> struct gil_to_libpng_format<gray8_pixel_t , false> : mpl::integral_c<int, PNG_COLOR_TYPE_GRAY      | (  8 << 16 )> {};
template <> struct gil_to_libpng_format<rgb16_pixel_t , false> : mpl::integral_c<int, PNG_COLOR_TYPE_RGB       | ( 16 << 16 )> {};
template <> struct gil_to_libpng_format<rgba16_pixel_t, false> : mpl::integral_c<int, PNG_COLOR_TYPE_RGB_ALPHA | ( 16 << 16 )> {};
template <> struct gil_to_libpng_format<gray16_pixel_t, false> : mpl::integral_c<int, PNG_COLOR_TYPE_GRAY      | ( 16 << 16 )> {};



struct view_libpng_format
{
    template <class View>
    struct apply : gil_to_libpng_format<typename View::value_type, is_planar<View>::value> {};
};


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

//...zzz...
//class scoped_read_png_ptr
//{
//public:
//    scoped_read_png_ptr()
//        :
//        p_png_( ::png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL ) )
//    {}
//    ~scoped_read_png_ptr()
//    {
//        ::png_destroy_read_struct( p_png_, NULL, NULL );
//    }
//
//private:
//    png_structp const p_png_;
//};
//
//class scoped_png_info_ptr
//{
//public:
//    scoped_png_ptr( png_structp const p_png_ )
//private:
//    ;
//    png_infop   const p_info_;
//};



struct libpng_view_data_t
{
    template <class View>
    /*explicit*/ libpng_view_data_t( View const & view, libpng_roi::offset_t const offset = 0 )
        :
        format_( view_libpng_format::apply<View>::value ),
        buffer_( formatted_image_base::get_raw_data( view ) ),
        offset_( offset ),
        height_( view.height()            ),
        width_ ( view.width ()            ),
        stride_( view.pixels().row_size() ),
        number_of_channels_( num_channels<View>::value )
    {}

    void set_format( unsigned int const format ) { format_ = format; }

    unsigned int          /*const*/ format_;
    unsigned int          /*const*/ colour_type_;
    unsigned int bits_per_pixel_;
    png_byte *              /*const*/ buffer_;
    libpng_roi::offset_t /*const*/ offset_;

    unsigned int /*const*/ height_;
    unsigned int /*const*/ width_ ;
    unsigned int /*const*/ stride_;
    unsigned int           number_of_channels_;
};


template <>
struct formatted_image_traits<libpng_image>
{
    typedef int                            format_t;
    typedef libpng_supported_pixel_formats supported_pixel_formats_t;
    typedef libpng_roi                     roi_t;
    typedef view_libpng_format             view_to_native_format;
    typedef libpng_view_data_t             view_data_t;

    template <class View>
    struct is_supported : mpl::bool_<view_libpng_format::apply<View>::value != -1> {};

    typedef mpl::map2
            <
                mpl::pair<FILE       &, libpng_image          >,
                mpl::pair<char const *, libpng_image_from_char>
            > readers;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true             );
};

//------------------------------------------------------------------------------
} // namespace detail


class libpng_image
    :
    public detail::formatted_image<libpng_image>
{
public:
    struct guard {};

public:
    format_t format() const
    {
        return ::png_get_color_type( &png_object(), &info_object() ) | ( bit_depth() << 16 );
    }

    format_t closest_gil_supported_format() const
    {
        format_t const current_format( format() );

        switch ( current_format & 0xFF )
        {
            default: return current_format;

            case PNG_COLOR_TYPE_PALETTE   : return PNG_COLOR_TYPE_RGB  | ( 8 << 16 );
            case PNG_COLOR_TYPE_GRAY_ALPHA: return PNG_COLOR_TYPE_RGBA | ( ( ( current_format >> 16 ) & 0xFF ) << 16 );
        }
    }

    image_type_id current_image_format_id() const
    {
        return image_format_id( closest_gil_supported_format() );
    }

    static image_type_id image_format_id( format_t const closest_gil_supported_format )
    {
        switch ( closest_gil_supported_format )
        {
            case PNG_COLOR_TYPE_RGB       | (  8 << 16 ): return 0;
            case PNG_COLOR_TYPE_RGB_ALPHA | (  8 << 16 ): return 1;
            case PNG_COLOR_TYPE_GRAY      | (  8 << 16 ): return 2;
            case PNG_COLOR_TYPE_RGB       | ( 16 << 16 ): return 3;
            case PNG_COLOR_TYPE_RGB_ALPHA | ( 16 << 16 ): return 4;
            case PNG_COLOR_TYPE_GRAY      | ( 16 << 16 ): return 5;

            default:
                return unsupported_format;
        }
    }

    std::size_t format_size( format_t const format ) const
    {
        return number_of_channels() * ( ( format >> 16 ) & 0xFF ) / 8;
    }

    std::size_t format_size() const
    {
        return number_of_channels() * bit_depth() / 8;
    }

public: /// \ingroup Construction
    explicit libpng_image( FILE & file )
        :
        p_png_ ( ::png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL ) ),
        p_info_( ::png_create_info_struct( p_png_                                  ) )
    {
        /// \todo Replace this manual logic with proper RAII guards.
        ///                                   (03.09.2010.) (Domagoj Saric)
        if ( !p_png_ || !p_info_ )
            cleanup_and_throw_libpng_error();

        ::png_init_io( &png_object(), &file );

        init();
    }

    ~libpng_image()
    {
        __assume( p_png_  != 0 );
        __assume( p_info_ != 0 );
        destroy_read_struct();
    }

public:
    point2<std::ptrdiff_t> dimensions() const
    {
        return point2<std::ptrdiff_t>
               (
                   ::png_get_image_width ( &png_object(), &info_object() ),
                   ::png_get_image_height( &png_object(), &info_object() )
               );
    }

    png_struct & lib_object() const { return png_object(); }

private: // Private formatted_image_base interface.
    friend class base_t;

    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
        std::size_t          const row_length  ( ::png_get_rowbytes( &png_object(), &info_object() ) );
        scoped_ptr<png_byte> const p_row_buffer( new png_byte[ row_length ]     );

        if ( setjmp( error_handler_target() ) )
            throw_libpng_error();

        unsigned int number_of_passes( ::png_set_interlace_handling( &png_object() ) );
        __assume( ( number_of_passes == 1 ) || ( number_of_passes == 7 ) );
        BOOST_ASSERT( ( number_of_passes == 1 ) && "Missing interlaced support for the generic conversion case." );
        ignore_unused_variable_warning( number_of_passes );

        skip_rows( detail::get_offset<offset_t>( view ) );

        png_byte       * const p_row    ( p_row_buffer.get() );
        png_byte const * const p_row_end( p_row + row_length );

        unsigned int const rows_to_read( detail::original_view( view ).dimensions().y );
        for ( unsigned int row_index( 0 ); row_index < rows_to_read; ++row_index )
        {
            ::png_read_row( &png_object(), p_row, NULL );

            typedef typename MyView::value_type pixel_t;

            pixel_t const *                 p_source_pixel( gil_reinterpret_cast_c<pixel_t const *>( p_row ) );
            typename TargetView::x_iterator p_target_pixel( view.row_begin( row_index )                      );
            while ( p_source_pixel < gil_reinterpret_cast_c<pixel_t const *>( p_row_end ) )
            {
                converter( *p_source_pixel, *p_target_pixel );
                ++p_source_pixel;
                ++p_target_pixel;
            }
        }
    }

    void raw_convert_to_prepared_view( detail::libpng_view_data_t const & view_data ) const throw(...)
    {
        BOOST_ASSERT( view_data.width_  == static_cast<unsigned int>( dimensions().x ) );
        BOOST_ASSERT( view_data.height_ == static_cast<unsigned int>( dimensions().y ) );
        BOOST_ASSERT( view_data.format_ == static_cast<unsigned int>( closest_gil_supported_format() ) );

        unsigned int const bit_depth  ( ( view_data.format_ >> 16 ) & 0xFF );
        unsigned int const colour_type(   view_data.format_         & 0xFF );

        if ( colour_type == PNG_COLOR_TYPE_PALETTE )
        {
            ::png_set_palette_to_rgb( &png_object() );
        }
        else
        if ( colour_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
            ::png_set_expand_gray_1_2_4_to_8( &png_object() );
        if ( ::png_get_valid( &png_object(), &info_object(), PNG_INFO_tRNS ) )
            ::png_set_tRNS_to_alpha( &png_object() );

        if ( bit_depth == 8 )
            ::png_set_strip_16( &png_object() );

        if ( !( colour_type & PNG_COLOR_MASK_ALPHA ) )
            ::png_set_strip_alpha( &png_object() );

        //...zzz...

        //if (color_type == PNG_COLOR_TYPE_RGB ||
        //    color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        //    png_set_bgr(png_ptr);

        //if (color_type == PNG_COLOR_TYPE_RGB ||
        //    color_type == PNG_COLOR_TYPE_GRAY)
        //    png_set_add_alpha(png_ptr, filler, PNG_FILLER_AFTER);

        //if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        //    png_set_swap_alpha(png_ptr);

        //if (color_type == PNG_COLOR_TYPE_GRAY ||
        //    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        //    png_set_gray_to_rgb(png_ptr);

        //if (color_type == PNG_COLOR_TYPE_RGB ||
        //    color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        //    png_set_rgb_to_gray_fixed(png_ptr, error_action,
        //    int red_weight, int green_weight);

        ::png_read_update_info( &png_object(), &info_object() );

        raw_copy_to_prepared_view( view_data );
    }

    void raw_copy_to_prepared_view( detail::libpng_view_data_t const view_data ) const
    {
        if ( setjmp( error_handler_target() ) )
            throw_libpng_error();

        unsigned int number_of_passes( ::png_set_interlace_handling( &png_object() ) );
        __assume( ( number_of_passes == 1 ) || ( number_of_passes == 7 ) );

        skip_rows( view_data.offset_ );

        while ( number_of_passes-- )
        {
            png_byte       *       p_row( view_data.buffer_ );
            png_byte const * const p_end( p_row + ( view_data.height_ * view_data.stride_ ) );
            while ( p_row < p_end )
            {
                ::png_read_row( &png_object(), p_row, NULL );
                memunit_advance( p_row, view_data.stride_ );
            }
        }
    }

private:
    jmp_buf & error_handler_target() const { return png_object().jmpbuf; }

    __declspec( noreturn )
    static void throw_libpng_error()
    {
        detail::io_error( "LibPNG failure" );
    }

    __declspec( noreturn )
    void cleanup_and_throw_libpng_error()
    {
        destroy_read_struct();
        throw_libpng_error ();
    }

    void init()
    {
        if ( setjmp( error_handler_target() ) )
            cleanup_and_throw_libpng_error();

        ::png_read_info( &png_object(), &info_object() );
        if ( little_endian() )
            ::png_set_swap( &png_object() );
    }

    void skip_rows( unsigned int number_of_rows_to_skip ) const
    {
        while ( number_of_rows_to_skip-- )
        {
            ::png_read_row( &png_object(), NULL, NULL );
        }
    }

    unsigned int number_of_channels() const { return ::png_get_channels ( &png_object(), &info_object() ); }
    std::size_t  bit_depth         () const { return ::png_get_bit_depth( &png_object(), &info_object() ); }

    void destroy_read_struct() { ::png_destroy_read_struct( &p_png_, &p_info_, NULL ); }

    png_struct & png_object() const
    {
        png_struct & png( *p_png_ );
        __assume( &png != 0 );
        return png;
    }

    png_info & info_object() const
    {
        png_info & info( *p_info_ );
        __assume( &info != 0 );
        return info;
    }

private:
    png_struct * __restrict /*const*/ p_png_ ;
    png_info   * __restrict /*const*/ p_info_;
};


class libpng_image_from_char
    :
    private detail::c_file_guard,
    public  libpng_image
{
public:
    libpng_image_from_char( char const * const file_name )
        :
        detail::c_file_guard( file_name ),
        libpng_image        ( get()     )
    {}
};

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libpng_image_hpp
