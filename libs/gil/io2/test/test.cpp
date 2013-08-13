////////////////////////////////////////////////////////////////////////////////
///
/// test.cpp
/// --------
///
/// Copyright (c) 2010.-2013. Domagoj Saric.
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifdef _MSC_VER
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#   pragma warning( disable : 4512 ) // "conditional expression is constant"
#endif

#if !defined( _DEBUG ) && !defined( _DLL )
//#define _HAS_EXCEPTIONS 0
//#include <exception>
//#include <typeinfo>
//
//_STD_BEGIN
//
//using ::type_info;
//
//_STD_END
#endif

#ifndef _DEBUG
    #define PNG_NO_WARNINGS
    #define PNG_NO_ERROR_TEXT
    #define PNG_NO_STDIO
    #define PNG_NO_CONSOLE_IO
#endif // _DEBUG

#ifndef _DEBUG
    #define PNG_NO_CONSOLE_IO
#endif // _DEBUG

#define BOOST_EXCEPTION_DISABLE

#define BOOST_MPL_LIMIT_VECTOR_SIZE 40

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include "windows.h"
#endif // _WIN32

#define TEST_TARGET 3

#if TEST_TARGET == 1
    #include "boost/gil/extension/io/jpeg_io.hpp"
    #include "boost/gil/extension/io/png_io.hpp"
#elif TEST_TARGET == 2
    #include "boost/gil/extension/io_new/jpeg_read.hpp"
    #include "boost/gil/extension/io_new/png_read.hpp"
    #include "boost/gil/extension/io_new/tiff_read.hpp"
#elif TEST_TARGET == 3
    #ifndef _DEBUG
        #define PNG_NO_WARNINGS
        #define PNG_NO_ERROR_TEXT
        #define PNG_NO_STDIO
    #endif // _DEBUG
    #define BOOST_GIL_EXTERNAL_LIB ( BOOST_LIB_LINK_LOADTIME_OR_STATIC, BOOST_LIB_LOADING_STATIC, BOOST_LIB_INIT_ASSUME )
    #define GDIPVER 0x0110
    #define BOOST_MMAP_HEADER_ONLY

    //#include "boost/gil/extension/io2/libjpeg_image.hpp"
    //#include "boost/gil/extension/io2/libpng_image.hpp"
    #include "boost/gil/extension/io2/backends/libtiff/backend.hpp"
    #include "boost/gil/extension/io2/backends/libtiff/reader.hpp"
    #include "boost/gil/extension/io2/backends/libtiff/writer.hpp"

    #ifdef _WIN32
        //#include "boost/gil/extension/io2/gp_image.hpp"

        #include "boost/gil/extension/io2/backends/wic/backend.hpp"
        #include "boost/gil/extension/io2/backends/wic/reader.hpp"
        #include "boost/gil/extension/io2/backends/wic/writer.hpp"
    #endif // _WIN32

    #include "boost/gil/extension/io2/devices/c_file_name.hpp"
#endif // TEST_TARGET

//#include "boost/iostreams/stream.hpp"
//#include "boost/iostreams/stream_buffer.hpp"
//#include <boost/filesystem/convenience.hpp>

#include "boost/gil/image.hpp"
#include "boost/timer.hpp"

#include <cstdio>
#include "direct.h"
//------------------------------------------------------------------------------

typedef char wrchar_t;

void convert_int_to_hex( unsigned int const integer, wrchar_t * p_char_buffer )
{
    static wrchar_t const hexDigits[] = "0123456789ABCDEF";
    unsigned char const *       p_current_byte( reinterpret_cast<unsigned char const *>( &integer ) );
    unsigned char const * const p_end         ( p_current_byte + sizeof( integer )                  );
    while ( p_current_byte != p_end )
    {
        *p_char_buffer-- = hexDigits[ ( ( *p_current_byte ) & 0x0F )      ];
        *p_char_buffer-- = hexDigits[ ( ( *p_current_byte ) & 0xF0 ) >> 4 ];
        ++p_current_byte;
    }
}


