////////////////////////////////////////////////////////////////////////////////
///
/// \file libjpeg_private_base.hpp
/// ------------------------------
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
#ifndef libjpeg_private_base_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
#define libjpeg_private_base_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
//------------------------------------------------------------------------------
#include "../../gil_all.hpp"
#include "formatted_image.hpp"
#include "io_error.hpp"

#include <boost/foreach.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>

#define JPEG_INTERNALS
#include "jpeglib.h"
#undef JPEG_INTERNALS

#include <csetjmp>
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

BOOST_STATIC_ASSERT( BITS_IN_JSAMPLE == 8 );


template <typename Pixel, bool isPlanar>
struct gil_to_libjpeg_format : mpl::integral_c<J_COLOR_SPACE, JCS_UNKNOWN> {};

template <> struct gil_to_libjpeg_format<rgb8_pixel_t , false> : mpl::integral_c<J_COLOR_SPACE, JCS_RGB      > {};
template <> struct gil_to_libjpeg_format<gray8_pixel_t, false> : mpl::integral_c<J_COLOR_SPACE, JCS_GRAYSCALE> {};
template <> struct gil_to_libjpeg_format<cmyk8_pixel_t, false> : mpl::integral_c<J_COLOR_SPACE, JCS_CMYK     > {};



struct view_libjpeg_format
{
    template <class View>
    struct apply : gil_to_libjpeg_format<typename View::value_type, is_planar<View>::value> {};
};


typedef mpl::vector3
<
    image<rgb8_pixel_t , false>,
    image<gray8_pixel_t, false>,
    image<cmyk8_pixel_t, false>
> libjpeg_supported_pixel_formats;



struct libjpeg_guard {};


class libjpeg_roi
{
public:
    typedef std::ptrdiff_t     value_type;
    typedef point2<value_type> point_t   ;

    typedef value_type         offset_t  ;

public:
    libjpeg_roi( value_type const start_row, value_type const end_row )
        :
        start_row_( start_row ),
        end_row_  ( end_row   )
    {}

    value_type start_row() const { return start_row_; }
    value_type end_row  () const { return end_row_  ; }

private:
    void operator=( libjpeg_roi const & );

private:
    value_type const start_row_;
    value_type const   end_row_;
};


#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

class libjpeg_base : private libjpeg_guard
{
protected:
    struct for_decompressor {};
    struct for_compressor   {};

protected:
    libjpeg_base( for_decompressor ) throw(...)
    {
        initialize_error_handler();
        if ( setjmp( error_handler_target() ) )
            throw_jpeg_error();
        jpeg_create_decompress( &decompressor() );
    }

    libjpeg_base( for_compressor ) throw(...)
    {
        initialize_error_handler();
        if ( setjmp( error_handler_target() ) )
            throw_jpeg_error();
        jpeg_create_compress( &compressor() );
    }

    ~libjpeg_base() { jpeg_destroy( &common() ); }

    void abort() const { jpeg_abort( &mutable_this().common() ); }

    jmp_buf & error_handler_target() const { return longjmp_target_; }

    jpeg_common_struct           & common      ()       { return *gil_reinterpret_cast<j_common_ptr>( &libjpeg_object.compressor_ ); }
    jpeg_common_struct     const & common      () const { return const_cast<libjpeg_base &>( *this ).common(); }
    jpeg_compress_struct         & compressor  ()       { return libjpeg_object.compressor_  ; }
    jpeg_compress_struct   const & compressor  () const { return const_cast<libjpeg_base &>( *this ).compressor(); }
    jpeg_decompress_struct       & decompressor()       { return libjpeg_object.decompressor_; }
    jpeg_decompress_struct const & decompressor() const { return const_cast<libjpeg_base &>( *this ).decompressor(); }

    libjpeg_base & mutable_this() const { return const_cast<libjpeg_base &>( *this ); }

    __declspec( noreturn )
    static void throw_jpeg_error() throw(...)
    {
        io_error( "jpeg error" );
    }

private:
    void initialize_error_handler()
    {
        common().err         = jpeg_std_error( &jerr_ );
        common().client_data = this;
        jerr_.error_exit = &libjpeg_base::libX_fatal_error_handler;
    }

    static inline void libX_fatal_error_handler( j_common_ptr const cinfo )
    {
        libjpeg_base * const p_me( gil_reinterpret_cast<libjpeg_base *>( cinfo->client_data ) );
        BOOST_ASSERT( p_me );
        longjmp( p_me->longjmp_target_, 1 );
    }

private:
    union libjpeg_object_t
    {
        jpeg_compress_struct     compressor_;
        jpeg_decompress_struct decompressor_;
    } libjpeg_object;

