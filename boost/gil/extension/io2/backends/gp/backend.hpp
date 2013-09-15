////////////////////////////////////////////////////////////////////////////////
///
/// \file gp/backend.hpp
/// --------------------
///
/// Base IO interface GDI+ implementation.
///
/// Copyright (c) 2010.-2013. Domagoj Saric
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
#ifndef backend_hpp__3B1ED5BC_42C6_4EC6_B700_01C1B8646431
#define backend_hpp__3B1ED5BC_42C6_4EC6_B700_01C1B8646431
#pragma once
//------------------------------------------------------------------------------
#include "detail/io_error.hpp"
#include "detail/gp_extern_lib_guard.hpp"
#include "detail/windows_shared.hpp"
#include "detail/windows_shared_istreams.hpp"
#include "backend.hpp"

#include <boost/array.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_pod.hpp>

#include <algorithm>
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

template <Gdiplus::PixelFormat gp_format> struct is_canonical            : mpl::bool_ <(gp_format & PixelFormatCanonical) != 0> {};
template <Gdiplus::PixelFormat gp_format> struct is_extended             : mpl::bool_ <(gp_format & PixelFormatExtended ) != 0> {};
template <Gdiplus::PixelFormat gp_format> struct is_indexed              : mpl::bool_ <(gp_format & PixelFormatIndexed  ) != 0> {};
template <Gdiplus::PixelFormat gp_format> struct has_alpha               : mpl::bool_ <(gp_format & PixelFormatAlpha    ) != 0> {};
template <Gdiplus::PixelFormat gp_format> struct has_premultiplied_alpha : mpl::bool_ <(gp_format & PixelFormatPAlpha   ) != 0> {};
template <Gdiplus::PixelFormat gp_format> struct is_supported            : mpl::bool_ <(gp_format & PixelFormatGDI      ) != 0 || ( ( gp_format == PixelFormat32bppCMYK ) && ( GDIPVER >= 0x0110 ) )> {};
template <Gdiplus::PixelFormat gp_format> struct pixel_size              : mpl::size_t<( gp_format >> 8 ) & 0xff              > {};


/// @see GdiplusPixelFormats.h: ARGB -> little endian BGRA
typedef bgra_layout_t gp_alpha_layout_t;
typedef bgr_layout_t  gp_layout_t;

typedef packed_pixel_type<uint16_t, mpl::vector3_c<unsigned,5,6,5>, gp_layout_t>::type gp_bgr565_pixel_t;

template <typename Pixel, bool IsPlanar>
struct gil_to_gp_format : mpl::integral_c<Gdiplus::PixelFormat, PixelFormatUndefined> {};

template <> struct gil_to_gp_format<gp_bgr565_pixel_t, false> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat16bppRGB565   > {};
template <> struct gil_to_gp_format<bgr8_pixel_t     , false> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat24bppRGB      > {};
template <> struct gil_to_gp_format<bgra8_pixel_t    , false> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat32bppARGB     > {};
template <> struct gil_to_gp_format<gray16_pixel_t   , false> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat16bppGrayScale> {};
template <> struct gil_to_gp_format<bgr16_pixel_t    , false> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat48bppRGB      > {};
template <> struct gil_to_gp_format<bgra16_pixel_t   , false> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat64bppARGB     > {};
#if (GDIPVER >= 0x0110)
template <> struct gil_to_gp_format<cmyk8_pixel_t    , false> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat32bppCMYK     > {};
#endif // (GDIPVER >= 0x0110)


template <typename Pixel, bool IsPlanar>
struct gp_is_supported : is_supported<gil_to_gp_format<Pixel, IsPlanar>::value>{};


typedef mpl::
#if ( GDIPVER >= 0x0110 )
    vector4
#else
    vector3
#endif
<
    image<bgr8_pixel_t     , false>,
    image<bgra8_pixel_t    , false>,
    image<gp_bgr565_pixel_t, false>
    #if ( GDIPVER >= 0x0110 )
    ,image<cmyk8_pixel_t   , false>
    #endif
> gp_supported_pixel_formats;


