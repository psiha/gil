////////////////////////////////////////////////////////////////////////////////
///
/// \file gp_private_base.hpp
/// -------------------------
///
/// Base IO interface GDI+ implementation.
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
#ifndef gp_private_base_hpp__3B1ED5BC_42C6_4EC6_B700_01C1B8646431
#define gp_private_base_hpp__3B1ED5BC_42C6_4EC6_B700_01C1B8646431
//------------------------------------------------------------------------------
#include "../../gil_all.hpp"
#include "extern_lib_guard.hpp"
#include "formatted_image.hpp"
#include "gp_private_istreams.hpp"
#include "io_error.hpp"

#include <boost/array.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/static_assert.hpp>
#include "boost/type_traits/is_pod.hpp"

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
template <Gdiplus::PixelFormat gp_format> struct is_supported            : mpl::bool_ <(gp_format & PixelFormatGDI      ) != 0> {};
template <Gdiplus::PixelFormat gp_format> struct has_alpha               : mpl::bool_ <(gp_format & PixelFormatAlpha    ) != 0> {};
template <Gdiplus::PixelFormat gp_format> struct has_premultiplied_alpha : mpl::bool_ <(gp_format & PixelFormatPAlpha   ) != 0> {};
template <Gdiplus::PixelFormat gp_format> struct pixel_size              : mpl::size_t<( gp_format >> 8 ) & 0xff              > {};


template <typename Channel, typename ColorSpace>
struct gil_to_gp_format : mpl::integral_c<Gdiplus::PixelFormat, PixelFormatUndefined> {};

template <> struct gil_to_gp_format<bits8 ,rgb_t > : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat24bppRGB      > {};
template <> struct gil_to_gp_format<bits8 ,rgba_t> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat32bppARGB     > {};
template <> struct gil_to_gp_format<bits16,gray_t> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat16bppGrayScale> {};
template <> struct gil_to_gp_format<bits16,rgb_t > : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat48bppRGB      > {};
template <> struct gil_to_gp_format<bits16,rgba_t> : mpl::integral_c<Gdiplus::PixelFormat, PixelFormat64bppARGB     > {};

/// @see GdiplusPixelFormats.h: ARGB -> little endian BGRA 
typedef bgra_layout_t gp_alpha_layout_t;
typedef bgr_layout_t  gp_layout_t;

typedef packed_pixel_type<uint16_t, mpl::vector3_c<unsigned,5,6,5>, gp_layout_t>::type gp_rgb565_pixel_t;

struct unpacked_view_gp_format
{
    template <class View>
    struct apply
    {
        typedef gil_to_gp_format
                <
                    typename channel_type    <View>::type,
                    typename color_space_type<View>::type
                > type;
    };
};

struct packed_view_gp_format
{
    template <class View>
    struct apply
    {
        typedef typename mpl::if_
        <
            is_same<typename View::value_type, gp_rgb565_pixel_t>,
            mpl::integral_c<Gdiplus::PixelFormat, PixelFormat16bppRGB565>,
            mpl::integral_c<Gdiplus::PixelFormat, PixelFormatUndefined>
        >::type type;
    };
};


typedef mpl::vector5
<
    pixel_format_type<pixel<bits8 , gp_layout_t      >, false>,
    pixel_format_type<pixel<bits8 , gp_alpha_layout_t>, false>,
    pixel_format_type<pixel<bits16, gray_layout_t    >, false>,
    pixel_format_type<gp_rgb565_pixel_t               , false>,
    pixel_format_type<cmyk_t                          , false>
> gp_supported_pixel_formats;


template <class View>
struct view_gp_format
    :
    mpl::eval_if_c
    <
        // Err, an 'official' way to detect this?
        ( mpl::size<typename View::value_type::layout_t::channel_mapping_t>::value <= sizeof( typename View::value_type ) ),
        mpl::identity<unpacked_view_gp_format>,
        mpl::identity<  packed_view_gp_format>
    >::type::apply<View>::type
{};


typedef iterator_range<TCHAR const *> string_chunk_t;


