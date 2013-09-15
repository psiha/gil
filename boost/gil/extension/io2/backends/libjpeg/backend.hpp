////////////////////////////////////////////////////////////////////////////////
///
/// \file backend.hpp
/// -----------------
///
/// Copyright (c) Domagoj Saric 2010.-2013
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
#ifndef backend_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
#define backend_hpp__7C5F6951_A00F_4E0D_9783_488A49B1CA2B
#pragma once
//------------------------------------------------------------------------------
#include "boost/gil/extension/io2/backends/detail/backend.hpp"

#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/libx_shared.hpp"
#include "boost/gil/extension/io2/detail/platform_specifics.hpp"
#include "boost/gil/extension/io2/detail/shared.hpp"

#include <boost/array.hpp>
#include <boost/range/size.hpp>
#include <boost/smart_ptr/scoped_array.hpp>

#define JPEG_INTERNALS
#include "jpeglib.h"
#undef JPEG_INTERNALS

#if defined(BOOST_MSVC)
    #pragma warning( push )
    #pragma warning( disable : 4996 ) // "The POSIX name for this item is deprecated. Instead, use the ISO C++ conformant name."
    #include "io.h"
    #include "sys/stat.h"
#endif // MSVC
#include "fcntl.h"

#ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
#include <csetjmp>
#endif // BOOST_GIL_THROW_THROUGH_C_SUPPORTED
#include <cstddef>
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

//------------------------------------------------------------------------------
} // namespace detail


class libjpeg_image;

template <>
struct backend_traits<libjpeg_image>
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
                mpl::pair<memory_range_t        , detail::seekable_input_memory_range_extender<libjpeg_image> >,
                mpl::pair<FILE                  ,                                              libjpeg_image  >,
                mpl::pair<char           const *, detail::input_c_str_for_mmap_extender       <libjpeg_image> >
            > native_sources;

    typedef mpl::map2
            <
                mpl::pair<FILE        , detail::libjpeg_writer>,
                mpl::pair<char const *, detail::libjpeg_writer>
            > native_sinks;

    typedef mpl::vector1_c<format_tag, jpeg> supported_image_formats;

    typedef view_data_t writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true             );
}; // struct backend_traits<libjpeg_image>


////////////////////////////////////////////////////////////////////////////////
///
/// \class libjpeg_image
///
////////////////////////////////////////////////////////////////////////////////

#if defined( BOOST_MSVC )
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

class libjpeg_image
	:
	private detail::libjpeg_object_wrapper_t,
	public  detail::backend<libjpeg_image>
{
public:
	struct guard {};

public: /// \ingroup Information
    typedef point2<unsigned int> dimensions_t;

    dimensions_t dimensions() const
    {
        BOOST_STATIC_ASSERT( offsetof( jpeg_compress_struct, image_width  ) == offsetof( jpeg_decompress_struct, image_width  ) );
        BOOST_STATIC_ASSERT( offsetof( jpeg_compress_struct, image_height ) == offsetof( jpeg_decompress_struct, image_height ) );
        return dimensions_t( decompressor().image_width, decompressor().image_height );
    }

    unsigned int number_of_channels() const
    {
        BOOST_STATIC_ASSERT( offsetof( jpeg_compress_struct, num_components ) == offsetof( jpeg_decompress_struct, num_components ) );
        return decompressor().num_components;
    }

    format_t format() const
    {
        BOOST_STATIC_ASSERT( offsetof( jpeg_compress_struct, jpeg_color_space ) == offsetof( jpeg_decompress_struct, jpeg_color_space ) );
        return decompressor().jpeg_color_space;
    }

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
            default        : return current_format;

            case JCS_BG_RGB:
            case JCS_BG_YCC:
            case JCS_YCbCr : return JCS_RGB       ;
            case JCS_YCCK  : return JCS_CMYK      ;
        }
    }

    image_type_id_t current_image_format_id() const
    {
        return image_format_id( closest_gil_supported_format() );
    }


	static image_type_id_t image_format_id( format_t const closest_gil_supported_format )
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

	~libjpeg_base() BOOST_BF_NOTHROW { jpeg_destroy( &common() ); }

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

    jpeg_error_mgr jerr_;
}; // class libjpeg_image

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

#if defined( BOOST_MSVC )
    #pragma warning( pop )
#endif // MSVC

#endif // backend_hpp
