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
#include "formatted_image.hpp"
#include "detail/io_error.hpp"
#include "detail/libx_shared.hpp"

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


typedef generic_vertical_roi libjpeg_roi;


struct libjpeg_object_wrapper_t
{
    union aux_union
    {
        jpeg_compress_struct     compressor_;
        jpeg_decompress_struct decompressor_;
    } libjpeg_object;
};


#if defined( BOOST_MSVC )
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

class libjpeg_base : private libjpeg_object_wrapper_t
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
    jpeg_common_struct     const & common      () const { return const_cast<libjpeg_base &>( *this ).common      (); }
    jpeg_compress_struct         & compressor  ()       { return libjpeg_object.compressor_  ; }
    jpeg_compress_struct   const & compressor  () const { return const_cast<libjpeg_base &>( *this ).compressor  (); }
    jpeg_decompress_struct       & decompressor()       { return libjpeg_object.decompressor_; }
    jpeg_decompress_struct const & decompressor() const { return const_cast<libjpeg_base &>( *this ).decompressor(); }

    libjpeg_base & mutable_this() const { return const_cast<libjpeg_base &>( *this ); }

    static libjpeg_base & base( jpeg_common_struct * const p_libjpeg_object )
    {
        BOOST_ASSERT( p_libjpeg_object );
        return *static_cast<libjpeg_base *>( gil_reinterpret_cast<libjpeg_object_wrapper_t *>( p_libjpeg_object ) );
    }

    __declspec( noreturn )
    static void throw_jpeg_error() throw(...)
    {
        io_error( "jpeg error" );
    }

    __declspec( noreturn )
    static inline void libX_fatal_error_handler( j_common_ptr const p_cinfo )
    {
        longjmp( base( p_cinfo ).error_handler_target(), 1 );
    }

private:
    void initialize_error_handler()
    {
        #ifndef NDEBUG
            jpeg_std_error( &jerr_ );
        #else
            std::memset( &jerr_, 0, sizeof( jerr_ ) );
            jerr_.emit_message    = &emit_message   ;
            jerr_.output_message  = &output_message ;
            jerr_.format_message  = &format_message ;
            jerr_.reset_error_mgr = &reset_error_mgr;
        #endif // NDEBUG
        jerr_.error_exit = &error_exit;

        common().err = &jerr_;
    }

    static void __cdecl error_exit( j_common_ptr const p_cinfo )
    {
        #ifndef NDEBUG
            p_cinfo->err->output_message( p_cinfo );
        #endif

        libX_fatal_error_handler( p_cinfo );
    }

    static void __cdecl output_message( j_common_ptr /*p_cinfo*/ ) {}
    static void __cdecl emit_message  ( j_common_ptr /*p_cinfo*/, int /*msg_level*/ ) {}
    static void __cdecl format_message( j_common_ptr /*p_cinfo*/, char * /*buffer*/ ) {}

    static void __cdecl reset_error_mgr( j_common_ptr const p_cinfo )
    {
        BOOST_ASSERT( p_cinfo->err->num_warnings == 0 );
        BOOST_ASSERT( p_cinfo->err->msg_code     == 0 );
        ignore_unused_variable_warning( p_cinfo );
    }

private:
    //#ifndef BOOST_MSVC
    mutable jmp_buf longjmp_target_;
    //#endif // BOOST_MSVC

    jpeg_error_mgr jerr_ ;
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
    /*explicit*/ view_data_t( View const & view, libjpeg_roi::offset_t const offset = 0 )
        :
        decompression_setup_data_t
        (
            view_libjpeg_format::apply<View>::value,
            formatted_image_base::get_raw_data( view ),
            offset
        ),
        height_( view.height()            ),
        width_ ( view.width ()            ),
        stride_( view.pixels().row_size() ),
        number_of_channels_( num_channels<View>::value )
    {}

    void set_format( J_COLOR_SPACE const format ) { format_ = format; }

    unsigned int /*const*/ height_;
    unsigned int /*const*/ width_ ;
    unsigned int /*const*/ stride_;
    unsigned int           number_of_channels_;
};


