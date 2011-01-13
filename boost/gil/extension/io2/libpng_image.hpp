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
struct gil_to_libpng_format : mpl::integral_c<unsigned int, formatted_image_base::unsupported_format> {};

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
        buffer_( formatted_image_base::get_raw_data( view ) ),
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
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class libpng_base
///
////////////////////////////////////////////////////////////////////////////////

class libpng_base : public lib_object_t
{
public:
    typedef lib_object_t lib_object_t;

    lib_object_t & lib_object() { return *this; }

protected:
    libpng_base( png_struct * const p_png )
        :
        lib_object_t( p_png, ::png_create_info_struct( p_png ) )
    {}

    //...zzz...forced by LibPNG into either duplication or this anti-pattern...
    bool successful_creation() const { return is_valid(); }

    static std::size_t format_bit_depth( libpng_view_data_t::format_t const format )
    {
        return ( format >> 16 ) & 0xFF;
    }

    static std::size_t format_colour_type( libpng_view_data_t::format_t const format )
    {
        return format & 0xFF;
    }

    #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
        jmp_buf & error_handler_target() const { return png_object().jmpbuf; }
    #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class libpng_writer
///
/// \brief
///
////////////////////////////////////////////////////////////////////////////////

class libpng_writer
    :
    public libpng_base,
    public configure_on_write_writer
{
public:
    void write_default( libpng_view_data_t const & view )
    {
        ::png_set_IHDR
        (
            &png_object ()                    ,
            &info_object()                    ,
            view.width_                       ,
            view.height_                      ,
            format_bit_depth  ( view.format_ ),
            format_colour_type( view.format_ ),
            PNG_INTERLACE_NONE                ,
            PNG_COMPRESSION_TYPE_DEFAULT      ,
            PNG_FILTER_TYPE_DEFAULT
        );

        //::png_set_invert_alpha( &png_object() );

        write( view );
    }

    void write( libpng_view_data_t const & view ) BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
    {
        BOOST_ASSERT( view.format_ != JCS_UNKNOWN );

        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
		if ( setjmp( error_handler_target() ) )
                detail::throw_libpng_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

        if ( little_endian() )
            ::png_set_swap( &png_object() );

        ::png_write_info( &png_object(), &info_object() );
        
        png_byte *       p_row( view.buffer_ );
        png_byte * const p_end( memunit_advanced( view.buffer_, view.height_ * view.stride_ ) );
        while ( p_row < p_end )
        {
            ::png_write_row( &png_object(), p_row );
            memunit_advance( p_row, view.stride_ );
        }

        ::png_write_end( &png_object(), 0 );
    }

protected:
    libpng_writer( void * const p_target_object, png_rw_ptr const write_data_fn, png_flush_ptr const output_flush_fn )
        :
        libpng_base( ::png_create_write_struct_2( PNG_LIBPNG_VER_STRING, NULL, &detail::png_error_function, &detail::png_warning_function, NULL, NULL, NULL ) )
    {
        if ( !successful_creation() )
            cleanup_and_throw_libpng_error();

        setup_destination( p_target_object, write_data_fn, output_flush_fn );
    }

    ~libpng_writer()
    {
        destroy_write_struct();
    }

private:
    void destroy_write_struct() { ::png_destroy_write_struct( &png_object_for_destruction(), &info_object_for_destruction() ); }

    void cleanup_and_throw_libpng_error()
    {
        destroy_write_struct();
        detail::throw_libpng_error();
    }

    void setup_destination( void * const p_target_object, png_rw_ptr const write_data_fn, png_flush_ptr const output_flush_fn )
    {
        ::png_set_write_fn( &png_object(), p_target_object, write_data_fn, output_flush_fn );
    }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class libpng_writer_FILE
///
/// \brief
///
////////////////////////////////////////////////////////////////////////////////

class libpng_writer_FILE : public libpng_writer
{
public:
    libpng_writer_FILE( FILE & file )
        :
        libpng_writer( &file, &png_write_data, &png_flush_data )
    {}

private:
    static void PNGAPI png_write_data( png_structp const png_ptr, png_bytep const data, png_size_t const length )
    {
        BOOST_ASSERT( png_ptr );

        png_size_t const written_size
        (
            static_cast<png_size_t>( std::fwrite( data, 1, length, static_cast<FILE *>( png_ptr->io_ptr ) ) )
        );

        if ( written_size != length )
            png_error_function( png_ptr, "Write Error" );
    }

    static void PNGAPI png_flush_data( png_structp const png_ptr )
    {
        BOOST_ASSERT( png_ptr );

        std::fflush( static_cast<FILE *>( png_ptr->io_ptr ) );
    }
};

//------------------------------------------------------------------------------
} // namespace detail


class libpng_image;

template <>
struct formatted_image_traits<libpng_image>
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
                mpl::pair<memory_chunk_t        , detail::seekable_input_memory_range_extender<libpng_image> >,
                mpl::pair<FILE                  ,                                              libpng_image  >,
                mpl::pair<char           const *, detail::input_c_str_for_mmap_extender       <libpng_image> >
            > readers;