string_chunk_t error_string( Gdiplus::GpStatus const status )
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
        #if (GDIPVER >= 0x0110)
        case ProfileNotFound           : return "Profile required to save an image in CMYK format not found";
        #endif //(GDIPVER >= 0x0110)
    }

    // Programmer errors:
    switch ( status )
    {
        case Ok                        : assert( !"Should not be called for no error" ); __assume( false );
        case InvalidParameter          : assert( !"Invalid parameter"                 ); __assume( false );
        case WrongState                : assert( !"Object in wrong state"             ); __assume( false );
        case GdiplusNotInitialized     : assert( !"GDI+ not initialized"              ); __assume( false );

        default: assert( !"Unknown GDI+ status code." ); __assume( false );
    }
}

inline void ensure_result( Gdiplus::GpStatus const result )
{
    if ( result != Gdiplus::Ok )
        io_error( error_string( result ).begin() );
}

inline void verify_result( Gdiplus::GpStatus const result )
{
    BOOST_VERIFY( result == Gdiplus::Ok );
}


inline gp_initialize_guard::gp_initialize_guard()
{
    using namespace Gdiplus;

    #if (GDIPVER >= 0x0110)
        GdiplusStartupInputEx const gp_startup_input( GdiplusStartupNoSetRound, 0, true, true );
    #else
        GdiplusStartupInput   const gp_startup_input(                           0, true, true );
    #endif //(GDIPVER >= 0x0110)
    GdiplusStartupOutput gp_startup_output;
    ensure_result
    (
        GdiplusStartup
        (
            &gp_token_,
            &gp_startup_input,
            &gp_startup_output
        )
    );
}

inline gp_initialize_guard::~gp_initialize_guard()
{
    Gdiplus::GdiplusShutdown( gp_token_ );
}


class gp_guard
    :
    gp_guard_base
#if BOOST_LIB_INIT( BOOST_GIL_EXTERNAL_LIB ) == BOOST_LIB_INIT_AUTO
    ,gp_initialize_guard
#endif
{
};


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
};


#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

class gp_image
    :
    private gp_guard,
    public  detail::formatted_image<gp_image, gp_supported_pixel_formats, gp_roi>
{
public:
    template <class View>
    struct is_supported : detail::is_supported<view_gp_format<View>::value> {};

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment = sizeof( Gdiplus::ARGB ) );
    
private:
    // - GP wants wide-char paths
    // - we received a narrow-char path
    // - we are using GP that means we are also using Windows
    // - on Windows a narrow-char path can only be up to MAX_PATH in length:
    //  http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#maxpath
    // - it is therefore safe to use a fixed sized/stack buffer...
    class wide_path
    {
    public:
        explicit wide_path( char const * const pFilename )
        {
            BOOST_ASSERT( pFilename );
            BOOST_ASSERT( std::strlen( pFilename ) < wideFileName_.size() );
            char    const * pSource     ( pFilename             );
            wchar_t       * pDestination( wideFileName_.begin() );
            do
            {
                *pDestination++ = *pSource;
            } while ( *pSource++ );
            BOOST_ASSERT( pDestination < wideFileName_.end() );
        }

        operator wchar_t const * () const { return wideFileName_.begin(); }

    private:
        boost::array<wchar_t, MAX_PATH> wideFileName_;
    };


