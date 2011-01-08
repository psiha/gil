////////////////////////////////////////////////////////////////////////////////
///
/// \file gp_image.hpp
/// ------------------
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


template <typename Pixel, bool IsPlanar>
struct gp_is_supported : is_supported<gil_to_gp_format<Pixel, IsPlanar>::value>{};


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
        io_error( error_string( result )/*.begin()*/ );
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
};


class gp_writer : public open_on_write_writer
{
public:
    typedef std::pair<Gdiplus::GpBitmap *, Gdiplus::EncoderParameters *> lib_object_t;

    // The passed View object must outlive the GpBitmap object (GDI+ uses lazy
    // evaluation).
    template <class View>
    explicit gp_writer( View & view )
    {
        BOOST_STATIC_ASSERT(( gp_is_supported<typename View::value_type, is_planar<View>::value>::value ));

        // http://msdn.microsoft.com/en-us/library/ms536315(VS.85).aspx
        // stride has to be a multiple of 4 bytes
        BOOST_ASSERT( !( view.pixels().row_size() % sizeof( Gdiplus::ARGB ) ) );

        ensure_result
        (
            Gdiplus::DllExports::GdipCreateBitmapFromScan0
            (
                view.width (),
                view.height(),
                view.pixels().row_size(),
                gil_to_gp_format<typename View::value_type, is_planar<View>::value>::value,
                formatted_image_base::get_raw_data( view ),
                &lib_object().first
            )
        );
        BOOST_ASSERT( lib_object().first );
    }

    ~gp_writer()
    {
        detail::verify_result( Gdiplus::DllExports::GdipDisposeImage( lib_object().first ) );
    }

    template <typename Target>
    void write_default( Target const & target, format_tag const format )
    {
        BOOST_ASSERT( lib_object().second == NULL );
        write( target, format );
    }

    void write( char    const * const pFilename, format_tag const format ) const
    {
        write( detail::wide_path( pFilename ), format );
    }

    void write( wchar_t const * const pFilename, format_tag const format ) const
    {
        detail::ensure_result
        (
            Gdiplus::DllExports::GdipSaveImageToFile
            (
                lib_object().first,
                pFilename,
                &encoder_id( format ),
                lib_object().second
            )
        );
    }