char const * error_string( Gdiplus::GpStatus const status )
{
    using namespace Gdiplus;
    switch ( status )
    {
        case GenericError              : return "Generic GDI+ error";
        case OutOfMemory               : return "Out of memory";
        case ObjectBusy                : return "Object busy";
        case InsufficientBuffer        : return "Insufficient buffer";
        case NotImplemented            : return "Not implemented";
        case Win32Error                : return "Win32 subsystem failure";
        case Aborted                   : return "Aborted";
        case FileNotFound              : return "File not found";
        case ValueOverflow             : return "Numeric overflow";
        case AccessDenied              : return "Access denied";
        case UnknownImageFormat        : return "Unknown image format";
        case FontFamilyNotFound        : return "Font family not found";
        case FontStyleNotFound         : return "Font style not found";
        case NotTrueTypeFont           : return "A non TrueType font specified";
        case UnsupportedGdiplusVersion : return "Unsupported GDI+ version";
        case PropertyNotFound          : return "Specified property does not exist in the image";
        case PropertyNotSupported      : return "Specified property not supported by the format of the image";
        #if ( GDIPVER >= 0x0110 )
        case ProfileNotFound           : return "Profile required to save an image in CMYK format not found";
        #endif //(GDIPVER >= 0x0110)
    }

    // Programmer errors or GIL::IO2 bugs:
    switch ( status )
    {
        case Ok                        : BOOST_ASSERT( !"Should not be called for no error" ); BF_UNREACHABLE_CODE break;
        case InvalidParameter          : BOOST_ASSERT( !"Invalid parameter"                 ); BF_UNREACHABLE_CODE break;
        case WrongState                : BOOST_ASSERT( !"Object in wrong state"             ); BF_UNREACHABLE_CODE break;
        case GdiplusNotInitialized     : BOOST_ASSERT( !"GDI+ not initialized"              ); BF_UNREACHABLE_CODE break;

        default: BOOST_ASSERT( !"Unknown GDI+ status code." ); BF_UNREACHABLE_CODE break;
    }
}

inline void ensure_result( Gdiplus::GpStatus const result )
{
    if ( result != Gdiplus::Ok )
        io_error( error_string( result ) );
}

inline void verify_result( Gdiplus::GpStatus const result )
{
    BOOST_VERIFY( result == Gdiplus::Ok );
}


class gp_roi : public Gdiplus::Rect
{
public:
    typedef INT                value_type;
    typedef point2<value_type> point_t   ;

    typedef point_t            offset_t  ;

public:
    gp_roi( value_type const x, value_type const y, value_type const width, value_type const height )
        : Gdiplus::Rect( x, y, width, height ) {}

    gp_roi( offset_t const offset, value_type const width, value_type const height )
        : Gdiplus::Rect( Gdiplus::Point( offset.x, offset.y ), Gdiplus::Size( width, height ) ) {}

    gp_roi( offset_t const top_left, offset_t const bottom_right )
        : Gdiplus::Rect( Gdiplus::Point( top_left.x, top_left.y ), Gdiplus::Size( bottom_right.x - top_left.x, bottom_right.y - top_left.y ) ) {}
}; // class gp_roi

class gp_view_base;

//------------------------------------------------------------------------------
} // namespace detail

class gp_image;

template <>
struct backend_traits<gp_image>
{
    typedef Gdiplus::PixelFormat format_t;

    typedef detail::gp_supported_pixel_formats supported_pixel_formats_t;

    typedef detail::gp_roi roi_t;

    struct gil_to_native_format
    {
        template <typename Pixel, bool IsPlanar>
        struct apply : detail::gil_to_gp_format<Pixel, IsPlanar> {};
    };

    template <typename Pixel, bool IsPlanar>
    struct is_supported : detail::gp_is_supported<Pixel, IsPlanar> {};

    typedef mpl::map5
    <
        mpl::pair<char           const *,                                           gp_image  >,
        mpl::pair<wchar_t        const *,                                           gp_image  >,
        mpl::pair<IStream               ,                                           gp_image  >,
        mpl::pair<FILE                  , detail::input_FILE_for_IStream_extender  <gp_image> >,
        mpl::pair<memory_range_t        , detail::memory_chunk_for_IStream_extender<gp_image> >
    > native_sources;

    typedef mpl::map4
    <
        mpl::pair<char    const *, detail::gp_writer>,
        mpl::pair<wchar_t const *, detail::gp_writer>,
        mpl::pair<IStream        , detail::gp_writer>,
        mpl::pair<FILE           , detail::gp_writer>
    > native_sinks;

    typedef mpl::vector5_c<format_tag, bmp, gif, jpeg, png, tiff> supported_image_formats;

    class view_data_t : public Gdiplus::BitmapData
    {
    public:
        template <typename View>
        view_data_t( View const & view ) : p_roi_( 0 ) { set_bitmapdata_for_view( view ); }

        template <typename View>
        view_data_t( View const & view, roi_t::offset_t const & offset )
            :
            p_roi_( static_cast<roi_t const *>( optional_roi_.address() ) )
        {
            set_bitmapdata_for_view( view );
            new ( optional_roi_.address() ) roi_t( offset, Width, Height );
        }

        void set_format( format_t const format ) { PixelFormat = format; }

    public:
        Gdiplus::Rect const * const p_roi_;

