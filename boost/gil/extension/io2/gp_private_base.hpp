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
#include "detail/io_error.hpp"
#include "detail/gp_extern_lib_guard.hpp"
#include "detail/windows_shared.hpp"
#include "detail/windows_shared_istreams.hpp"
#include "formatted_image.hpp"

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


typedef mpl::
#if (GDIPVER >= 0x0110)
    vector4
#else
    vector3
#endif
<
    image<bgr8_pixel_t     , false>,
    image<bgra8_pixel_t    , false>,
    image<gp_bgr565_pixel_t, false>
    #if (GDIPVER >= 0x0110)
    ,image<cmyk8_pixel_t   , false>
    #endif
> gp_supported_pixel_formats;


struct view_gp_format
{
    template <class View>
    struct apply : gil_to_gp_format<typename View::value_type, is_planar<View>::value> {};
};


typedef iterator_range<TCHAR const *> string_chunk_t;


/*string_chunk_t*/char const * error_string( Gdiplus::GpStatus const status )
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
        case Ok                        : BOOST_ASSERT( !"Should not be called for no error" ); __assume( false ); break;
        case InvalidParameter          : BOOST_ASSERT( !"Invalid parameter"                 ); __assume( false ); break;
        case WrongState                : BOOST_ASSERT( !"Object in wrong state"             ); __assume( false ); break;
        case GdiplusNotInitialized     : BOOST_ASSERT( !"GDI+ not initialized"              ); __assume( false ); break;

        default: BOOST_ASSERT( !"Unknown GDI+ status code." ); __assume( false ); break;
    }
}

inline void ensure_result( Gdiplus::GpStatus const result )
{
    if ( result != Gdiplus::Ok )
        io_error( error_string( result )/*.begin()*/ );
}

inline void verify_result( Gdiplus::GpStatus const result )
{
    BOOST_VERIFY( result == Gdiplus::Ok );
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

class gp_image;

template <>
struct formatted_image_traits<gp_image>
{
    typedef Gdiplus::PixelFormat format_t;

    typedef gp_supported_pixel_formats supported_pixel_formats_t;

    typedef gp_roi roi_t;

    typedef view_gp_format view_to_native_format;

    template <class View>
    struct is_supported : detail::is_supported<view_gp_format::apply<View>::value> {};

    struct view_data_t : public Gdiplus::BitmapData
    {
        template <typename View>
        view_data_t( View const & view ) : p_roi_( 0 ) { set_bitmapdata_for_view( view ); }

        template <typename View>
        view_data_t( View const & view, gp_roi::offset_t const & offset )
            :
            p_roi_( static_cast<gp_roi const *>( optional_roi_.address() ) )
        {
            set_bitmapdata_for_view( view );
            new ( optional_roi_.address() ) gp_roi( offset, Width, Height );
        }

        void set_format( format_t const format ) { PixelFormat = format; }

        Gdiplus::Rect const * const p_roi_;

    private:
        template <typename View>
        void set_bitmapdata_for_view( View const & view )
        {
            Width       = view.width();
            Height      = view.height();
            Stride      = view.pixels().row_size();
            PixelFormat = view_gp_format::apply<View>::value;
            Scan0       = formatted_image_base::get_raw_data( view );
            Reserved    = 0;
        }

        void operator=( view_data_t const & );

    private:
        aligned_storage<sizeof( gp_roi ), alignment_of<gp_roi>::value>::type optional_roi_;
    };

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( Gdiplus::ARGB ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true                    );
};


#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

class gp_image
    :
    private gp_guard,
    public  detail::formatted_image<gp_image>
{
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
                view_gp_format::apply<View>::value,
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
        verify_result( DllExports::GdipGetImageDimension( const_cast<GpBitmap *>( pBitmap_ ), &width, &height ) );
        return point2<std::ptrdiff_t>( static_cast<std::ptrdiff_t>( width ), static_cast<std::ptrdiff_t>( height ) );
    }

    static std::size_t format_size( format_t const format )
    {
        return Gdiplus::GetPixelFormatSize( format );
    }

    void save_to_png( char    const * const pFilename ) const { save_to( pFilename, png_codec() ); }
    void save_to_png( wchar_t const * const pFilename ) const { save_to( pFilename, png_codec() ); }

    ::Gdiplus::GpBitmap       & lib_object()       { return *pBitmap_; }
    ::Gdiplus::GpBitmap const & lib_object() const { return const_cast<gp_image &>( *this ).lib_object(); }

private: // Private formatted_image_base interface.
    friend base_t;

    format_t format() const
    {
        format_t pixel_format;
        verify_result( Gdiplus::DllExports::GdipGetImagePixelFormat( pBitmap_, &pixel_format ) );
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
                #if (GDIPVER >= 0x0110)
                    PixelFormat32bppCMYK;
                #else
                    PixelFormat24bppRGB;
                #endif // (GDIPVER >= 0x0110)

            default:
                BOOST_ASSERT( !"Should not get reached." ); __assume( false );
                return PixelFormatUndefined;
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
            case PixelFormat24bppRGB      : return 0;
            case PixelFormat32bppARGB     : return 1;
            case PixelFormat16bppRGB565   : return 2;
            #if (GDIPVER >= 0x0110)
            case PixelFormat32bppCMYK     : return 3;
            #endif

            default:
                BOOST_ASSERT( !"Should not get reached." ); __assume( false );
                return unsupported_format;
        }
    }