public: /// \ingroup Construction
    explicit gp_image( wchar_t const * const filename )
    {
        ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromFileICM( filename, &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

    explicit gp_image( char const * const filename )
    {
        ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromFileICM( wide_path( filename ), &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

    // The passed IStream object must outlive the GpBitmap object (GDI+ uses
    // lazy evaluation).
    explicit gp_image( IStream & stream )
    {
        ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromStreamICM( &stream, &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

    // The passed View object must outlive the GpBitmap object (GDI+ uses lazy
    // evaluation).
    template <class View>
    explicit gp_image( View & view )
    {
        // http://msdn.microsoft.com/en-us/library/ms536315(VS.85).aspx
        // stride has to be a multiple of 4 bytes
        BOOST_ASSERT( !( view.pixels().row_size() % sizeof( Gdiplus::ARGB ) ) );

        ensure_result
        (
            Gdiplus::DllExports::GdipCreateBitmapFromScan0
            (
                view.width(),
                view.height(),
                view.pixels().row_size(),
                view_gp_format<View>::value,
                get_raw_data( view ),
                &pBitmap_
            )
        );
        BOOST_ASSERT( pBitmap_ );
    }

    ~gp_image()
    {
        verify_result( Gdiplus::DllExports::GdipDisposeImage( pBitmap_ ) );
    }

public:
    point2<std::ptrdiff_t> dimensions() const
    {
        using namespace Gdiplus;
        REAL width, height;
        verify_result( Gdiplus::DllExports::GdipGetImageDimension( const_cast<Gdiplus::GpBitmap *>( pBitmap_ ), &width, &height ) );
        return point2<std::ptrdiff_t>( static_cast<std::ptrdiff_t>( width ), static_cast<std::ptrdiff_t>( height ) );
    }


    void save_to_png( char    const * const pFilename ) const { save_to( pFilename, png_codec() ); }
    void save_to_png( wchar_t const * const pFilename ) const { save_to( pFilename, png_codec() ); }



private: // Private formatted_image_base interface.
    friend base_t;

    Gdiplus::PixelFormat get_format() const
    {
        using namespace Gdiplus;
        PixelFormat format;
        verify_result( DllExports::GdipGetImagePixelFormat( pBitmap_, &format ) );
        return format;
    }

    image_type_id current_image_type_id() const
    {
        switch ( get_format() )
        {
            case PixelFormat48bppRGB      :
            case PixelFormat24bppRGB      :
                return 0;

            case PixelFormat64bppARGB     :
            case PixelFormat32bppARGB     :
                return 1;

            case PixelFormat16bppGrayScale:
                return 2;

            case PixelFormat16bppRGB565   :
                return 3;

            case PixelFormat32bppCMYK     :
                return 4;

            default:
                BOOST_ASSERT( !"Should not get reached." ); __assume( false );

            case PixelFormat16bppRGB555   :

            case PixelFormat64bppPARGB    :
            case PixelFormat32bppPARGB    :

            case PixelFormat16bppARGB1555 :

            case PixelFormat32bppRGB      :

            case PixelFormat1bppIndexed   :
            case PixelFormat4bppIndexed   :
            case PixelFormat8bppIndexed   :
                return unsupported_format;
        }
    }

public:
    template <typename View>
    void convert_to_prepared_view( View const & view ) const
    {
        BOOST_STATIC_ASSERT( detail::is_supported<view_gp_format<View>::value>::value );

        BOOST_ASSERT( !dimensions_mismatch( view ) );

        using namespace Gdiplus;

        if ( detail::is_supported<view_gp_format<View>::value>::value )
        {
            PixelFormat const desired_format( view_gp_format<View>::value );
            pre_palettized_conversion<desired_format>( is_indexed<desired_format>::type() );
            copy_to_target( get_bitmap_data_for_view( view ) );
        }
        else
        {
            convert_to_prepared_view( view, default_color_converter() );
        }
    }

    template <typename View, typename CC>
    void convert_to_prepared_view( View const & view, CC const & converter ) const
    {
        BOOST_ASSERT( !dimensions_mismatch( view ) );

        using namespace Gdiplus;

        PixelFormat const my_format ( get_format()                     );
        BitmapData        bitmapData( get_bitmap_data_for_view( view ) );

        if ( my_format == view_gp_format<View>::value )
        {
            copy_to_target( bitmapData );
        }
        else
        {
            bool const can_do_in_place( can_do_inplace_transform<typename View::value_type>( my_format ) );
            if ( can_do_in_place )
            {
                bitmapData.PixelFormat = my_format;
                copy_to_target( bitmapData );
                transform( my_format, view, view, converter );
            }
            else
            {
                BOOST_ASSERT( bitmapData.Scan0 );
                typedef typename View::value_type pixel_t;
                pixel_t * const p_raw_view( static_cast<pixel_t *>( bitmapData.Scan0 ) );
                bitmapData.Scan0 = 0;

                Rect const rect( 0, 0, bitmapData.Width, bitmapData.Height );
                ensure_result
                (
                    DllExports::GdipBitmapLockBits
                    (
                        pBitmap_,
                        &rect,
                        ImageLockModeRead,
                        bitmapData.PixelFormat,
                        &bitmapData
                    )
                );
                transform
                (
                    bitmapData.PixelFormat,
                    interleaved_view( bitmapData.Width, bitmapData.Height, p_raw_view, bitmapData.Stride ),
                    view,
                    converter
                );
                verify_result( DllExports::GdipBitmapUnlockBits( pBitmap_, &bitmapData ) );
            }
        }
    }

    template <typename View>
    void copy_to_prepared_view( View const & view ) const
    {
        BOOST_ASSERT( !dimensions_mismatch( view ) );
        BOOST_ASSERT( !formats_mismatch<View>()    );
        convert_to_prepared_view( view );
    }

private:
    static CLSID const & png_codec()
    {
        static CLSID const clsid = { 0x557cf406, 0x1a04, 0x11d3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E };
        return clsid;
    }
    static CLSID const & jpg_codec()
    {
        static CLSID const clsid = { 0x557CF401, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E };
        return clsid;
    }
    static CLSID const & tiff_codec()
    {
        static CLSID const clsid = { 0x557CF405, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E };
        return clsid;
    }
    static CLSID const & gif_codec()
    {
        static CLSID const clsid = { 0x557CF402, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E };
        return clsid;
    }
    static CLSID const & bmp_codec()
    {
        static CLSID const clsid = { 0x557CF400, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E };
        return clsid;
    }

    template <typename View>
    static BYTE * get_raw_data( View const & view )
    {
        // A private implementation of interleaved_view_get_raw_data() that
        // works with packed pixel views.
        BOOST_STATIC_ASSERT((!is_planar<View>::value /*&& view_is_basic<View>::value*/));
        BOOST_STATIC_ASSERT((boost::is_pointer<typename View::x_iterator>::value));

        BOOST_STATIC_ASSERT( detail::is_supported<view_gp_format<View>::value>::value );

        return static_cast<BYTE *>( &gil::at_c<0>( view( 0, 0 ) ) );
    }

    template <typename View>
    Gdiplus::BitmapData get_bitmap_data_for_view( View const & view ) const
    {
        using namespace Gdiplus;

        BitmapData const bitmapData =
        {
            view.width(),
            view.height(),
            view.pixels().row_size(),
            view_gp_format<View>::value,
            get_raw_data( view ),
            0
        };
        return bitmapData;
    }

    void convert_to_target( Gdiplus::BitmapData const & bitmapData ) const
    {
        BOOST_ASSERT( bitmapData.Scan0 );

        using namespace Gdiplus;

        BitmapData * const pMutableBitmapData( const_cast<BitmapData *>( &bitmapData ) );
        GpStatus const load_result
        (
            DllExports::GdipBitmapLockBits
            (
                pBitmap_,
                0,
                ImageLockModeRead | ImageLockModeUserInputBuf,
                bitmapData.PixelFormat,
                pMutableBitmapData
            )
        );
        GpStatus const unlock_result( DllExports::GdipBitmapUnlockBits( pBitmap_, pMutableBitmapData ) );
        ensure_result(   load_result );
        verify_result( unlock_result );
    }


    void copy_to_target( Gdiplus::BitmapData const & bitmapData ) const
    {
        BOOST_ASSERT( bitmapData.Width       == static_cast<UINT>( dimensions().x ) );
        BOOST_ASSERT( bitmapData.Height      == static_cast<UINT>( dimensions().y ) );
        //...this need not hold as it can be used to perform GDI+ default
        //internal colour conversion...maybe i'll provide another worker
        //function...
        //BOOST_ASSERT( bitmapData.PixelFormat ==                    get_format    ()     );
        convert_to_target( bitmapData );
    }

    template <Gdiplus::PixelFormat desired_format>
    void pre_palettized_conversion( mpl::true_ /*is_indexed*/ )
    {
    #if (GDIPVER >= 0x0110)
        // A GDI+ 1.1 (a non-distributable version, distributed with MS Office
        // 2003 and MS Windows Vista and MS Windows 7) 'enhanced'/'tuned'
        // version of the conversion routine for indexed/palettized image
        // formats. Unless/until proven usefull, pretty much still a GDI+ 1.1
        // tester...
    
        BOOST_ASSERT( !has_alpha<desired_format>::value && "Is this possible for indexed formats?" );
        std::size_t const number_of_pixel_bits     ( pixel_size<desired_format>::value );
        std::size_t const number_of_palette_colours( static_cast<std::size_t>( ( 2 << ( number_of_pixel_bits - 1 ) ) - 1 ) );
        std::size_t const palette_size             ( sizeof( ColorPalette ) + number_of_palette_colours * sizeof( ARGB ) );
        aligned_storage
        <
           palette_size,
           alignment_of<ColorPalette>::value
        >::type palette_storage;
        ColorPalette & palette( *static_cast<ColorPalette *>( palette_storage.address() ) );
        palette.Flags = ( PaletteFlagsHasAlpha  * has_alpha<desired_format>::value ) |
                        ( PaletteFlagsGrayScale * is_same<typename color_space_type<View>::type, gray_t>::value );
        palette.Count = number_of_palette_colours;

        verify_result
        (
            DllExports::GdipInitializePalette
            (
                &palette,
                PaletteTypeOptimal,
                number_of_palette_colours,
                has_alpha<desired_format>::value,
                pBitmap_
            )
        );

        ensure_result
        (
            DllExports::GdipBitmapConvertFormat
            (
                pBitmap_,
                desired_format,
                DitherTypeErrorDiffusion,
                PaletteTypeOptimal,
                &palette,
                50
            )
        );
    #endif // (GDIPVER >= 0x0110)
    }

    template <Gdiplus::PixelFormat desired_format>
    void pre_palettized_conversion( mpl::false_ /*not is_indexed*/ ) const {}


    template <class SourceView, class DestinationView, class CC>
    static void transform( Gdiplus::PixelFormat const sourceFormat, SourceView const & src, DestinationView const & dst, CC const & converter )
    {
        // Reinterpret cast/type punning ugliness is required for dynamic
        // in-place detection/optimization...to be cleaned up...
        typedef typename DestinationView::value_type target_pixel_t;
        switch ( sourceFormat )
        {
            case PixelFormat48bppRGB      :
            case PixelFormat24bppRGB      :
                typedef pixel<bits8, gp_layout_t>                           gp_rgb24_pixel_t;
                typedef view_type_from_pixel<gp_rgb24_pixel_t, false>::type gp_rgb24_view_t ;
                transform_pixels
                (
                     *gil_reinterpret_cast_c<gp_rgb24_view_t const *>( &src ),
                     dst,
                     color_convert_deref_fn
                     <
                        gp_rgb24_pixel_t const &,
                        target_pixel_t,
                        CC
                     >( converter )
                );
                break;

            case PixelFormat64bppARGB     :
            case PixelFormat32bppARGB     :
                typedef pixel<bits8, gp_alpha_layout_t>                      gp_argb32_pixel_t;
                typedef view_type_from_pixel<gp_argb32_pixel_t, false>::type gp_argb32_view_t ;
                transform_pixels
                (
                     *gil_reinterpret_cast_c<gp_argb32_view_t const *>( &src ),
                     dst,
                     color_convert_deref_fn
                     <
                        gp_argb32_pixel_t const &,
                        target_pixel_t,
                        CC
                     >( converter )
                );
                break;

            case PixelFormat16bppGrayScale:
                typedef view_type_from_pixel<gray16_pixel_t, false>::type gp_gray16_view_t ;
                transform_pixels
                (
                     *gil_reinterpret_cast_c<gp_gray16_view_t const *>( &src ),
                     dst,
                     color_convert_deref_fn
                     <
                        gray16_ref_t,
                        target_pixel_t,
                        CC
                     >( converter )
                );
                break;

            case PixelFormat16bppRGB555   :
                BOOST_ASSERT( !"What about unused bits?" );
                typedef packed_pixel_type<uint16_t, mpl::vector3_c<unsigned,5,5,5>, gp_layout_t>::type gp555_pixel_t      ;
                typedef view_type_from_pixel<gp555_pixel_t, false>::type                               gp_rgb16_555_view_t;
                //...this...does not compile...how to specify a layout with an
                //unused bit?
                //transform_pixels
                //(
                //     *gil_reinterpret_cast_c<gp_rgb16_555_view_t const *>( &src ),
                //     dst,
                //     color_convert_deref_fn
                //     <
                //        gp555_pixel_t const &,
                //        target_pixel_t,
                //        CC
                //     >( converter )
                //);
                break;

            case PixelFormat16bppRGB565   :
                BOOST_ASSERT( !"Why does this fail to compile?" );
                typedef packed_pixel_type<uint16_t, mpl::vector3_c<unsigned,5,6,5>, gp_layout_t>::type gp565_pixel_t      ;
                typedef view_type_from_pixel<gp565_pixel_t, false>::type                               gp_rgb16_565_view_t;
                //transform_pixels
                //(
                //     *gil_reinterpret_cast_c<gp_rgb16_565_view_t const *>( &src ),
                //     dst,
                //     color_convert_deref_fn
                //     <
                //        //gp565_pixel_t const &,
                //        //gp_rgb16_565_view_t::reference,
                //        gp565_pixel_t::const_reference,
                //        target_pixel_t,
                //        CC
                //     >( converter )
                //);
                break;

            case PixelFormat32bppCMYK     :
                BOOST_ASSERT( !"What about byte order/endianess here?" );
                typedef view_type_from_pixel<cmyk_t, false>::type gp_cmyk_view_t;
                //...this...does not compile...
                //transform_pixels
                //(
                //     *gil_reinterpret_cast_c<gp_cmyk_view_t const *>( &src ),
                //     dst,
                //     color_convert_deref_fn
                //     <
                //        cmyk_t const &,
                //        target_pixel_t,
                //        CC
                //     >( converter )
                //);
                break;

            case PixelFormat64bppPARGB    :
            case PixelFormat32bppPARGB    :
                BOOST_ASSERT( !"What about premultiplied alpha?" );
                break;

            case PixelFormat16bppARGB1555 :
                BOOST_ASSERT( !"What about 'high colour with 1 bit alpha?" );
                break;

            case PixelFormat32bppRGB      :
                BOOST_ASSERT( !"What about 4 byte pixels without alpha?" );
                break;

            case PixelFormat1bppIndexed   :
            case PixelFormat4bppIndexed   :
            case PixelFormat8bppIndexed   :
                BOOST_ASSERT( !"What about indexed colour?" );
                break;

            default: BOOST_ASSERT( !"Should not get reached." ); __assume( false );
        }
    }


    void save_to( char    const * const pFilename, CLSID const & encoderID ) const { save_to( wide_path( pFilename ), encoderID ); }
    void save_to( wchar_t const * const pFilename, CLSID const & encoderID ) const
    {
        ensure_result( Gdiplus::DllExports::GdipSaveImageToFile( pBitmap_, pFilename, &encoderID, NULL ) );
    }


    template <typename Pixel>
    bool can_do_inplace_transform() const
    {
        return can_do_inplace_transform<Pixel>( get_format() );
    }

    template <typename Pixel>
    bool can_do_inplace_transform( Gdiplus::PixelFormat const my_format ) const
    {
        return ( Gdiplus::GetPixelFormatSize( my_format ) == sizeof( Pixel ) );
    }


    template <typename View>
    bool dimensions_mismatch( View const & view ) const
    {
        return dimensions_mismatch( view.dimensions() );
    }

    bool dimensions_mismatch( point2<std::ptrdiff_t> const & other_dimensions ) const
    {
        return other_dimensions != dimensions();
    }


    template <typename View>
    bool formats_mismatch() const
    {
        return formats_mismatch( view_gp_format<View>::value );
    }

    bool formats_mismatch( Gdiplus::PixelFormat const other_format ) const
    {
        return other_format != get_format();
    }


    template <typename View>
    void ensure_dimensions_match( View const & view ) const
    {
        ensure_dimensions_match( view.dimensions() );
    }

    void ensure_dimensions_match( point2<std::ptrdiff_t> const & other_dimensions ) const
    {
        io_error_if( dimensions_mismatch( other_dimensions ), "input view size does not match source image size" );
    }


    template <typename View>
    void ensure_formats_match() const
    {
        ensure_formats_match( view_gp_format<View>::value );
    }

    void ensure_formats_match( Gdiplus::PixelFormat const other_format ) const
    {
        io_error_if( formats_mismatch( other_format ), "input view type is incompatible with the image type" );
    }

private:
    friend class gp_view_base;

    Gdiplus::GpBitmap * /*const*/ pBitmap_;
};

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif



////////////////////////////////////////////////////////////////////////////////
///
/// \class gp_view_base
///
////////////////////////////////////////////////////////////////////////////////

class gp_view_base : noncopyable
{
public:
    ~gp_view_base()
    {
        verify_result( Gdiplus::DllExports::GdipBitmapUnlockBits( &bitmap_, &bitmapData_ ) );
    }

protected:
    gp_view_base( gp_image & bitmap, unsigned int const lock_mode, gp_image::roi const * const p_roi = 0 )
        :
        bitmap_( *bitmap.pBitmap_ )
    {
        std::memset( &bitmapData_, 0, sizeof( bitmapData_ ) );

        ensure_result
        (
            Gdiplus::DllExports::GdipBitmapLockBits
            (
                &bitmap_,
                p_roi,
                lock_mode,
                bitmap.get_format(),
                &bitmapData_
            )
        );
    }

    template <typename Pixel>
    typename type_from_x_iterator<Pixel *>::view_t
    get_typed_view()
    {
        //todo assert correct type...
        interleaved_view<Pixel *>( bitmapData_.Width, bitmapData_.Height, bitmapData_.Scan0, bitmapData_.Stride );
    }

private:
    Gdiplus::GpBitmap   & bitmap_    ;
    Gdiplus::BitmapData   bitmapData_;
};



////////////////////////////////////////////////////////////////////////////////
///
/// \class gp_view
///
////////////////////////////////////////////////////////////////////////////////

template <typename Pixel>
class gp_view
    :
    private gp_view_base,
    public  type_from_x_iterator<Pixel *>::view_t
{
public:
    gp_view( gp_image & image, gp_image::roi const * const p_roi = 0 )
        :
        gp_view_base( image, ImageLockModeRead | ( is_const<Pixel>::value * ImageLockModeWrite ), p_roi ),
        type_from_x_iterator<Pixel *>::view_t( get_typed_view<Pixel>() )
    {}
};


//...mhmh...to be seen if necessary...
//template <typename Pixel>
//class gp_const_view
//    :
//    private gp_view_base,
//    public  type_from_x_iterator<Pixel const *>::view_t
//{
//public:
//    gp_const_view( gp_image & image, gp_image::roi const * const p_roi = 0 )
//        :
//        gp_view_base( image, ImageLockModeRead ),
//        type_from_x_iterator<Pixel const *>::view_t( get_typed_view<Pixel const>() )
//    {}
//};


//...mhmh...to be implemented...
//template <class Impl, class SupportedPixelFormats, class ROI>
//inline
//typename formatted_image<Impl, SupportedPixelFormats, ROI>::view_t
//view( formatted_image<Impl, SupportedPixelFormats, ROI> & img );// { return img._view; }


#pragma warning( push )
#pragma warning( disable : 4355 ) // 'this' used in base member initializer list.

class __declspec( novtable ) gp_memory_image sealed
    :
    private MemoryReadStream,
    public  gp_image
{
public:
    gp_memory_image( memory_chunk_t const & in_memory_image )
        :
        MemoryReadStream( in_memory_image                 ),
        gp_image        ( static_cast<IStream &>( *this ) )
    {
    }
};

class __declspec( novtable ) gp_FILE_image sealed
    :
    private FileReadStream,
    public  gp_image
{
public:
    explicit gp_FILE_image( FILE * const pFile )
        :
        FileReadStream( *pFile                          ),
        gp_image      ( static_cast<IStream &>( *this ) )
    {
    }
};

#pragma warning( pop )


//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // gp_private_base_hpp
