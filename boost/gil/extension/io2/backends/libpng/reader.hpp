////////////////////////////////////////////////////////////////////////////////
///
/// \file libpng/reader.hpp
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
#ifndef reader_hpp__9E0C99E6_EAE8_4D71_844B_93518FFCB5CE
#define reader_hpp__9E0C99E6_EAE8_4D71_844B_93518FFCB5CE
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

////////////////////////////////////////////////////////////////////////////////
///
/// \class libpng_reader
///
/// \brief
///
////////////////////////////////////////////////////////////////////////////////

class libpng_reader
    :
    public libpng_image
{
public:
    struct guard {};

public: /// \ingroup Construction
	template <class Device>
    explicit libpng_reader( Device & device )
        :
        libpng_image( ::png_create_read_struct_2( PNG_LIBPNG_VER_STRING, NULL, &detail::png_error_function, &detail::png_warning_function, NULL, NULL, NULL ) )
    {
        if ( !successful_creation() )
            cleanup_and_throw_libpng_error();

    #ifndef PNG_NO_STDIO
		if ( boost::is_same<Device, ::FILE>::value )
			::png_init_io( &png_object(), &device );
		else
    #else
		//png_rw_ptr
		::png_set_read_fn( &png_object(), &device, &png_read_data<Device> );
    #endif

        init();
    }


    ~libpng_reader()
    {
        destroy_read_struct();
    }

public: // Low-level (row, strip, tile) access
    void read_row( sequential_row_read_state, unsigned char * const p_row_storage ) const
    {
        read_row( p_row_storage );
    }

private: // Private backend_base interface.
    // Implementation note:
    //   MSVC 10 accepts friend base_t and friend class base_t, Clang 2.8
    // accepts friend class base_t, Apple Clang 1.6 and GCC (4.2 and 4.6) accept
    // neither.
    //                                        (13.01.2011.) (Domagoj Saric)
    friend class detail::backend<libpng_image>;

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
        if ( ( colour_type == PNG_COLOR_TYPE_GRAY ) && ( bit_depth < 8 ) )
        {
            ::png_set_expand_gray_1_2_4_to_8( &png_object() );
        }

        if ( !( colour_type & PNG_COLOR_MASK_ALPHA ) )
            ::png_set_strip_alpha( &png_object() );

        if ( ::png_get_valid( &png_object(), &info_object(), PNG_INFO_tRNS ) )
            ::png_set_tRNS_to_alpha( &png_object() );

        if ( bit_depth == 8 )
            ::png_set_strip_16( &png_object() );

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

    unsigned int cached_format_size( format_t const format ) const
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

        memory_range_t & memory_chunk( *static_cast<memory_range_t *>( png_ptr->io_ptr ) );

        if ( length <= static_cast<std::size_t>( memory_chunk.size() ) )
        {
            std::memcpy( data, memory_chunk.begin(), length );
            memory_chunk.advance_begin( length );
        }
        else
            detail::png_error_function( png_ptr, "Read Error" );
    }
}; // class libpng_reader

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // reader_hpp
