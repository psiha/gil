////////////////////////////////////////////////////////////////////////////////
///
/// \file libjpeg_image.hpp
/// -----------------------
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
#ifndef writer_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
#define writer_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
#pragma once
//------------------------------------------------------------------------------
#include "backend.hpp"
#include "../detail/writer.hpp"

#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/libx_shared.hpp"
#include "boost/gil/extension/io2/detail/platform_specifics.hpp"
#include "boost/gil/extension/io2/detail/shared.hpp"

#include <boost/array.hpp>
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------

class libjpeg_writer
    :
    public libjpeg_image,
    public configure_on_write_writer
{
protected:
    BOOST_STATIC_CONSTANT( bool, auto_closes_device = true );

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

    void do_write( view_data_t const & view ) BOOST_GIL_CAN_THROW
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

        compressor().client_data = reinterpret_cast<void *>( static_cast<std::intptr_t>( file_descriptor ) );

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
                static_cast<int>( reinterpret_cast<std::intptr_t>( compressor().client_data ) ),
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
        BOOST_VERIFY( /*std*/::close( reinterpret_cast<long>( get_writer( p_cinfo ).compressor().client_data ) ) == 0 );
    }

private:
    jpeg_destination_mgr        destination_manager_;
    array<unsigned char, 65536> write_buffer_       ;
}; // class libjpeg_writer

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // writer_hpp