class libjpeg_writer : public libjpeg_base
{
public:
    explicit libjpeg_writer( char const * const p_target_file_name )
        :
        libjpeg_base( for_compressor() )
    {
        setup_destination( p_target_file_name );
    }


    ~libjpeg_writer()
    {
        jpeg_finish_compress( &compressor() );
    }

    jpeg_compress_struct       & lib_object()       { return compressor(); }
    jpeg_compress_struct const & lib_object() const { return const_cast<libjpeg_writer &>( *this ).lib_object(); }

    static void write( view_data_t const & view, char const * const p_target_file_name )
    {
        BOOST_ASSERT( view.format_ != JCS_UNKNOWN );

        libjpeg_writer writer( p_target_file_name );
        
        writer.compressor().image_width  = static_cast<JDIMENSION>( view.width_  );
        writer.compressor().image_height = static_cast<JDIMENSION>( view.height_ );
        writer.compressor().input_components = view.number_of_channels_;
        writer.compressor().in_color_space   = view.format_;
        jpeg_set_defaults  ( &writer.compressor()             );
        //jpeg_set_quality   ( &writer.compressor(), 100, false );
        jpeg_start_compress( &writer.compressor(),      false );

        JSAMPLE *       p_row( view.buffer_ );
        JSAMPLE * const p_end( memunit_advanced( view.buffer_, view.height_ * view.stride_ ) );
        while ( p_row < p_end )
        {
            io_error_if( jpeg_write_scanlines( &writer.compressor(), &p_row, 1 ) != 1 );
            memunit_advance( p_row, view.stride_ );
        }
    }

private:
    static libjpeg_writer & writer(  j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( static_cast<libjpeg_writer &>( base( gil_reinterpret_cast<j_common_ptr>( p_cinfo ) ) ) );
        BOOST_ASSERT( p_cinfo->dest == &writer.destination_manager_ );
        return writer;
    }

    void setup_destination()
    {
        destination_manager_.next_output_byte = 0;
        destination_manager_.free_in_buffer   = 0;

        BOOST_ASSERT( compressor().dest == NULL );
        compressor().dest = &destination_manager_;
    }

    void setup_destination( FILE & file )
    {
        setup_destination();

        compressor().client_data = &file;

        destination_manager_.init_destination    = &init_FILE_destination;
        destination_manager_.empty_output_buffer = &empty_FILE_buffer    ;
        destination_manager_.term_destination    = &term_FILE_destination;
    }

    void setup_destination( char const * const p_file_name )
    {
        FILE * const p_file( /*std*/::fopen( p_file_name, "wb" ) );
        if ( !p_file )
            throw_jpeg_error();
        setup_destination( *p_file );
        destination_manager_.term_destination = &term_and_close_FILE_destination;
    }

    static void __cdecl init_FILE_destination( j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( writer( p_cinfo ) );

        writer.destination_manager_.next_output_byte = writer.write_buffer_.begin();
        writer.destination_manager_.free_in_buffer   = writer.write_buffer_.size ();
    }

    void write_FILE_bytes( std::size_t const number_of_bytes )
    {
        if
        (
            /*std*/::fwrite
            (
                write_buffer_.begin(),
                number_of_bytes,
                1,
                static_cast<FILE *>( compressor().client_data )
            ) != 1
        )
            libX_fatal_error_handler( &common() );
    }
    
    static boolean __cdecl empty_FILE_buffer( j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( writer( p_cinfo ) );
        writer.write_FILE_bytes( writer.write_buffer_.size() );
        init_FILE_destination( p_cinfo );
        return true;
    }

    static void __cdecl term_FILE_destination( j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( writer( p_cinfo ) );

        std::size_t const remaining_bytes( writer.write_buffer_.size() - writer.destination_manager_.free_in_buffer );

        writer.write_FILE_bytes( remaining_bytes );
    }

    static void __cdecl term_and_close_FILE_destination( j_compress_ptr const p_cinfo )
    {
        term_FILE_destination( p_cinfo );
        BOOST_VERIFY( /*std*/::fclose( static_cast<FILE *>( writer( p_cinfo ).compressor().client_data ) ) == 0 );
    }

private:
    jpeg_destination_mgr       destination_manager_;
    array<unsigned char, 4096> write_buffer_       ;
};


