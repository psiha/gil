////////////////////////////////////////////////////////////////////////////////
///
/// \file libjpeg_image.hpp
/// -----------------------
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
#ifndef libjpeg_image_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
#define libjpeg_image_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
//------------------------------------------------------------------------------
#include "formatted_image.hpp"
#include "detail/io_error.hpp"
#include "detail/libx_shared.hpp"
#include "detail/shared.hpp"

#include <boost/array.hpp>
#include <boost/range/size.hpp>
#include <boost/smart_ptr/scoped_array.hpp>

#define JPEG_INTERNALS
#include "jpeglib.h"
#undef JPEG_INTERNALS

#ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    #include <csetjmp>
#endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
#include <cstdlib>
#if defined(BOOST_MSVC)
    #pragma warning( push )
    #pragma warning( disable : 4996 ) // "The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name."
    #include "io.h"
    #include "sys/stat.h"
#endif // MSVC
#include "fcntl.h"
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


template <typename Pixel, bool IsPlanar>
struct libjpeg_is_supported : mpl::bool_<gil_to_libjpeg_format<Pixel, IsPlanar>::value != JCS_UNKNOWN> {};


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
    libjpeg_base( for_decompressor ) BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
    {
        initialize_error_handler();
        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( error_handler_target() ) )
                throw_jpeg_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        jpeg_create_decompress( &decompressor() );
    }

    libjpeg_base( for_compressor ) BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
    {
        initialize_error_handler();
        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( error_handler_target() ) )
                throw_jpeg_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        jpeg_create_compress( &compressor() );
    }

    ~libjpeg_base() { jpeg_destroy( &common() ); }

    void abort() const { jpeg_abort( &mutable_this().common() ); }

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

    #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        jmp_buf & error_handler_target() const { return longjmp_target_; }
    #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

    static void fatal_error_handler( j_common_ptr const p_cinfo )
    {
        #ifdef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            throw_jpeg_error();
            ignore_unused_variable_warning( p_cinfo );
        #else
            longjmp( base( p_cinfo ).error_handler_target(), 1 );
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    }

    static void throw_jpeg_error()
    {
        io_error( "LibJPEG error" );
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

    static void BF_CDECL error_exit( j_common_ptr const p_cinfo )
    {
        #ifndef NDEBUG
            p_cinfo->err->output_message( p_cinfo );
        #endif

        fatal_error_handler( p_cinfo );
    }

    static void BF_CDECL output_message( j_common_ptr /*p_cinfo*/                    ) {}
    static void BF_CDECL emit_message  ( j_common_ptr /*p_cinfo*/, int /*msg_level*/ ) {}
    static void BF_CDECL format_message( j_common_ptr /*p_cinfo*/, char * /*buffer*/ ) {}

    static void BF_CDECL reset_error_mgr( j_common_ptr const p_cinfo )
    {
        BOOST_ASSERT( p_cinfo->err->num_warnings == 0 );
        BOOST_ASSERT( p_cinfo->err->msg_code     == 0 );
        ignore_unused_variable_warning( p_cinfo );
    }

private:
    #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        mutable jmp_buf longjmp_target_;
    #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

    jpeg_error_mgr jerr_ ;
};


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
            gil_to_libjpeg_format<typename View::value_type, is_planar<View>::value>::value,
            formatted_image_base::get_raw_data( view ),
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
};


class libjpeg_writer
    :
    private libjpeg_base,
    public  configure_on_write_writer
{
public:
    explicit libjpeg_writer( char const * const p_target_file_name )
        :
        libjpeg_base( for_compressor() )
    {
        setup_destination( p_target_file_name );
    }

    explicit libjpeg_writer( FILE & file )
        :
        libjpeg_base( for_compressor() )
    {
        setup_destination( file );
    }

    ~libjpeg_writer()
    {
        jpeg_finish_compress( &compressor() );
    }

    jpeg_compress_struct       & lib_object()       { return compressor(); }
    jpeg_compress_struct const & lib_object() const { return const_cast<libjpeg_writer &>( *this ).lib_object(); }

    void write_default( view_data_t const & view ) BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
    {
        setup_compression( view );

        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( libjpeg_base::error_handler_target() ) )
                libjpeg_base::throw_jpeg_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        jpeg_set_defaults( &compressor() );
        //jpeg_set_quality( &compressor(), 100, false );

        do_write( view );
    }

    void write( view_data_t const & view )
    {
        setup_compression( view );
        do_write         ( view );
    }