    jpeg_error_mgr         jerr_ ;

    //#ifndef BOOST_MSVC
    mutable jmp_buf longjmp_target_;
    //#endif // BOOST_MSVC
};

class libjpeg_image;

struct decompression_setup_data_t
{
    decompression_setup_data_t( J_COLOR_SPACE const format, JSAMPROW const buffer, libjpeg_roi::offset_t const offset )
        : format_( format ), buffer_( buffer ), offset_( offset ) {}

    J_COLOR_SPACE         /*const*/ format_;
    JSAMPROW              /*const*/ buffer_;
    libjpeg_roi::offset_t /*const*/ offset_;
};

struct view_data_t : decompression_setup_data_t
{
    template <class View>
    explicit view_data_t( View const & view, libjpeg_roi::offset_t const offset )
        :
        decompression_setup_data_t
        (
            view_libjpeg_format::apply<View>::value,
            formatted_image_base::get_raw_data( view ),
            offset
        ),
        height_( view.height()            ),
        width_ ( view.width ()            ),
        stride_( view.pixels().row_size() )
    {}

    void set_format( J_COLOR_SPACE const format ) { format_ = format; }

    unsigned int    /*const*/ height_;
    unsigned int    /*const*/ width_ ;
    unsigned int    /*const*/ stride_;
};

template <>
struct formatted_image_traits<libjpeg_image>
{
    typedef J_COLOR_SPACE format_t;

    typedef libjpeg_supported_pixel_formats supported_pixel_formats_t;

    typedef libjpeg_roi roi_t;

    typedef view_libjpeg_format view_to_native_format;

    typedef view_data_t view_data_t;

    template <class View>
    struct is_supported : mpl::bool_<view_libjpeg_format::apply<View>::value != JCS_UNKNOWN> {};

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment = sizeof( void * ) );
};