private:
    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
        BOOST_ASSERT( !dimensions_mismatch( view ) );
        //BOOST_ASSERT( !formats_mismatch   ( view ) );

        using namespace Gdiplus;
        point2<std::ptrdiff_t> const & targetDimensions( original_view( view ).dimensions() );
        gp_roi const roi( get_offset<gp_roi::offset_t>( view ), targetDimensions.x, targetDimensions.y );
        BitmapData bitmapData;
        ensure_result
        (
            DllExports::GdipBitmapLockBits
            (
                pBitmap_,
                &roi,
                ImageLockModeRead,
                view_gp_format::apply<MyView>::value,
                &bitmapData
            )
        );
        BOOST_ASSERT( bitmapData.PixelFormat == view_gp_format::apply<MyView>::value );
        copy_and_convert_pixels // This must not throw!
        (
            interleaved_view
            (
                bitmapData.Width ,
                bitmapData.Height,
                gil_reinterpret_cast_c<typename MyView::value_type const *>( bitmapData.Scan0 ),
                bitmapData.Stride
            ),
            view,
            converter
        );
        verify_result( DllExports::GdipBitmapUnlockBits( pBitmap_, &bitmapData ) );
    }

    void raw_convert_to_prepared_view( formatted_image_traits<gp_image>::view_data_t const & view_data ) const
    {
        BOOST_ASSERT( view_data.Scan0 );

        using namespace Gdiplus;

        BitmapData * const pMutableBitmapData( const_cast<BitmapData *>( static_cast<BitmapData const *>( &view_data ) ) );
        GpStatus const load_result
        (
            DllExports::GdipBitmapLockBits
            (
                pBitmap_,
                view_data.p_roi_,
                ImageLockModeRead | ImageLockModeUserInputBuf,
                view_data.PixelFormat,
                pMutableBitmapData
            )
        );
        GpStatus const unlock_result( DllExports::GdipBitmapUnlockBits( pBitmap_, pMutableBitmapData ) );
        ensure_result(   load_result );
        verify_result( unlock_result );
    }


    void copy_to_target( formatted_image_traits<gp_image>::view_data_t const & view_data ) const
    {
        BOOST_ASSERT( view_data.Width       == static_cast<UINT>( dimensions().x ) );
        BOOST_ASSERT( view_data.Height      == static_cast<UINT>( dimensions().y ) );
        //...this need not hold as it can be used to perform GDI+ default
        //internal colour conversion...maybe i'll provide another worker
        //function...
        //BOOST_ASSERT( view_data.PixelFormat ==                    format    ()     );
        raw_convert_to_prepared_view( view_data );
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


    template <Gdiplus::PixelFormat desired_format>
    void pre_palettized_conversion( mpl::true_ /*is_indexed*/ )
    {
    #if (GDIPVER >= 0x0110)
        // A GDI+ 1.1 (a non-distributable version, distributed with MS Office
        // 2003 and MS Windows Vista and MS Windows 7) 'enhanced'/'tuned'
        // version of the conversion routine for indexed/palettized image
        // formats. Unless/until proven useful, pretty much still a GDI+ 1.1
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

    void save_to( char    const * const pFilename, CLSID const & encoderID ) const { save_to( wide_path( pFilename ), encoderID ); }
    void save_to( wchar_t const * const pFilename, CLSID const & encoderID ) const
    {
        ensure_result( Gdiplus::DllExports::GdipSaveImageToFile( pBitmap_, pFilename, &encoderID, NULL ) );
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
        detail::verify_result( Gdiplus::DllExports::GdipBitmapUnlockBits( &bitmap_, &bitmapData_ ) );
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
                bitmap.format(),
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