extern "C" int __cdecl main( int /*argc*/, char * /*argv*/[] )
{
#ifdef _WIN32
    ::SetPriorityClass( ::GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
#elif defined( __APPLE__ )
	BOOST_VERIFY( ::setpriority( PRIO_PROCESS, 0, -20 ) );
#endif // _WIN32

    using namespace boost;
    using namespace boost::gil;
    using namespace boost::gil::io;

    //gp_image::guard const lib_guard;
    wic_image::guard const wic_guard;

	//image<cmyk8_pixel_t, false> cmyki;
	//read_and_convert_image( "quad-tile.tif", cmyki, tiff_tag () );
	//write_view( "ktt.tif", cmyki._view, image_write_info<tiff_tag>() );
	//return 0;

	//libjpeg_image::read( "stlab2007.jpg", jpeg_test_image );
	//libpng_image ::read( "boost.png"    , png_test_image  );

    typedef image<rgb8_pixel_t, false> tile_holder_t;

    //for ( unsigned int i( 0 ); i < 10000; ++i )
    //{
        #if TEST_TARGET == 1
            jpeg_read_and_convert_image( "stlab2007.jpg", jpeg_test_image );
            png_read_and_convert_image( "boost.png"     , png_test_image  );
        #elif TEST_TARGET == 2
            using namespace boost::iostreams;
            stream<array_source> bufferStream( reinterpret_cast<char const *>( foldericon_png ), sizeof( foldericon_png ) );
            read_and_convert_image( bufferStream   , png_test_image , png_tag () );
            read_and_convert_image( "stlab2007.jpg", jpeg_test_image, jpeg_tag() );
            read_and_convert_image( "boost.png"    , png_test_image , png_tag () );
        #elif TEST_TARGET == 3

		//libjpeg_image::reader_for<char const *>::type your_image( "stlab2007.jpg" );
		//your_image.lib_object().dct_method = JDCT_IFAST;
		//your_image.lib_object().scale_num  = 4;

        //typedef wic_image ttt;
        //((ttt *)NULL)->row_size();
        //((ttt *)NULL)->format() == ttt::native_format<rgb8_pixel_t, false>::value;
        //((ttt *)NULL)->can_do_row_access();

        //std::size_t const input_tile_size( 5000 );
        //tile_holder_t input_tile_holder( input_tile_size, input_tile_size );

        #ifdef _WIN32
            typedef wic_image::reader_for<wchar_t const *>::type wreader_t;
        #endif // _WIN32
        //typedef wic_image::reader_for<char const *>::type reader_t;
        typedef libtiff_image::reader_for<char const *>::type reader_t;
        reader_t reader( BOOST_TEST_GIL_IO_IMAGES_PATH "/tiff/TrueMarble.250m.21600x21600.E2.tif" );
        point2<uint32> const input_dimensions( reader.dimensions() );
        //BOOST_ASSERT( input_dimensions.x >= input_tile_size );
        //BOOST_ASSERT( input_dimensions.y >= input_tile_size );

        reader_t::sequential_tile_read_state state( reader.begin_sequential_tile_access() );

        point2<uint32> const output_tile_dimensions( 512, 512/*reader.tile_dimensions()*/ );
        tile_holder_t output_tile_holder( output_tile_dimensions.x, output_tile_dimensions.y );
        //typedef libjpeg_image::writer_for<wrchar_t const *>::type writer_t;
        typedef wic_image::writer_for<wrchar_t const *>::type writer_t;

		BOOST_VERIFY( /*std*/::mkdir( BOOST_TEST_GIL_IO_IMAGES_PATH "/_test_output" ) == 0 || errno == EEXIST );
        wrchar_t output_file_name[] = BOOST_TEST_GIL_IO_IMAGES_PATH "/_test_output/__out_tile__00000000.jpg";
        wrchar_t * const p_back_of_file_name_number( boost::end( output_file_name ) - ( boost::size( ".jpg" ) + 1 ) );

        boost::timer benchmark_timer;

        unsigned int output_tile_counter( 0 );
        for ( unsigned int x( 0 ); x < static_cast<unsigned>( input_dimensions.x ); x += output_tile_dimensions.x )
        {
            for ( unsigned int y( 0 ); y < static_cast<unsigned>( input_dimensions.y ); y += output_tile_dimensions.y )
            {
            #ifdef _WIN323
                reader.copy_to
                (
                    //view( output_tile_holder ),
                    reader_t::offset_view
                    (
                        view( output_tile_holder ),
                        reader_t::offset_t( x, y )
                    ),
                    assert_dimensions_match(),
                    assert_formats_match   ()
                    //...or...ensure_formats_match   ()
                    //...or...synchronize_formats    ()
                );
            #endif // _WIN32
                reader.read_tile( state, interleaved_view_get_raw_data( view( output_tile_holder ) ) );
                BOOST_ASSERT( !state.failed() );

                convert_int_to_hex( output_tile_counter++, p_back_of_file_name_number );
                //libjpeg_image::write( output_file_name, view( output_tile_holder ) );
                //wic_image::write( output_file_name, view( output_tile_holder ) );
                writer_t( output_file_name, view( output_tile_holder ) ).write_default();
            }
        //}

        #endif // TEST_TARGET

        BOOST_ASSERT( !state.failed() );
    }

    std::printf( "%u ms\n", static_cast<unsigned int>( benchmark_timer.elapsed() * 1000 ) );

    return EXIT_SUCCESS;
}