    typedef mpl::map2
            <
                mpl::pair<FILE        ,                                          detail::libpng_writer_FILE  >,
                mpl::pair<char const *, detail::output_c_str_for_c_file_extender<detail::libpng_writer_FILE> >
            > writers;

    typedef mpl::vector1_c<format_tag, png> supported_image_formats;

    typedef view_data_t writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true             );
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class libpng_image
///
/// \brief
///
////////////////////////////////////////////////////////////////////////////////

class libpng_image
    :
    public  detail::formatted_image<libpng_image>,
    private detail::libpng_base
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

    std::size_t pixel_size() const
    {
        return number_of_channels() * bit_depth() / 8;
    }

public: /// \ingroup Construction
    explicit libpng_image( FILE & file )
        :
        libpng_base( ::png_create_read_struct_2( PNG_LIBPNG_VER_STRING, NULL, &detail::png_error_function, &detail::png_warning_function, NULL, NULL, NULL ) )
    {
        if ( !successful_creation() )
            cleanup_and_throw_libpng_error();

        #ifdef PNG_NO_STDIO
            ::png_set_read_fn( &png_object(), &file, &png_FILE_read_data );
        #else
            ::png_init_io( &png_object(), &file );
        #endif

        init();
    }

    explicit libpng_image( memory_chunk_t & in_memory_image )
        :
        libpng_base( ::png_create_read_struct_2( PNG_LIBPNG_VER_STRING, NULL, &detail::png_error_function, &detail::png_warning_function, NULL, NULL, NULL ) )
    {
        if ( !successful_creation() )
            cleanup_and_throw_libpng_error();
        //png_rw_ptr
        ::png_set_read_fn( &png_object(), &in_memory_image, &png_memory_chunk_read_data );

        init();
    }

    ~libpng_image()
    {
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

public: // Low-level (row, strip, tile) access
    void read_row( sequential_row_access_state, unsigned char * const p_row_storage ) const
    {
        read_row( p_row_storage );
    }

    png_struct & lib_object() const { return png_object(); }

private: // Private formatted_image_base interface.
    // Implementation note:
    //   MSVC 10 accepts friend base_t and friend class base_t, Clang 2.8
    // accepts friend class base_t, Apple Clang 1.6 and GCC (4.2 and 4.6) accept
    // neither.
    //                                        (13.01.2011.) (Domagoj Saric)
    friend class detail::formatted_image<libpng_image>;

    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
    {
        using namespace detail;

        std::size_t            const row_length  ( ::png_get_rowbytes( &png_object(), &info_object() ) );
        scoped_array<png_byte> const p_row_buffer( new png_byte[ row_length ]                          );

        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( error_handler_target() ) )
                detail::throw_libpng_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

        unsigned int number_of_passes( ::png_set_interlace_handling( &png_object() ) );
        BF_ASSUME( ( number_of_passes == 1 ) || ( number_of_passes == 7 ) );
        BOOST_ASSERT( ( number_of_passes == 1 ) && "Missing interlaced support for the generic conversion case." );
        ignore_unused_variable_warning( number_of_passes );

        if ( is_offset_view<TargetView>::value )
            skip_rows( get_offset<offset_t>( view ) );

        png_byte       * const p_row    ( p_row_buffer.get() );
        png_byte const * const p_row_end( p_row + row_length );

        unsigned int const rows_to_read( original_view( view ).dimensions().y );
        for ( unsigned int row_index( 0 ); row_index < rows_to_read; ++row_index )
        {
            read_row( p_row );

            typedef typename MyView::value_type pixel_t;
            typedef typename get_original_view_t<TargetView>::type::x_iterator x_iterator;

            pixel_t    const * p_source_pixel( gil_reinterpret_cast_c<pixel_t const *>( p_row ) );
            x_iterator         p_target_pixel( original_view( view ).row_begin( row_index )     );
            while ( p_source_pixel < gil_reinterpret_cast_c<pixel_t const *>( p_row_end ) )
            {
                converter( *p_source_pixel, *p_target_pixel );
                ++p_source_pixel;
                ++p_target_pixel;
            }
        }
    }

    void raw_convert_to_prepared_view( detail::libpng_view_data_t const & view_data ) const
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

    void raw_copy_to_prepared_view( detail::libpng_view_data_t const view_data ) const BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
    {
        #ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            if ( setjmp( error_handler_target() ) )
                detail::throw_libpng_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

        unsigned int number_of_passes( ::png_set_interlace_handling( &png_object() ) );
        BF_ASSUME( ( number_of_passes == 1 ) || ( number_of_passes == 7 ) );

        skip_rows( view_data.offset_ );

        while ( number_of_passes-- )
        {
            png_byte       *       p_row( view_data.buffer_                                 );
            png_byte const * const p_end( p_row + ( view_data.height_ * view_data.stride_ ) );
            while ( p_row < p_end )
            {
                read_row( p_row );
                memunit_advance( p_row, view_data.stride_ );
            }
        }
    }

    std::size_t cached_format_size( format_t const format ) const
    {
        return number_of_channels() * format_bit_depth( format ) / 8;
    }

