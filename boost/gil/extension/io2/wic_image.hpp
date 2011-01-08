////////////////////////////////////////////////////////////////////////////////
///
/// \file wic_image.hpp
/// -------------------
///
/// Base IO interface WIC implementation.
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
#ifndef wic_image_hpp__78D710F7_11C8_4023_985A_22B180C9A476
#define wic_image_hpp__78D710F7_11C8_4023_985A_22B180C9A476
//------------------------------------------------------------------------------
#include "detail/io_error.hpp"
#include "detail/libx_shared.hpp" //...zzz...
#include "detail/wic_extern_lib_guard.hpp"
#include "detail/windows_shared.hpp"
#include "detail/windows_shared_istreams.hpp"
#include "formatted_image.hpp"

#include <boost/mpl/vector_c.hpp> //...missing from metafuncitons.hpp...
#include "boost/gil/metafunctions.hpp"
#include "boost/gil/rgb.hpp"
#include "boost/gil/typedefs.hpp"

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

typedef WICPixelFormatGUID const & wic_format_t;

template <wic_format_t guid>
struct format_guid
{
    typedef format_guid type;

    static wic_format_t & value;
};

template <wic_format_t guid>
wic_format_t format_guid<guid>::value( guid );

template <typename Pixel, bool IsPlanar>
struct gil_to_wic_format : format_guid<GUID_WICPixelFormatUndefined> {};

typedef packed_pixel_type<uint16_t, mpl::vector3_c<unsigned,5,6,5>, bgr_layout_t>::type wic_bgr565_pixel_t;

template <> struct gil_to_wic_format<wic_bgr565_pixel_t, false> : format_guid<GUID_WICPixelFormat16bppBGR555> {};
template <> struct gil_to_wic_format<bgr8_pixel_t      , false> : format_guid<GUID_WICPixelFormat24bppBGR   > {};
template <> struct gil_to_wic_format<rgb8_pixel_t      , false> : format_guid<GUID_WICPixelFormat24bppRGB   > {};
template <> struct gil_to_wic_format<bgra8_pixel_t     , false> : format_guid<GUID_WICPixelFormat32bppBGRA  > {};
template <> struct gil_to_wic_format<gray16_pixel_t    , false> : format_guid<GUID_WICPixelFormat16bppGray  > {};
template <> struct gil_to_wic_format<bgr16_pixel_t     , false> : format_guid<GUID_WICPixelFormat48bppRGB   > {};
template <> struct gil_to_wic_format<bgra16_pixel_t    , false> : format_guid<GUID_WICPixelFormat64bppBGRA  > {};
template <> struct gil_to_wic_format<cmyk8_pixel_t     , false> : format_guid<GUID_WICPixelFormat32bppCMYK  > {};


typedef mpl::vector8
<
    image<wic_bgr565_pixel_t, false>,
    image<bgr8_pixel_t      , false>,
    image<rgb8_pixel_t      , false>,
    image<bgra8_pixel_t     , false>,
    image<gray16_pixel_t    , false>,
    image<bgr16_pixel_t     , false>,
    image<bgra16_pixel_t    , false>,
    image<cmyk8_pixel_t     , false>
> wic_supported_pixel_formats;


struct view_wic_format
{
    template <typename Pixel, bool IsPlanar>
    struct apply : gil_to_wic_format<Pixel, IsPlanar> {};
};


class wic_roi : public WICRect
{
public:
    typedef INT                value_type;
    typedef point2<value_type> point_t   ;

    typedef point_t            offset_t  ;

public:
    wic_roi( value_type const x, value_type const y, value_type const width, value_type const height )
    {
        X = x; Y = y;
        Width = width; Height = height;
    }

    wic_roi( offset_t const top_left, value_type const width, value_type const height )
    {
        X = top_left.x; Y = top_left.y;
        Width = width; Height = height;
    }

    wic_roi( offset_t const top_left, offset_t const bottom_right )
    {
        X = top_left.x; Y = top_left.y;
        Width  = bottom_right.x - top_left.x;
        Height = bottom_right.y - top_left.y;
    }
};