private:
    void setup_compression( view_data_t const & view )
    {
        compressor().image_width      = static_cast<JDIMENSION>( view.width_  );
        compressor().image_height     = static_cast<JDIMENSION>( view.height_ );
        compressor().input_components = view.number_of_channels_;
        compressor().in_color_space   = view.format_;
    }

    void do_write( view_data_t const & view ) BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
    {
        BOOST_ASSERT( view.format_ != JCS_UNKNOWN );

        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( libjpeg_base::error_handler_target() ) )
                libjpeg_base::throw_jpeg_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        
        jpeg_start_compress( &compressor(), false );

        JSAMPLE *       p_row( view.buffer_ );
        JSAMPLE * const p_end( memunit_advanced( view.buffer_, view.height_ * view.stride_ ) );
        while ( p_row < p_end )
        {
            write_row( p_row );
            memunit_advance( p_row, view.stride_ );
        }
    }

    void write_row( JSAMPLE * p_row ) BOOST_GIL_CAN_THROW
    {
        BOOST_VERIFY( jpeg_write_scanlines( &compressor(), &p_row, 1 ) == 1 );
    }

    static libjpeg_writer & get_writer( j_compress_ptr const p_cinfo )
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

        destination_manager_.init_destination    = &init_destination     ;
        destination_manager_.empty_output_buffer = &empty_FILE_buffer    ;
        destination_manager_.term_destination    = &term_FILE_destination;
    }

    void setup_destination( char const * const p_file_name )
    {
        int const file_descriptor( /*std*/::open( p_file_name, BF_MSVC_SPECIFIC( O_BINARY | ) O_CREAT | O_WRONLY, S_IREAD | S_IWRITE ) );
        if ( file_descriptor < 0 )
            throw_jpeg_error();

        setup_destination();

        compressor().client_data = reinterpret_cast<void *>( file_descriptor );

        destination_manager_.init_destination    = &init_destination             ;
        destination_manager_.empty_output_buffer = &empty_fd_buffer              ;
        destination_manager_.term_destination    = &term_and_close_fd_destination;
    }

    static void BF_CDECL init_destination( j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( get_writer( p_cinfo ) );

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
            fatal_error_handler( &common() );
    }

    void write_fd_bytes( std::size_t const number_of_bytes )
    {
        if
        (
            /*std*/::write
            (
                reinterpret_cast<int>( compressor().client_data ),
                write_buffer_.begin(),
                number_of_bytes
            ) != static_cast<int>( number_of_bytes )
        )
            fatal_error_handler( &common() );
    }
    
    static boolean BF_CDECL empty_FILE_buffer( j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( get_writer( p_cinfo ) );
        writer.write_FILE_bytes( writer.write_buffer_.size() );
        init_destination( p_cinfo );
        return true;
    }

    static boolean BF_CDECL empty_fd_buffer( j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( get_writer( p_cinfo ) );
        writer.write_fd_bytes( writer.write_buffer_.size() );
        init_destination( p_cinfo );
        return true;
    }

    static void BF_CDECL term_FILE_destination( j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( get_writer( p_cinfo ) );

        std::size_t const remaining_bytes( writer.write_buffer_.size() - writer.destination_manager_.free_in_buffer );

        writer.write_FILE_bytes( remaining_bytes );
    }

    static void BF_CDECL term_fd_destination( j_compress_ptr const p_cinfo )
    {
        libjpeg_writer & writer( get_writer( p_cinfo ) );

        std::size_t const remaining_bytes( writer.write_buffer_.size() - writer.destination_manager_.free_in_buffer );

        writer.write_fd_bytes( remaining_bytes );
    }

    // Ensure that jpeg_finish_compress() is called so that this gets called...
    static void BF_CDECL term_and_close_FILE_destination( j_compress_ptr const p_cinfo )
    {
        term_FILE_destination( p_cinfo );
        BOOST_VERIFY( /*std*/::fclose( static_cast<FILE *>( get_writer( p_cinfo ).compressor().client_data ) ) == 0 );
    }

    static void BF_CDECL term_and_close_fd_destination( j_compress_ptr const p_cinfo )
    {
        term_fd_destination( p_cinfo );
        BOOST_VERIFY( /*std*/::close( reinterpret_cast<int>( get_writer( p_cinfo ).compressor().client_data ) ) == 0 );
    }

private:
    jpeg_destination_mgr        destination_manager_;
    array<unsigned char, 65536> write_buffer_       ;
};
//------------------------------------------------------------------------------
} // namespace detail