    lib_object_t       & lib_object()       { return lib_instance_; }
    lib_object_t const & lib_object() const { return lib_instance_; }

private:
    static CLSID const & encoder_id( format_tag const format )
    {
        static CLSID const ids[ number_of_known_formats ] =
        {
            { 0x557CF400, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // BMP
            { 0x557CF402, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // GIF
            { 0x557CF401, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // JPEG
            { 0x557CF406, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // PNG
            { 0x557CF405, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // TIFF
            CLSID_NULL // TGA
        };
        BOOST_ASSERT( ids[ format ] != CLSID_NULL );
        return ids[ format ];
    }

private:
    lib_object_t lib_instance_;
};


class gp_view_base;

//------------------------------------------------------------------------------
} // namespace detail

class gp_image;

template <>
struct formatted_image_traits<gp_image>
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
        mpl::pair<memory_chunk_t        , detail::memory_chunk_for_IStream_extender<gp_image> >
    > readers;

    typedef mpl::map4
    <
        mpl::pair<char    const *, detail::gp_writer>,
        mpl::pair<wchar_t const *, detail::gp_writer>,
        mpl::pair<IStream        , detail::gp_writer>,
        mpl::pair<FILE           , detail::gp_writer>
    > writers;

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
            Scan0       = formatted_image_base::get_raw_data( view );
            Reserved    = 0;
        }

        void operator=( view_data_t const & );

    private:
        aligned_storage<sizeof( roi_t ), alignment_of<roi_t>::value>::type optional_roi_;
    };

    typedef view_data_t writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( Gdiplus::ARGB ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true                    );
};


#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

class gp_image
    :
    private detail::gp_guard,
    public  detail::formatted_image<gp_image>
{
public:
    typedef detail::gp_user_guard guard;

public: /// \ingroup Construction
    explicit gp_image( wchar_t const * const filename )
    {
        detail::ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromFile( filename, &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

    explicit gp_image( char const * const filename )
    {
        detail::ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromFile( detail::wide_path( filename ), &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

    // The passed IStream object must outlive the GpBitmap object (GDI+ uses
    // lazy evaluation).
    explicit gp_image( IStream & stream )
    {
        detail::ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromStream( &stream, &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

    ~gp_image()
    {
        detail::verify_result( Gdiplus::DllExports::GdipDisposeImage( pBitmap_ ) );
    }

public:
    point2<std::ptrdiff_t> dimensions() const
    {
        using namespace Gdiplus;
        REAL width, height;
        detail::verify_result( DllExports::GdipGetImageDimension( const_cast<GpBitmap *>( pBitmap_ ), &width, &height ) );
        return point2<std::ptrdiff_t>( static_cast<std::ptrdiff_t>( width ), static_cast<std::ptrdiff_t>( height ) );
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
                #if (GDIPVER >= 0x0110)
                    PixelFormat32bppCMYK;
                #else
                    PixelFormat24bppRGB;
                #endif // (GDIPVER >= 0x0110)

            default:
                BF_UNREACHABLE_CODE
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
                BF_UNREACHABLE_CODE
                return unsupported_format;
        }
    }

public: // Low-level (row, strip, tile) access
    static bool can_do_roi_access() { return true; }

    class sequential_row_access_state
        :
        private detail::cumulative_result
    {
    public:
        using detail::cumulative_result::failed;
        void throw_if_error() const { detail::cumulative_result::throw_if_error( "GDI+ failure" ); }

        BOOST_STATIC_CONSTANT( bool, throws_on_error = false );

    private:
        sequential_row_access_state( gp_image const & source_image )
            :
            roi_( 0, 0, source_image.dimensions().x, 1 )
        {
            bitmapData_.Width  = roi_.Width;
            bitmapData_.Height = 1;
            bitmapData_.PixelFormat = source_image.format();
            bitmapData_.Stride = bitmapData_.Width * source_image.cached_format_size( bitmapData_.PixelFormat );
            bitmapData_.Reserved = 0;
        }

    private: friend gp_image;
        Gdiplus::Rect       roi_       ;
        Gdiplus::BitmapData bitmapData_;
    };

    sequential_row_access_state begin_sequential_row_access() const { return sequential_row_access_state( *this ); }

    /// \todo Kill duplication with raw_convert_to_prepared_view().
    ///                                       (04.01.2011.) (Domagoj Saric)
    void read_row( sequential_row_access_state & state, unsigned char * const p_row_storage ) const
    {
        using namespace detail ;
        using namespace Gdiplus;

        state.bitmapData_.Scan0 = p_row_storage;

        state.accumulate_equal
        (
            DllExports::GdipBitmapLockBits
            (
                pBitmap_,
                &state.roi_,
                ImageLockModeRead | ImageLockModeUserInputBuf,
                state.bitmapData_.PixelFormat,
                &state.bitmapData_
            ),
            Gdiplus::Ok
        );
        verify_result( DllExports::GdipBitmapUnlockBits( pBitmap_, &state.bitmapData_ ) );

        ++state.roi_.Y;
    }

    ::Gdiplus::GpBitmap       & lib_object()       { return *pBitmap_; }
    ::Gdiplus::GpBitmap const & lib_object() const { return const_cast<gp_image &>( *this ).lib_object(); }

private: // Private formatted_image_base interface.
    friend base_t;

    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
        BOOST_ASSERT( !dimensions_mismatch( view ) );
        //BOOST_ASSERT( !formats_mismatch   ( view ) );

        using namespace detail ;
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


    void raw_convert_to_prepared_view( view_data_t const & view_data ) const
    {
        BOOST_ASSERT( view_data.Scan0 );

        using namespace detail ;
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


    void raw_copy_to_prepared_view( view_data_t const & view_data ) const
    {
        BOOST_ASSERT( view_data.Width       == static_cast<UINT>( dimensions().x ) );
        BOOST_ASSERT( view_data.Height      == static_cast<UINT>( dimensions().y ) );
        //...this need not hold as it can be used to perform GDI+ default
        //internal colour conversion...maybe i'll provide another worker
        //function...
        //BOOST_ASSERT( view_data.PixelFormat ==                    format    ()     );
        raw_convert_to_prepared_view( view_data );
    }


    static std::size_t cached_format_size( format_t const format )
    {
        return Gdiplus::GetPixelFormatSize( format );
    }

private:
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

private:
    friend class detail::gp_view_base;

    Gdiplus::GpBitmap * /*const*/ pBitmap_;
};

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif

namespace detail
{
//------------------------------------------------------------------------------

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

        detail::ensure_result
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

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // gp_private_base_hpp