inline void ensure_result( HRESULT const result )
{
    io_error_if( result != S_OK, "WIC failure"/*error_string( result )*/ );
}

inline void verify_result( HRESULT const result )
{
    BOOST_VERIFY( result == S_OK );
}


struct wic_view_data_t
{
    template <typename View>
    wic_view_data_t( View const & view )
        :
        p_roi_ ( 0                                                                                ),
        format_( view_wic_format::apply<typename View::value_type, is_planar<View>::value>::value )
    {
        set_bitmapdata_for_view( view );
    }

    template <typename View>
    wic_view_data_t( View const & view, wic_roi::offset_t const & offset )
        :
        p_roi_ ( static_cast<wic_roi const *>( optional_roi_.address() ) ),
        format_( view_wic_format::apply<View>::value                     )
    {
        set_bitmapdata_for_view( view );
        new ( optional_roi_.address() ) wic_roi( offset, width_, height_ );
    }

    WICRect      const * const p_roi_ ;
    unsigned int         /*const*/ width_ ;
    unsigned int         /*const*/ height_;
    unsigned int         /*const*/ stride_;
    unsigned int         /*const*/ pixel_size_;
    BYTE               * /*const*/ p_buffer_;
    wic_format_t         const format_;

private:
    template <typename View>
    void set_bitmapdata_for_view( View const & view )
    {
        width_      = view.width();
        height_     = view.height();
        stride_     = view.pixels().row_size();
        pixel_size_ = memunit_step( typename View::x_iterator() );
        //format_     = view_wic_format::apply<View>::value;
        p_buffer_   = detail::formatted_image_base::get_raw_data( view );
    }

    void operator=( wic_view_data_t const & );

private:
    aligned_storage<sizeof( wic_roi ), alignment_of<wic_roi>::value>::type optional_roi_;
};


class wic_writer : public configure_on_write_writer
{
public:
    struct lib_object_t
    {
        detail::com_scoped_ptr<IWICBitmapFrameEncode>   p_frame_           ;
        detail::com_scoped_ptr<IWICBitmapEncoder    >   p_encoder_         ;
        IPropertyBag2                                 * p_frame_parameters_;
    };

    lib_object_t & lib_object() { return lib_object_; }

public:
    wic_writer( IStream & target, format_tag const format )
    {
        create_encoder( target, encoder_guid( format ) );
    }

    void write_default( wic_view_data_t const & view_data )
    {
        HRESULT hr( frame().Initialize( lib_object().p_frame_parameters_ ) );

        if ( SUCCEEDED( hr ) )
        {
            hr = frame().SetSize( view_data.width_, view_data.height_ );
        }

        if ( SUCCEEDED( hr ) )
        {
            WICPixelFormatGUID formatGUID( view_data.format_ );
            hr = frame().SetPixelFormat( &formatGUID );
            if ( SUCCEEDED( hr ) )
            {
                hr = ( formatGUID == view_data.format_ ) ? S_OK : E_FAIL;
            }
        }

        if ( SUCCEEDED( hr ) )
        {
            hr = frame().WritePixels( view_data.height_, view_data.stride_, view_data.height_ * view_data.stride_, view_data.p_buffer_ );
        }

        if ( SUCCEEDED( hr ) )
        {
            hr = frame().Commit();
        }

        if ( SUCCEEDED( hr ) )
        {
            hr = encoder().Commit();
        }

        ensure_result( hr );
    }

    void write( wic_view_data_t const & view_data ) { write_default( view_data ); }

private:
    void create_encoder( IStream & target, GUID const & format )
    {
        ensure_result( wic_factory::singleton().CreateEncoder( format, NULL, &lib_object().p_encoder_ ) );
        ensure_result( lib_object().p_encoder_->Initialize( &target, WICBitmapEncoderNoCache )          );
        ensure_result( lib_object().p_encoder_->CreateNewFrame( &lib_object().p_frame_, &lib_object().p_frame_parameters_ ) );
    }