class libjpeg_image;

template <>
struct formatted_image_traits<libjpeg_image>
{
    typedef       ::J_COLOR_SPACE                   format_t;
    typedef detail::libjpeg_supported_pixel_formats supported_pixel_formats_t;
    typedef detail::libjpeg_roi                     roi_t;
    typedef detail::view_data_t                     view_data_t;

    struct gil_to_native_format
    {
        template <typename Pixel, bool IsPlanar>
        struct apply : detail::gil_to_libjpeg_format<Pixel, IsPlanar> {};
    };

    template <typename Pixel, bool IsPlanar>
    struct is_supported : detail::libjpeg_is_supported<Pixel, IsPlanar> {};

    typedef mpl::map3
            <
                mpl::pair<memory_chunk_t        , detail::seekable_input_memory_range_extender<libjpeg_image> >,
                mpl::pair<FILE                  ,                                              libjpeg_image  >,
                mpl::pair<char           const *, detail::input_c_str_for_mmap_extender       <libjpeg_image> >
            > readers;

    typedef mpl::map2
            <
                mpl::pair<FILE        , detail::libjpeg_writer>,
                mpl::pair<char const *, detail::libjpeg_writer>
            > writers;

    typedef mpl::vector1_c<format_tag, jpeg> supported_image_formats;

    typedef view_data_t writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true             );
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class libjpeg_image
///
////////////////////////////////////////////////////////////////////////////////

class libjpeg_image
    :
    public detail::libjpeg_base,
    public detail::formatted_image<libjpeg_image>
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

    point2<std::ptrdiff_t> dimensions() const
    {
        // Implementation note:
        //   A user might have setup output image scaling through the low-level
        // lib_object accessor.
        //                                    (17.10.2010.) (Domagoj Saric)
        if ( dirty_output_dimensions_ )
        {
            jpeg_calc_output_dimensions( &const_cast<libjpeg_image &>( *this ).lib_object() );
            dirty_output_dimensions_ = false;
        }
        return point2<std::ptrdiff_t>( decompressor().output_width, decompressor().output_height );
    }

public: // Low-level (row, strip, tile) access
    void read_row( sequential_row_access_state, unsigned char * const p_row_storage ) const
    {
        read_scanline( p_row_storage );
    }

    jpeg_decompress_struct       & lib_object()       { dirty_output_dimensions_ = true; return decompressor(); }
    jpeg_decompress_struct const & lib_object() const {                                  return decompressor(); }

public: /// \ingroup Construction
    explicit libjpeg_image( memory_chunk_t & memory_chunk )
        :
        libjpeg_base( for_decompressor() )
    {
        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( libjpeg_base::error_handler_target() ) )
                libjpeg_base::throw_jpeg_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

        setup_source( memory_chunk );

        read_header();
    }

    explicit libjpeg_image( FILE & file )
        :
        libjpeg_base( for_decompressor() )
    {
        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( libjpeg_base::error_handler_target() ) )
                libjpeg_base::throw_jpeg_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

        setup_source( file );

        read_header();
    }

private: // Private interface for the base formatted_image<> class.
    // Implementation note:
    //   MSVC 10 accepts friend base_t and friend class base_t, Clang 2.8
    // accepts friend class base_t, Apple Clang 1.6 and GCC (4.2 and 4.6) accept
    // neither.
    //                                        (13.01.2011.) (Domagoj Saric)
    friend class detail::formatted_image<libjpeg_image>;

    void raw_convert_to_prepared_view( detail::view_data_t const & view_data ) const BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
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


    static std::size_t cached_format_size( format_t const format )
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
    void setup_decompression( detail::decompression_setup_data_t const & view_data ) const BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
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

        dirty_output_dimensions_ = false;

        detail::io_error_if( decompressor().data_precision != BITS_IN_JSAMPLE, "Unsupported image file data precision." );
    }

    // Unextracted "libjpeg_reader" interface.
    void setup_source()
    {
        BOOST_ASSERT( decompressor().src == NULL );
        decompressor().src = &source_manager_;
    }

    void setup_source( memory_chunk_t & memory_chunk )
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
    jpeg_source_mgr     source_manager_         ;
    mutable bool        dirty_output_dimensions_;
    array<JOCTET, 4096> read_buffer_            ;//...zzz...extract to a wrapper...not needed for in memory sources...
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
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------

#if defined(BOOST_MSVC)
    #pragma warning( pop )
#endif // MSVC

#endif // libjpeg_image_hpp