class libjpeg_image
    :
    public  libjpeg_base,
    public  detail::formatted_image<libjpeg_image>
{
public:
    format_t format() const { return decompressor().jpeg_color_space; }

    format_t closest_gil_supported_format() const
    {
        switch ( format() )
        {
            case JCS_RGB  :
            case JCS_YCbCr:
                return JCS_RGB;
            
            case JCS_CMYK:
            case JCS_YCCK:
                return JCS_CMYK;

            case JCS_GRAYSCALE:
                return JCS_GRAYSCALE;

            default:
                BOOST_ASSERT( format() == JCS_UNKNOWN );
                BOOST_ASSERT( !"Unknown format." );
                return JCS_UNKNOWN;
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
            case JCS_RGB      : return 0;
            case JCS_GRAYSCALE: return 1;
            case JCS_CMYK     : return 2;
            default:
                BOOST_ASSERT( !"Should not get reached." ); __assume( false );
                return unsupported_format;
        }
    }

    static std::size_t format_size( format_t const format )
    {
        switch ( format )
        {
            default:
                BOOST_ASSERT( !"Invalid or unknown format specified." ); __assume( false );
                return 0;

            case JCS_RGB:
            case JCS_YCbCr:
                return 3;
            case JCS_CMYK:
            case JCS_YCCK:
                return 4;
            case JCS_GRAYSCALE:
                return 1;
        }
    }

    //...zzz...
    //std::size_t format_size( format_t const format ) const
    //{
    //    BOOST_ASSERT( format_size( format ) == decompressor().output_components );
    //    BOOST_ASSERT( decompressor().out_color_components == decompressor().output_components );
    //    ignore_unused_variable_warning( format );
    //    return decompressor().output_components;
    //}


public: /// \ingroup Construction
    explicit libjpeg_image( FILE * const p_file )
        :
        libjpeg_base( for_decompressor() )
    {
        BOOST_ASSERT( p_file );

        if ( setjmp( error_handler_target() ) )
            throw_jpeg_error();

        jpeg_stdio_src( &decompressor(), p_file );

        BOOST_VERIFY( jpeg_read_header( &decompressor(), true ) == JPEG_HEADER_OK );

        io_error_if( decompressor().data_precision != 8, "Unsupported image file data precision." );
    }


public:
    point2<std::ptrdiff_t> dimensions() const
    {
        return point2<std::ptrdiff_t>( decompressor().image_width, decompressor().image_height );
    }


private: // Private formatted_image_base interface.
    friend base_t;
    void raw_convert_to_prepared_view( view_data_t const & view_data ) const throw(...)
    {
        setup_decompression( view_data );

        if ( setjmp( error_handler_target() ) )
            throw_jpeg_error();

        JSAMPROW scanlines[ 4 ] =
        {
            view_data.buffer_,
            scanlines[ 0 ] + view_data.stride_,
            scanlines[ 1 ] + view_data.stride_,
            scanlines[ 2 ] + view_data.stride_
        };

        BOOST_ASSERT( _countof( scanlines ) >= decompressor().rec_outbuf_height );
        unsigned int scanlines_to_read( view_data.height_ );
        for ( ; ; )
        {
            BOOST_ASSERT( scanlines_to_read <= ( decompressor().output_height - decompressor().output_scanline ) );
            unsigned int const lines_read
            (
                jpeg_read_scanlines
                (
                    &mutable_this().decompressor(),
                    scanlines,
                    (std::min)( _countof( scanlines ), scanlines_to_read )
                )
            );
            scanlines_to_read -= lines_read;
            if ( !scanlines_to_read )
                return;

            scanlines[ 0 ] = scanlines[ lines_read - 1 ] + view_data.stride_;
            scanlines[ 1 ] = scanlines[ 0 ] + view_data.stride_;
            scanlines[ 2 ] = scanlines[ 1 ] + view_data.stride_;
            scanlines[ 3 ] = scanlines[ 2 ] + view_data.stride_;
        }
    }

    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
        typedef typename MyView::value_type pixel_t;
        std::size_t const scanline_length( decompressor().image_width * decompressor().num_components );
        scoped_ptr<JSAMPLE> const p_scanline_buffer( new JSAMPLE[ scanline_length ] );
        JSAMPROW       scanline   ( p_scanline_buffer.get()    );
        JSAMPROW const scanlineEnd( scanline + scanline_length );
        
        BOOST_ASSERT( closest_gil_supported_format() == view_libjpeg_format::apply<MyView>::value );
        setup_decompression
        (
            decompression_setup_data_t
            (
                view_libjpeg_format::apply<MyView>::value,
                scanline,
                detail::get_offset<offset_t>( view )
            )
        );

        BOOST_ASSERT( decompressor().output_width      == decompressor().image_width    );
        BOOST_ASSERT( decompressor().output_components == decompressor().num_components );
        
        if ( setjmp( error_handler_target() ) )
            throw_jpeg_error();

        unsigned int const scanlines_to_read( detail::original_view( view ).dimensions().y );
        for ( unsigned int scanline_index( 0 ); scanline_index < scanlines_to_read; ++scanline_index )
        {
            BOOST_VERIFY
            (
                jpeg_read_scanlines
                (
                    &mutable_this().decompressor(),
                    &scanline,
                    1
                ) == 1
            );

            pixel_t const *                 p_source_pixel( gil_reinterpret_cast_c<pixel_t const *>( scanline ) );
            typename TargetView::x_iterator p_target_pixel( view.row_begin( scanline_index )                    );
            while ( p_source_pixel < gil_reinterpret_cast_c<pixel_t const *>( scanlineEnd ) )
            {
                converter( *p_source_pixel, *p_target_pixel );
                ++p_source_pixel;
                ++p_target_pixel;
            }
        }
    }

    void copy_to_prepared_view( view_data_t const & view_data ) const
    {
        BOOST_ASSERT( view_data.width_  == static_cast<unsigned int>( dimensions().x ) );
        BOOST_ASSERT( view_data.height_ == static_cast<unsigned int>( dimensions().y ) );
        BOOST_ASSERT( view_data.format_ == closest_gil_supported_format()              );
        raw_convert_to_prepared_view( view_data );
    }