private:
    void cleanup_and_throw_libpng_error()
    {
        destroy_read_struct();
        detail::throw_libpng_error();
    }

    void init() BOOST_GIL_CAN_THROW //...zzz...a plain throw(...) would be enough here but it chokes GCC...
    {
        #ifdef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            try
            {
        #else
            if ( setjmp( error_handler_target() ) )
                cleanup_and_throw_libpng_error();
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED

            ::png_read_info( &png_object(), &info_object() );
            if ( little_endian() )
                ::png_set_swap( &png_object() );

        #ifdef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
            }
            catch (...)
            {
                destroy_read_struct();
                throw;
            }
        #endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    }

    void skip_rows( unsigned int const row_to_skip_to ) const
    {
        BOOST_ASSERT( ( row_to_skip_to >= png_object().row_number ) && "No 'rewind' capability for LibPNG yet." );

        unsigned int number_of_rows_to_skip( row_to_skip_to - png_object().row_number );
        while ( number_of_rows_to_skip-- )
        {
            BF_ASSUME( row_to_skip_to != 0 );
            read_row( NULL );
        }
    }

    unsigned int number_of_channels() const { return ::png_get_channels ( &png_object(), &info_object() ); }
    std::size_t  bit_depth         () const { return ::png_get_bit_depth( &png_object(), &info_object() ); }

    void destroy_read_struct() { ::png_destroy_read_struct( &png_object_for_destruction(), &info_object_for_destruction(), NULL ); }

    void read_row( png_byte * const p_row ) const BOOST_GIL_CAN_THROW
    {
        ::png_read_row( &png_object(), p_row, NULL );
    }

    static void PNGAPI png_FILE_read_data( png_structp const png_ptr, png_bytep const data, png_size_t const length )
    {
        BOOST_ASSERT( png_ptr         );
        BOOST_ASSERT( png_ptr->io_ptr );

        png_size_t const read_size
        (
            static_cast<png_size_t>( std::fread( data, 1, length, static_cast<FILE *>( png_ptr->io_ptr ) ) )
        );

        if ( read_size != length )
            detail::png_error_function( png_ptr, "Read Error" );
    }

    static void PNGAPI png_memory_chunk_read_data( png_structp const png_ptr, png_bytep const data, png_size_t const length )
    {
        BOOST_ASSERT( png_ptr         );
        BOOST_ASSERT( png_ptr->io_ptr );

        memory_chunk_t & memory_chunk( *static_cast<memory_chunk_t *>( png_ptr->io_ptr ) );

        if ( length <= static_cast<std::size_t>( memory_chunk.size() ) )
        {
            std::memcpy( data, memory_chunk.begin(), length );
            memory_chunk.advance_begin( length );
        }
        else
            detail::png_error_function( png_ptr, "Read Error" );
    }
};


//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libpng_image_hpp
