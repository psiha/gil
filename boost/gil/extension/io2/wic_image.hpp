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
#include "../../gil_all.hpp"
#include "detail/io_error.hpp"
#include "detail/wic_extern_lib_guard.hpp"
#include "detail/windows_shared.hpp"
#include "detail/windows_shared_istreams.hpp"

#include <boost/array.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/static_assert.hpp>
#include "boost/type_traits/is_pod.hpp"

#include "wincodec.h"

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

template <wic_format_t wic_format>
struct is_supported_format
    : is_same
      <
        format_guid<wic_format                  >,
        format_guid<GUID_WICPixelFormatUndefined>
      >
{};


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
    template <class View>
    struct apply : gil_to_wic_format<typename View::value_type, is_planar<View>::value> {};
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
        X = x; Y = y; Width = width; Height = height;
    }

    wic_roi( offset_t const offset, value_type const width, value_type const height )
    {
        X = offset.x; Y = offset.y; Width = width; Height = height;
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
        p_roi_ ( 0                                   ),
        format_( view_wic_format::apply<View>::value )
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
        CComPtr<IWICBitmapFrameEncode>   p_frame_           ;
        CComPtr<IWICBitmapEncoder    >   p_encoder_         ;
        IPropertyBag2                  * p_frame_parameters_;
    };

    lib_object_t & lib_object() { return lib_object_; }

public:
    wic_writer( IStream & target )
    {
        create_encoder( GUID_ContainerFormatPng, target );
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
    void create_encoder( GUID const & format, IStream & target )
    {
        ensure_result( wic_factory<>::singleton().CreateEncoder( format, NULL, &lib_object().p_encoder_ ) );
        ensure_result( lib_object().p_encoder_->Initialize( &target, WICBitmapEncoderNoCache )            );
        ensure_result( lib_object().p_encoder_->CreateNewFrame( &lib_object().p_frame_, &lib_object().p_frame_parameters_ ) );
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

    typedef detail::view_wic_format view_to_native_format;

    template <class View>
    struct is_supported
        :
        mpl::not_
        <
            is_same
            <
                typename view_to_native_format::apply<View>::type,
                //format_guid<wic_format                  >,
                detail::format_guid<GUID_WICPixelFormatUndefined>
            >
        > {};

    typedef mpl::map6
    <
        mpl::pair<char                    const *,                                                    wic_image  >,
        mpl::pair<wchar_t                 const *,                                                    wic_image  >,
        mpl::pair<HANDLE                         ,                                                    wic_image  >,
        mpl::pair<IStream                       &,                                                    wic_image  >,
        mpl::pair<FILE                          &, detail::input_FILE_for_IStream_extender           <wic_image> >,
        mpl::pair<writable_memory_chunk_t const &, detail::writable_memory_chunk_for_IStream_extender<wic_image> >
    > readers;

    typedef mpl::map4
    <
        mpl::pair<char           const *, detail::output_c_str_for_c_file_extender<detail::output_FILE_for_IStream_extender <detail::wic_writer> > >,
        mpl::pair<IStream              &,                                           detail::wic_writer  >,
        mpl::pair<FILE                 &, detail::output_FILE_for_IStream_extender <detail::wic_writer> >,
        mpl::pair<memory_chunk_t const &, detail::memory_chunk_for_IStream_extender<detail::wic_writer> >
    > writers;

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
                CComPtr<IWICBitmapFrameDecode>,
                CComPtr<IWICBitmapDecoder    >
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
        ensure_result( wic_factory<>::singleton().CreateDecoderFromStream( &stream, NULL, WICDecodeMetadataCacheOnDemand, &lib_object().second ) );
        create_first_frame_decoder();
    }

    explicit wic_image( HANDLE const file_handle )
    {
        using namespace detail;
        ensure_result( wic_factory<>::singleton().CreateDecoderFromFileHandle( reinterpret_cast<ULONG_PTR>( file_handle ), NULL, WICDecodeMetadataCacheOnDemand, &lib_object().second ) );
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
    //        wic_factory<>::singleton().CreateBitmapFromMemory
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

    lib_object_t & lib_object() { return lib_object_; }

public:
    point2<std::ptrdiff_t> dimensions() const
    {
        using namespace detail;
        typedef point2<std::ptrdiff_t> result_t;
        aligned_storage<sizeof( result_t ), alignment_of<result_t>::value>::type placeholder;
        result_t & result( *gil_reinterpret_cast<result_t *>( placeholder.address() ) );
        verify_result( frame_decoder().GetSize( gil_reinterpret_cast<UINT *>( &result.x ), gil_reinterpret_cast<UINT *>( &result.y ) ) );
        return result;
    }

    static std::size_t format_size( format_t /*const*/ format )
    {
        using namespace detail;
        CComQIPtr<IWICComponentInfo> p_component_info;
        ensure_result( wic_factory<>::singleton().CreateComponentInfo( format, &p_component_info ) );
        //CComQIPtr<IWICPixelFormatInfo> p_pixel_format_info;
        //p_component_info->QueryInterface( );(IID_IWICPixelFormatInfo
        CComQIPtr<IWICPixelFormatInfo> const p_pixel_format_info( p_component_info );
        io_error_if_not( p_pixel_format_info, "WIC failure" );
        unsigned int bits_per_pixel;
        verify_result( p_pixel_format_info->GetBitsPerPixel( &bits_per_pixel ) );
        return bits_per_pixel;
    }

private: // Private formatted_image_base interface.
    friend base_t;

    //...zzz...cleanup...
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


    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
        BOOST_ASSERT( !dimensions_mismatch( view ) );

        using namespace detail;

        //...non template related code yet to be extracted...
        point2<std::ptrdiff_t> const & targetDimensions( original_view( view ).dimensions() );
        wic_roi const roi( get_offset<wic_roi::offset_t>( view ), targetDimensions.x, targetDimensions.y );
        CComQIPtr<IWICBitmap> p_bitmap;
        ensure_result( wic_factory<>::singleton().CreateBitmapFromSourceRect( &frame_decoder(), roi.X, roi.Y, roi.Width, roi.Height, &p_bitmap ) );
        CComQIPtr<IWICBitmapLock> p_bitmap_lock;
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
        CComQIPtr<IWICFormatConverter> p_converter;
        ensure_result( wic_factory<>::singleton().CreateFormatConverter( &p_converter ) );
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

private:
    IWICBitmapFrameDecode & frame_decoder() const { return *lib_object_.first ; }
    IWICBitmapDecoder     & wic_decoder  () const { return *lib_object_.second; }

    void create_decoder_from_filename( wchar_t const * const filename )
    {
        using namespace detail;
        ensure_result( wic_factory<>::singleton().CreateDecoderFromFilename( filename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &lib_object().second ) );
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
