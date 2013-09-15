////////////////////////////////////////////////////////////////////////////////
///
/// \file libjpeg_image.hpp
/// -----------------------
///
/// Copyright (c) Domagoj Saric 2010.-2013.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef reader_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
#define reader_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
#pragma once
//------------------------------------------------------------------------------
#include "backend.hpp"
#include "../detail/reader.hpp"

#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/libx_shared.hpp"
#include "boost/gil/extension/io2/detail/platform_specifics.hpp"
#include "boost/gil/extension/io2/detail/shared.hpp"

#include "boost/gil/image_view_factory.hpp"

#include <boost/array.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/smart_ptr/scoped_array.hpp>
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

#if defined( BOOST_MSVC )
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

struct decompression_setup_data_t
{
    decompression_setup_data_t( J_COLOR_SPACE const format, JSAMPROW const buffer, libjpeg_roi::offset_t const offset )
        : format_( format ), buffer_( buffer ), offset_( offset ) {}

    J_COLOR_SPACE         /*const*/ format_;
    JSAMPROW              /*const*/ buffer_;
    libjpeg_roi::offset_t /*const*/ offset_;
}; // struct decompression_setup_data_t

struct view_data_t : decompression_setup_data_t
{
    template <class View>
    explicit view_data_t( View const & view, libjpeg_roi::offset_t const offset = 0 )
        :
        decompression_setup_data_t
        (
            gil_to_libjpeg_format<typename View::value_type, is_planar<View>::value>::value,
            backend_base::get_raw_data( view ),
            offset
        ),
        height_( view.height()            ),
        width_ ( view.width ()            ),
        stride_( view.pixels().row_size() ),
        number_of_channels_( num_channels<View>::value )
    {
        BOOST_STATIC_ASSERT(( libjpeg_is_supported<typename View::value_type, is_planar<View>::value>::value ));
    }

    void set_format( J_COLOR_SPACE const format ) { format_ = format; }

    unsigned int /*const*/ height_;
    unsigned int /*const*/ width_ ;
    unsigned int /*const*/ stride_;
    unsigned int           number_of_channels_;
}; // struct view_data_t

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
///
/// \class libjpeg_image
///
////////////////////////////////////////////////////////////////////////////////