template <>
struct formatted_image_traits<libjpeg_image>
{
    typedef J_COLOR_SPACE                   format_t;
    typedef libjpeg_supported_pixel_formats supported_pixel_formats_t;
    typedef libjpeg_roi                     roi_t;
    typedef view_libjpeg_format             view_to_native_format;
    typedef view_data_t                     view_data_t;

    template <class View>
    struct is_supported : mpl::bool_<view_libjpeg_format::apply<View>::value != JCS_UNKNOWN> {};

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true             );
};


class libjpeg_image
    :
    public  libjpeg_base,
    public  detail::formatted_image<libjpeg_image>
{
public:
    struct guard {};

public:
    format_t format() const { return decompressor().jpeg_color_space; }

    format_t closest_gil_supported_format() const
    {
        format_t const current_format( format() );
        #ifdef _DEBUG
        switch ( current_format )
        {
            case JCS_RGB      :
            case JCS_YCbCr    :
            case JCS_CMYK     :
            case JCS_YCCK     :
            case JCS_GRAYSCALE:
            case JCS_UNKNOWN  :
                break;

            default:
                BOOST_ASSERT( !"Unknown format code." );
        }
        #endif

        switch ( current_format )
        {
            case JCS_YCbCr: return JCS_RGB       ;
            case JCS_YCCK : return JCS_CMYK      ;
            default       : return current_format;
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
                BOOST_ASSERT( closest_gil_supported_format == JCS_UNKNOWN );
                return unsupported_format;
        }
    }

    static std::size_t format_size( format_t const format )
    {
        switch ( format )
        {
            case JCS_RGB:
            case JCS_YCbCr:
                return 3;
            case JCS_CMYK:
            case JCS_YCCK:
                return 4;
            case JCS_GRAYSCALE:
                return 1;

            default:
                BOOST_ASSERT( !"Invalid or unknown format specified." ); __assume( false );
                return 0;
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

    jpeg_decompress_struct       & lib_object()       { return decompressor(); }
    jpeg_decompress_struct const & lib_object() const { return const_cast<libjpeg_image &>( *this ).lib_object(); }

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
        std::size_t         const scanline_length  ( decompressor().image_width * decompressor().num_components );
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

    void raw_copy_to_prepared_view( view_data_t const & view_data ) const
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
            ( state                          !=                          DSTATE_SCANNING   )   ||
            ( decompressor().out_color_space !=                          view_data.format_ )   ||
            ( decompressor().output_scanline  > static_cast<JDIMENSION>( view_data.offset_ ) )
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
        BOOST_ASSERT( decompressor().output_scanline == static_cast<JDIMENSION>( view_data.offset_ ) );
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

//class libjpeg_view_base : noncopyable
//{
//public:
//    ~libjpeg_view_base()
//    {
//    }
//
//protected:
//    libjpeg_view_base( libjpeg_image & bitmap, unsigned int const lock_mode, libjpeg_image::roi const * const p_roi = 0 )
//    {
//    }
//
//    template <typename Pixel>
//    typename type_from_x_iterator<Pixel *>::view_t
//    get_typed_view()
//    {
//        //todo assert correct type...
//        interleaved_view<Pixel *>( bitmapData_.Width, bitmapData_.Height, bitmapData_.Scan0, bitmapData_.Stride );
//    }
//
//private:
//};


//...zzz...incomplete...
////////////////////////////////////////////////////////////////////////////////
///
/// \class libjpeg_view
///
////////////////////////////////////////////////////////////////////////////////

//template <typename Pixel>
//class libjpeg_view
//    :
//    private libjpeg_view_base,
//    public  type_from_x_iterator<Pixel *>::view_t
//{
//public:
//    libjpeg_view( libjpeg_image & image, libjpeg_image::roi const * const p_roi = 0 )
//        :
//        libjpeg_view_base( image, ImageLockModeRead | ( is_const<Pixel>::value * ImageLockModeWrite ), p_roi ),
//        type_from_x_iterator<Pixel *>::view_t( get_typed_view<Pixel>() )
//    {}
//};


//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libjpeg_private_base_hpp