    static GUID const & encoder_guid( format_tag const format )
    {
        static GUID const * const guids[ number_of_known_formats ] =
        {
            &GUID_ContainerFormatBmp,
            &GUID_ContainerFormatGif,
            &GUID_ContainerFormatJpeg,
            &GUID_ContainerFormatPng,
            &GUID_ContainerFormatTiff,
            NULL // TGA
        };
        BOOST_ASSERT( guids[ format ] != NULL );
        return *guids[ format ];
    }

    IWICBitmapFrameEncode & frame  () { return *lib_object().p_frame_  ; }
    IWICBitmapEncoder     & encoder() { return *lib_object().p_encoder_; }
    
private:
    lib_object_t lib_object_;
};

//------------------------------------------------------------------------------
} // namespace detail

class wic_image;

template <>
struct formatted_image_traits<wic_image>
{
    typedef detail::wic_format_t format_t;

    typedef detail::wic_supported_pixel_formats supported_pixel_formats_t;

    typedef detail::wic_roi roi_t;

    typedef detail::view_wic_format gil_to_native_format;

    template <typename Pixel, bool IsPlanar>
    struct is_supported
        :
        mpl::not_
        <
            is_same
            <
                typename gil_to_native_format:: BOOST_NESTED_TEMPLATE apply<Pixel, IsPlanar>::type,
                //format_guid<wic_format                  >,
                detail::format_guid<GUID_WICPixelFormatUndefined>
            >
        > {};

    typedef mpl::map6
    <
        mpl::pair<char           const *,                                           wic_image  >,
        mpl::pair<wchar_t        const *,                                           wic_image  >,
        mpl::pair<HANDLE                ,                                           wic_image  >,
        mpl::pair<IStream               ,                                           wic_image  >,
        mpl::pair<FILE                  , detail::input_FILE_for_IStream_extender  <wic_image> >,
        mpl::pair<memory_chunk_t        , detail::memory_chunk_for_IStream_extender<wic_image> >
    > readers;

    typedef mpl::map3
    <
        mpl::pair<wchar_t const *, detail::output_file_name_for_IStream_extender<detail::wic_writer> >,
        mpl::pair<IStream        ,                                               detail::wic_writer  >,
        mpl::pair<FILE           , detail::output_FILE_for_IStream_extender     <detail::wic_writer> >
    > writers;

    typedef mpl::vector5_c<format_tag, bmp, gif, jpeg, png, tiff> supported_image_formats;

    typedef detail::wic_view_data_t view_data_t       ;
    typedef view_data_t             writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = true             );
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class wic_image
///
////////////////////////////////////////////////////////////////////////////////
// http://msdn.microsoft.com/en-us/magazine/cc500647.aspx
////////////////////////////////////////////////////////////////////////////////