class libjpeg_reader
    :
    public libjpeg_image
{
public: /// \ingroup Information
    dimensions_t dimensions() const
    {
        // Implementation note:
        //   A user might have setup output image scaling through the low-level
        // lib_object accessor or the scale_image() member function so we have
        // to use the "output" dimensions.
        //                                    (17.10.2010.) (Domagoj Saric)
        return dimensions_t( decompressor().output_width, decompressor().output_height );
    }

public: /// \ingroup Backend specific - transformation
    void scale_image( unsigned int const scale_numerator, unsigned int const scale_denominator )
    {
        decompressor().scale_num   = scale_numerator  ;
        decompressor().scale_denom = scale_denominator;
        jpeg_calc_output_dimensions( &const_cast<libjpeg_image &>( *this ).lib_object() );
    }

public: // Low-level (row, strip, tile) access
    void read_row( sequential_row_read_state, unsigned char * const p_row_storage ) const
    {
        read_scanline( p_row_storage );
    }

    jpeg_decompress_struct       & lib_object()       { return decompressor(); }
    jpeg_decompress_struct const & lib_object() const { return decompressor(); }

public: /// \ingroup Construction
	template <class Device>
    explicit libjpeg_image( Device & device )
        :
        libjpeg_base( for_decompressor() )
    {
    #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        if ( setjmp( libjpeg_base::error_handler_target() ) )
            libjpeg_base::throw_jpeg_error();
    #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

        setup_source( device );

        read_header();
    }


private: // Private interface for the base backend<> class.
    // Implementation note:
    //   MSVC 10 accepts friend base_t and friend class base_t, Clang 2.8
    // accepts friend class base_t, Apple Clang 1.6 and GCC (4.2 and 4.6) accept
    // neither.
    //                                        (13.01.2011.) (Domagoj Saric)
    friend class detail::backend<libjpeg_image>;

    void raw_convert_to_prepared_view( detail::view_data_t const & view_data ) const BOOST_GIL_CAN_THROW
    {
        setup_decompression( view_data );

        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( libjpeg_base::error_handler_target() ) )
                libjpeg_base::throw_jpeg_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

        JSAMPROW scanlines[ 4 ] =
        {
            view_data.buffer_,
            scanlines[ 0 ] + view_data.stride_,
            scanlines[ 1 ] + view_data.stride_,
            scanlines[ 2 ] + view_data.stride_
        };

        BOOST_ASSERT( boost::size( scanlines ) >= decompressor().rec_outbuf_height );
        unsigned int scanlines_to_read( view_data.height_ );
        for ( ; ; )
        {
            BOOST_ASSERT( scanlines_to_read <= ( decompressor().output_height - decompressor().output_scanline ) );
            unsigned int const lines_read
            (
                read_scanlines
                (
                    scanlines,
                    std::min<std::size_t>( boost::size( scanlines ), scanlines_to_read )
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
        using namespace detail;

        typedef typename MyView::value_type pixel_t;
        std::size_t           const scanline_length  ( decompressor().image_width * decompressor().num_components );
        scoped_array<JSAMPLE> const p_scanline_buffer( new JSAMPLE[ scanline_length ] );
        JSAMPROW       scanline    ( p_scanline_buffer.get()    );
        JSAMPROW const scanline_end( scanline + scanline_length );

        format_t const my_format( gil_to_libjpeg_format<typename MyView::value_type, is_planar<MyView>::value>::value );
        BOOST_ASSERT( this->closest_gil_supported_format() == my_format );
        setup_decompression
        (
            decompression_setup_data_t
            (
                my_format,
                scanline,
                detail::get_offset<offset_t>( view )
            )
        );

        BOOST_ASSERT( decompressor().output_width      == decompressor().image_width    );
        BOOST_ASSERT( decompressor().output_components == decompressor().num_components );
        
        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( libjpeg_base::error_handler_target() ) )
                libjpeg_base::throw_jpeg_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

        unsigned int const scanlines_to_read( detail::original_view( view ).dimensions().y );
        for ( unsigned int scanline_index( 0 ); scanline_index < scanlines_to_read; ++scanline_index )
        {
            read_scanline( scanline );

            typedef typename get_original_view_t<TargetView>::type::x_iterator target_x_iterator;

            pixel_t const *   p_source_pixel( gil_reinterpret_cast_c<pixel_t const *>( scanline ) );
            target_x_iterator p_target_pixel( original_view( view ).row_begin( scanline_index )   );
            while ( p_source_pixel < gil_reinterpret_cast_c<pixel_t const *>( scanline_end ) )
            {
                converter( *p_source_pixel, *p_target_pixel );
                ++p_source_pixel;
                ++p_target_pixel;
            }
        }
    }


    void raw_copy_to_prepared_view( detail::view_data_t const & view_data ) const
    {
        BOOST_ASSERT( view_data.width_  == static_cast<unsigned int>( dimensions().x ) );
        BOOST_ASSERT( view_data.height_ == static_cast<unsigned int>( dimensions().y ) );
        BOOST_ASSERT( view_data.format_ == closest_gil_supported_format()              );
        raw_convert_to_prepared_view( view_data );
    }


    static unsigned int cached_format_size( format_t const format )
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
                BOOST_ASSERT( !"Invalid or unknown format specified." );
                BF_UNREACHABLE_CODE
                return 0;
        }
    }