    private:
        template <typename View>
        void set_bitmapdata_for_view( View const & view )
        {
            using namespace detail;

            BOOST_STATIC_ASSERT(( is_supported<typename View::value_type, is_planar<View>::value>::value ));

            Width       = view.width ();
            Height      = view.height();
            Stride      = view.pixels().row_size();
            PixelFormat = detail::gil_to_gp_format<typename View::value_type, is_planar<View>::value>::value;
            Scan0       = backend_base::get_raw_data( view );
            Reserved    = 0;
        }

        void operator=( view_data_t const & );

    private:
        aligned_storage<sizeof( roi_t ), alignment_of<roi_t>::value>::type optional_roi_;
    }; // class view_data_t

    typedef view_data_t writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( Gdiplus::ARGB ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true                    );
}; // struct backend_traits<gp_image>


#if defined( BOOST_MSVC )
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

class gp_image
    :
    private detail::gp_guard,
    public  detail::backend<gp_image>
{
public:
    typedef detail::gp_user_guard guard;

protected: /// \ingroup Construction
    ~gp_image()
    {
        detail::verify_result( Gdiplus::DllExports::GdipDisposeImage( pBitmap_ ) );
    }

public: /// \ingroup Information
    typedef point2<unsigned int> dimensions_t;

    dimensions_t dimensions() const
    {
        using namespace Gdiplus;
        REAL width_float, height_float;
        detail::verify_result( DllExports::GdipGetImageDimension( const_cast<GpBitmap *>( pBitmap_ ), &width_float, &height_float ) );
    #ifndef NDEBUG
        BOOST_ASSERT( static_cast<unsigned int>( width_float  ) == width_float  );
        BOOST_ASSERT( static_cast<unsigned int>( height_float ) == height_float );
        UINT width ; detail::verify_result( DllExports::GetWidth ( const_cast<GpBitmap *>( pBitmap_ ), &width  ) );
        UINT height; detail::verify_result( DllExports::GetHeight( const_cast<GpBitmap *>( pBitmap_ ), &height ) );
        BOOST_ASSERT( width_float  == width  );
        BOOST_ASSERT( height_float == height );
    #endif // NDEBUG
        return dimensions_t
        (
            static_cast<dimensions_t::value_type>( width_float  ),
            static_cast<dimensions_t::value_type>( height_float )
        );
    }

    format_t format() const
    {
        format_t pixel_format;
        detail::verify_result( Gdiplus::DllExports::GdipGetImagePixelFormat( pBitmap_, &pixel_format ) );
        return pixel_format;
    }

    format_t closest_gil_supported_format() const
    {
        //http://www.tech-archive.net/Archive/Development/microsoft.public.win32.programmer.gdi/2008-01/msg00044.html
        switch ( format() )
        {
            case PixelFormat16bppGrayScale:
            case PixelFormat48bppRGB      :
            case PixelFormat32bppRGB      :
            case PixelFormat24bppRGB      :
                return
                    PixelFormat24bppRGB;

            case PixelFormat64bppPARGB    :
            case PixelFormat32bppPARGB    :
            case PixelFormat64bppARGB     :
            case PixelFormat32bppARGB     :
            case PixelFormat16bppARGB1555 :
                return
                    PixelFormat32bppARGB;

            case PixelFormat16bppRGB565   :
            case PixelFormat16bppRGB555   :
            case PixelFormat8bppIndexed   :
            case PixelFormat4bppIndexed   :
            case PixelFormat1bppIndexed   :
                return
                    PixelFormat16bppRGB565;

            case PixelFormat32bppCMYK:
                return
                #if ( GDIPVER >= 0x0110 )
                    PixelFormat32bppCMYK;
                #else
                    PixelFormat24bppRGB;
                #endif // GDIPVER >= 0x0110

            default:
                BF_UNREACHABLE_CODE
                return PixelFormatUndefined;
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
            case PixelFormat24bppRGB      : return 0;
            case PixelFormat32bppARGB     : return 1;
            case PixelFormat16bppRGB565   : return 2;
            #if ( GDIPVER >= 0x0110 )
            case PixelFormat32bppCMYK     : return 3;
            #endif

            default:
                BF_UNREACHABLE_CODE
                return unsupported_format;
        }
    }

public: /// \ingroup Low level access
    typedef ::Gdiplus::GpBitmap lib_object_t;

    lib_object_t       & lib_object()       { return *pBitmap_; }
    lib_object_t const & lib_object() const { return const_cast<gp_image &>( *this ).lib_object(); }

private:
    friend class detail::gp_view_base;

    lib_object_t * /*const*/ pBitmap_;
}; // class gp_image

#if defined( BOOST_MSVC )
#   pragma warning( pop )
#endif

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // backend_hpp