class wic_image
    :
    private detail::wic_base_guard,
    public  detail::formatted_image<wic_image>
{
public:
    // Implementation note:
    //   The IWICBitmapDecoder instance is not otherwise necessary once an
    // IWICBitmapFrameDecode instance is created but we keep it here and make it
    // accessible to the user to enable the use of multi frame/page/picture
    // formats like GIF and TIFF.
    //                                        (26.07.2010.) (Domagoj Saric)
    typedef std::pair
            <
                detail::com_scoped_ptr<IWICBitmapFrameDecode>,
                detail::com_scoped_ptr<IWICBitmapDecoder    >
            > lib_object_t;

    typedef detail::wic_user_guard guard;

public:
    explicit wic_image( wchar_t const * const filename )
    {
        create_decoder_from_filename( filename );
    }

    explicit wic_image( char const * const filename )
    {
        create_decoder_from_filename( detail::wide_path( filename ) );
    }

    // The passed IStream object must outlive the wic_image object (to support
    // lazy evaluation).
    explicit wic_image( IStream & stream )
    {
        using namespace detail;
        ensure_result( wic_factory::singleton().CreateDecoderFromStream( &stream, NULL, WICDecodeMetadataCacheOnDemand, &lib_object().second ) );
        create_first_frame_decoder();
    }

    explicit wic_image( HANDLE const file_handle )
    {
        using namespace detail;
        ensure_result( wic_factory::singleton().CreateDecoderFromFileHandle( reinterpret_cast<ULONG_PTR>( file_handle ), NULL, WICDecodeMetadataCacheOnDemand, &lib_object().second ) );
        create_first_frame_decoder();
    }

    // The passed View object must outlive the wic_image object (to support
    // lazy evaluation).
    //...zzz...the same approach as with GDI+ cannot be used with WIC so an IStream approach will be required...
    //template <class View>
    //explicit wic_image( View & view )
    //{
    //    ensure_result
    //    (
    //        wic_factory::singleton().CreateBitmapFromMemory
    //        (
    //            view.width(),
    //            view.height(),
    //            view.pixels().row_size(),
    //            view_wic_format::apply<View>::value,
    //            get_raw_data( view ),
    //            &pBitmap_
    //        )
    //    );
    //    create_first_frame_decoder();
    //}

public:
    point2<std::ptrdiff_t> dimensions() const
    {
        using namespace detail;
        typedef point2<std::ptrdiff_t> result_t;
        aligned_storage<sizeof( result_t ), alignment_of<result_t>::value>::type placeholder;
        result_t & result( *gil_reinterpret_cast<result_t *>( placeholder.address() ) );
        BOOST_STATIC_ASSERT( sizeof( result_t::value_type ) == sizeof( UINT ) );
        verify_result( frame_decoder().GetSize( gil_reinterpret_cast<UINT *>( &result.x ), gil_reinterpret_cast<UINT *>( &result.y ) ) );
        return result;
    }

    /*format_t*/WICPixelFormatGUID format() const
    {
        WICPixelFormatGUID pixel_format;
        detail::verify_result( frame_decoder().GetPixelFormat( &pixel_format ) );
        //...zzz...check that it is a supported format...
        return pixel_format;
    }

    /*format_t*/WICPixelFormatGUID closest_gil_supported_format() const { return format(); }

    image_type_id current_image_format_id() const
    {
        return image_format_id( closest_gil_supported_format() );
    }

public: // Low-level (row, strip, tile) access
    static bool can_do_roi_access() { return true; }

    class sequential_row_access_state
        :
        private detail::cumulative_result
    {
    public:
        using detail::cumulative_result::failed;
        void throw_if_error() const { detail::cumulative_result::throw_if_error( "WIC failure" ); }

        BOOST_STATIC_CONSTANT( bool, throws_on_error = false );

    private:
        sequential_row_access_state( wic_image const & source_image )
            :
            roi_   ( 0, 0, source_image.dimensions().x, 1 ),
            stride_( roi_.X * source_image.pixel_size()   )
        {}

    private: friend wic_image;
        detail::wic_roi       roi_   ;
        UINT            const stride_;
    };

    sequential_row_access_state begin_sequential_row_access() const { return sequential_row_access_state( *this ); }

    void read_row( sequential_row_access_state & state, unsigned char * const p_row_storage ) const
    {
        state.accumulate_equal
        (
            frame_decoder().CopyPixels
            (
                &state.roi_,
                state.stride_,
                state.stride_,
                p_row_storage
            ),
            S_OK
        );
        ++state.roi_.Y;
    }

    lib_object_t & lib_object() { return lib_object_; }

private: // Private formatted_image_base interface.
    friend base_t;

    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
        BOOST_ASSERT( !dimensions_mismatch( view ) );

        using namespace detail;

        //...non template related code yet to be extracted...
        point2<std::ptrdiff_t> const & targetDimensions( original_view( view ).dimensions() );
        wic_roi const roi( get_offset<wic_roi::offset_t>( view ), targetDimensions.x, targetDimensions.y );
        com_scoped_ptr<IWICBitmap> p_bitmap;
        ensure_result( wic_factory::singleton().CreateBitmapFromSourceRect( &frame_decoder(), roi.X, roi.Y, roi.Width, roi.Height, &p_bitmap ) );
        com_scoped_ptr<IWICBitmapLock> p_bitmap_lock;
        ensure_result( p_bitmap->Lock( &roi, WICBitmapLockRead, &p_bitmap_lock ) );
        unsigned int buffer_size;
        BYTE * p_buffer;
        verify_result( p_bitmap_lock->GetDataPointer( &buffer_size, &p_buffer ) );
        unsigned int stride;
        verify_result( p_bitmap_lock->GetStride( &stride ) );
        #ifndef NDEBUG
            WICPixelFormatGUID locked_format;
            verify_result( p_bitmap_lock->GetPixelFormat( &locked_format ) );
            BOOST_ASSERT( locked_format == view_wic_format::apply<MyView>::value );
        #endif
        copy_and_convert_pixels
        (
            interleaved_view
            (
                roi.Width ,
                roi.Height,
                gil_reinterpret_cast_c<typename MyView::value_type const *>( p_buffer ),
                stride
            ),
            view,
            converter
        );
    }


    void raw_convert_to_prepared_view( view_data_t const & view_data ) const
    {
        BOOST_ASSERT( view_data.format_ != GUID_WICPixelFormatUndefined ); //...zzz...
        using namespace detail;
        com_scoped_ptr<IWICFormatConverter> p_converter;
        ensure_result( wic_factory::singleton().CreateFormatConverter( &p_converter ) );
        ensure_result( p_converter->Initialize( &frame_decoder(), view_data.format_, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom ) );
        ensure_result
        (
            p_converter->CopyPixels
            (
                view_data.p_roi_,
                view_data.stride_,
                view_data.height_ * view_data.stride_ * view_data.pixel_size_,
                view_data.p_buffer_
            )
        );
    }


    void raw_copy_to_prepared_view( view_data_t const & view_data ) const
    {
        detail::ensure_result
        (
            frame_decoder().CopyPixels
            (
                view_data.p_roi_,
                view_data.stride_,
                view_data.height_ * view_data.stride_ * view_data.pixel_size_,
                view_data.p_buffer_
            )
        );
    }


    static std::size_t cached_format_size( format_t const format )
    {
        using namespace detail;
        com_scoped_ptr<IWICComponentInfo> p_component_info;
        ensure_result( wic_factory::singleton().CreateComponentInfo( format, &p_component_info ) );
        //com_scoped_ptr<IWICPixelFormatInfo> p_pixel_format_info;
        //p_component_info->QueryInterface( );IID_IWICPixelFormatInfo
        com_scoped_ptr<IWICPixelFormatInfo> const p_pixel_format_info( *p_component_info );
        io_error_if_not( p_pixel_format_info, "WIC failure" );
        unsigned int bits_per_pixel;
        verify_result( p_pixel_format_info->GetBitsPerPixel( &bits_per_pixel ) );
        return bits_per_pixel;
    }

private:
    IWICBitmapFrameDecode & frame_decoder() const { return *lib_object_.first ; }
    IWICBitmapDecoder     & wic_decoder  () const { return *lib_object_.second; }

    void create_decoder_from_filename( wchar_t const * const filename )
    {
        using namespace detail;
        ensure_result( wic_factory::singleton().CreateDecoderFromFilename( filename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &lib_object().second ) );
        create_first_frame_decoder();
    }

    void create_first_frame_decoder()
    {
        using namespace detail;
        #ifndef NDEBUG
            unsigned int frame_count;
            verify_result( wic_decoder().GetFrameCount( &frame_count ) );
            BOOST_ASSERT( frame_count >= 1 );
        #endif // NDEBUG
        ensure_result( wic_decoder().GetFrame( 0, &lib_object().first ) );
    }

private:
    lib_object_t lib_object_;
};


//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // wic_image_hpp