private:
    void setup_decompression( detail::decompression_setup_data_t const & view_data ) const BOOST_GIL_CAN_THROW
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

            #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
                if ( setjmp( libjpeg_base::error_handler_target() ) )
                    libjpeg_base::throw_jpeg_error();
            #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

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

        unsigned int const max_number_of_components( 4 ); // CMYK
        unsigned int const max_sampling_factor     ( 2 ); // Documentation
        unsigned int const mcu_row_size            ( max_sampling_factor * DCTSIZE ); // Documentation
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
            unsigned int const lines_to_read( boost::size( dummy_component_2d_array ) );
            read_raw_data( dummy_scan_lines, lines_to_read );
            number_of_rows_to_skip_using_raw -= lines_to_read;
        }

        mutable_this().decompressor().raw_data_out = false;
        mutable_this().decompressor().global_state = DSTATE_SCANNING;
        while ( number_of_rows_to_skip_using_scanlines-- )
        {
            read_scanline( dummy_scanline_buffer );
        }
    }

    unsigned int read_scanlines( JSAMPROW scanlines[], unsigned int const scanlines_to_read ) const BOOST_GIL_CAN_THROW
    {
        return jpeg_read_scanlines
        (
            &mutable_this().decompressor(),
            scanlines,
            scanlines_to_read
        );
    }

    void read_scanline( JSAMPROW scanline ) const BOOST_GIL_CAN_THROW
    {
        BOOST_VERIFY
        (
            read_scanlines( &scanline, 1 ) == 1
        );
    }

    void read_raw_data( JSAMPARRAY scanlines[], unsigned int const scanlines_to_read ) const BOOST_GIL_CAN_THROW
    {
        BOOST_VERIFY
        (
            jpeg_read_raw_data
            (
                &mutable_this().decompressor(),
                scanlines,
                scanlines_to_read
            ) == scanlines_to_read
        );
    }

    libjpeg_image & mutable_this() const { return static_cast<libjpeg_image &>( libjpeg_base::mutable_this() ); }

