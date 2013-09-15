////////////////////////////////////////////////////////////////////////////////
///
/// \file libpng/writer.hpp
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
#ifndef writer_hpp__9E0C99E6_EAE8_4D71_844B_93518FFCB5CE
#define writer_hpp__9E0C99E6_EAE8_4D71_844B_93518FFCB5CE
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
/// \class libpng_writer
///
////////////////////////////////////////////////////////////////////////////////

class libpng_writer
    :
    public libpng_image,
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

    void write( libpng_view_data_t const & view ) BOOST_GIL_CAN_THROW
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
        libpng_image( ::png_create_write_struct_2( PNG_LIBPNG_VER_STRING, NULL, &detail::png_error_function, &detail::png_warning_function, NULL, NULL, NULL ) )
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
}; // class libpng_writer


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
}; // class libpng_writer_FILE

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // writer_hpp