private:
    void setup_decompression( decompression_setup_data_t const & view_data ) const throw(...)
    {
        unsigned int const state       ( decompressor().global_state );
        unsigned int       rows_to_skip( view_data.offset_           );
        if
        (
            ( state                          != DSTATE_SCANNING   ) ||
            ( decompressor().out_color_space != view_data.format_ ) ||
            ( decompressor().output_scanline  > view_data.offset_ )
        )
        {
            BOOST_ASSERT
            (
                ( state == DSTATE_READY    ) ||
                ( state == DSTATE_SCANNING )
            );

            if ( setjmp( error_handler_target() ) )
                throw_jpeg_error();

            if ( state == DSTATE_SCANNING )
                abort();

            mutable_this().decompressor().out_color_space = view_data.format_;
            BOOST_VERIFY( jpeg_start_decompress( &mutable_this().decompressor() ) );
            BOOST_ASSERT( decompressor().output_scanline == 0 );
        }
        else
            rows_to_skip -= decompressor().output_scanline;

        if ( rows_to_skip )
            skip_rows( rows_to_skip, view_data.buffer_ );
        BOOST_ASSERT( decompressor().output_scanline == view_data.offset_ );
    }

    void skip_rows( unsigned int const number_of_rows_to_skip, JSAMPROW /*const*/ dummy_scanline_buffer ) const
    {
        BOOST_ASSERT( decompressor().raw_data_out == false           );
        BOOST_ASSERT( decompressor().global_state == DSTATE_SCANNING );
        mutable_this().decompressor().raw_data_out = true;
        mutable_this().decompressor().global_state = DSTATE_RAW_OK;

        std::size_t const max_number_of_components( 4 ); // CMYK
        std::size_t const max_sampling_factor     ( 2 ); // Documentation
        std::size_t const mcu_row_size            ( max_sampling_factor * DCTSIZE ); // Documentation
        BOOST_ASSERT( decompressor().num_components    <= max_number_of_components );
        BOOST_ASSERT( decompressor().max_v_samp_factor <= max_sampling_factor      );

        JSAMPROW   dummy_component_2d_array[ mcu_row_size             ];
        JSAMPARRAY dummy_scan_lines        [ max_number_of_components ];

        std::fill( begin( dummy_component_2d_array ), end( dummy_component_2d_array ), dummy_scanline_buffer          );
        std::fill( begin( dummy_scan_lines         ), end( dummy_scan_lines         ), &dummy_component_2d_array[ 0 ] );

        unsigned int number_of_rows_to_skip_using_scanlines
        (
            number_of_rows_to_skip % mcu_row_size
        );
        unsigned int number_of_rows_to_skip_using_raw
        (
            number_of_rows_to_skip - number_of_rows_to_skip_using_scanlines
        );

        while ( number_of_rows_to_skip_using_raw )
        {
            unsigned int const lines_to_read( _countof( dummy_component_2d_array ) );
            BOOST_VERIFY
            (
                jpeg_read_raw_data
                (
                    &mutable_this().decompressor(),
                    dummy_scan_lines,
                    lines_to_read
                ) == lines_to_read
            );
            number_of_rows_to_skip_using_raw -= lines_to_read;
        }

        mutable_this().decompressor().raw_data_out = false;
        mutable_this().decompressor().global_state = DSTATE_SCANNING;
        while ( number_of_rows_to_skip_using_scanlines-- )
        {
            BOOST_VERIFY
            (
                jpeg_read_scanlines
                (
                    &mutable_this().decompressor(),
                    &dummy_scanline_buffer,
                    1
                ) == 1
            );
        }
    }

    libjpeg_image & mutable_this() const { return static_cast<libjpeg_image &>( libjpeg_base::mutable_this() ); }

private:
    friend class libjpeg_view_base;

};

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif


//...zzz...incomplete...
////////////////////////////////////////////////////////////////////////////////
///
/// \class libjpeg_view_base
///
////////////////////////////////////////////////////////////////////////////////

class libjpeg_view_base : noncopyable
{
public:
    ~libjpeg_view_base()
    {
    }

protected:
    libjpeg_view_base( libjpeg_image & bitmap, unsigned int const lock_mode, libjpeg_image::roi const * const p_roi = 0 )
    {
    }

    template <typename Pixel>
    typename type_from_x_iterator<Pixel *>::view_t
    get_typed_view()
    {
        //todo assert correct type...
        interleaved_view<Pixel *>( bitmapData_.Width, bitmapData_.Height, bitmapData_.Scan0, bitmapData_.Stride );
    }

private:
};


//...zzz...incomplete...
////////////////////////////////////////////////////////////////////////////////
///
/// \class libjpeg_view
///
////////////////////////////////////////////////////////////////////////////////

template <typename Pixel>
class libjpeg_view
    :
    private libjpeg_view_base,
    public  type_from_x_iterator<Pixel *>::view_t
{
public:
    libjpeg_view( libjpeg_image & image, libjpeg_image::roi const * const p_roi = 0 )
        :
        libjpeg_view_base( image, ImageLockModeRead | ( is_const<Pixel>::value * ImageLockModeWrite ), p_roi ),
        type_from_x_iterator<Pixel *>::view_t( get_typed_view<Pixel>() )
    {}
};


//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libjpeg_private_base_hpp