private:
    friend class libjpeg_view_base;

    void read_header()
    {
        BOOST_VERIFY( jpeg_read_header( &decompressor(), true ) == JPEG_HEADER_OK );

        // Implementation note:
        //   To enable users to setup output scaling we use the output
        // dimensions to report the image dimensions in the dimensions() getter
        // so we have to manually initialize them here.
        //                                    (17.10.2010.) (Domagoj Saric)
        BOOST_ASSERT( decompressor().output_width  == 0 );
        BOOST_ASSERT( decompressor().output_height == 0 );

        decompressor().output_width  = decompressor().image_width ;
        decompressor().output_height = decompressor().image_height;

        detail::io_error_if( decompressor().data_precision != BITS_IN_JSAMPLE, "Unsupported image file data precision." );
    }

    // Unextracted "libjpeg_reader" interface.
    void setup_source()
    {
        BOOST_ASSERT( decompressor().src == NULL );
        decompressor().src = &source_manager_;
    }

    void setup_source( memory_range_t & memory_chunk )
    {
        setup_source();

        decompressor().client_data = &memory_chunk;

        source_manager_.next_input_byte = memory_chunk.begin();
        source_manager_.bytes_in_buffer = memory_chunk.size ();

        source_manager_.init_source       = &init_memory_chunk_source;
        source_manager_.fill_input_buffer = &fill_memory_chunk_buffer;
        source_manager_.skip_input_data   = &skip_memory_chunk_data  ;
        source_manager_.resync_to_restart = &jpeg_resync_to_restart  ;
        source_manager_.term_source       = &term_memory_chunk_source;
    }

    void setup_source( FILE & file )
    {
        setup_source();

        decompressor().client_data = &file;

        source_manager_.next_input_byte = read_buffer_.begin();
        source_manager_.bytes_in_buffer = 0;

        source_manager_.init_source       = &init_FILE_source      ;
        source_manager_.fill_input_buffer = &fill_FILE_buffer      ;
        source_manager_.skip_input_data   = &skip_FILE_data        ;
        source_manager_.resync_to_restart = &jpeg_resync_to_restart;
        source_manager_.term_source       = &term_FILE_source      ;
    }


    static void BF_CDECL init_FILE_source( j_decompress_ptr const p_cinfo )
    {
        libjpeg_image & reader( get_reader( p_cinfo ) );

        reader.source_manager_.next_input_byte = reader.read_buffer_.begin();
        reader.source_manager_.bytes_in_buffer = 0;
    }

    static boolean BF_CDECL fill_FILE_buffer( j_decompress_ptr const p_cinfo )
    {
        libjpeg_image & reader( get_reader( p_cinfo ) );

        std::size_t bytes_read
        (
            /*std*/::fread
            (
                reader.read_buffer_.begin(),
                1,
                reader.read_buffer_.size(),
                static_cast<FILE *>( reader.decompressor().client_data )
            )
        );

        if ( bytes_read == 0 )
        {
            // Insert a fake EOI marker (see the comment for the default library
            // implementation).
            reader.read_buffer_[ 0 ] = 0xFF;
            reader.read_buffer_[ 1 ] = JPEG_EOI;

            bytes_read = 2;
        }

        reader.source_manager_.next_input_byte = reader.read_buffer_.begin();
        reader.source_manager_.bytes_in_buffer = bytes_read;

        return true;
    }

    static void BF_CDECL skip_FILE_data( j_decompress_ptr const p_cinfo, long num_bytes )
    {
        libjpeg_image & reader( get_reader( p_cinfo ) );

        if ( static_cast<std::size_t>( num_bytes ) <= reader.source_manager_.bytes_in_buffer )
        {
            reader.source_manager_.next_input_byte += num_bytes;
            reader.source_manager_.bytes_in_buffer -= num_bytes;
        }
        else
        {
            num_bytes -= reader.source_manager_.bytes_in_buffer;
            /*std*/::fseek( static_cast<FILE *>( reader.decompressor().client_data ), num_bytes, SEEK_CUR ); //...failure?
            reader.source_manager_.next_input_byte = 0;
            reader.source_manager_.bytes_in_buffer = 0;
        }
    }

    static void BF_CDECL term_FILE_source( j_decompress_ptr /*p_cinfo*/ )
    {
    }

    // Ensure that jpeg_finish_decompress() is called so that this gets called...
    static void BF_CDECL term_and_close_FILE_source( j_decompress_ptr const p_cinfo )
    {
        term_FILE_source( p_cinfo );
        BOOST_VERIFY( /*std*/::fclose( static_cast<FILE *>( get_reader( p_cinfo ).decompressor().client_data ) ) == 0 );
    }

    static void BF_CDECL init_memory_chunk_source( j_decompress_ptr /*p_cinfo*/ )
    {
    }

    static boolean BF_CDECL fill_memory_chunk_buffer( j_decompress_ptr /*p_cinfo*/ )
    {
        BF_UNREACHABLE_CODE
        return true;
    }

    static void BF_CDECL skip_memory_chunk_data( j_decompress_ptr const p_cinfo, long num_bytes )
    {
        libjpeg_image & reader( get_reader( p_cinfo ) );

        BOOST_ASSERT( static_cast<std::size_t>( num_bytes ) <= reader.source_manager_.bytes_in_buffer );
        reader.source_manager_.next_input_byte += num_bytes;
        reader.source_manager_.bytes_in_buffer -= num_bytes;
    }

    static void BF_CDECL term_memory_chunk_source( j_decompress_ptr /*p_cinfo*/ )
    {
    }

    static libjpeg_image & get_reader(  j_decompress_ptr const p_cinfo )
    {
        libjpeg_image & reader( static_cast<libjpeg_image &>( base( gil_reinterpret_cast<j_common_ptr>( p_cinfo ) ) ) );
        BOOST_ASSERT( p_cinfo->src == &reader.source_manager_ );
        return reader;
    }

private:
    jpeg_source_mgr     source_manager_;
    array<JOCTET, 4096> read_buffer_   ;//...zzz...extract to a wrapper...not needed for in memory sources...
}; // class libjpeg_reader

#if defined( BOOST_MSVC )
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
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // reader_hpp
