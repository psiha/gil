////////////////////////////////////////////////////////////////////////////////
///
/// \file wic_image.hpp
/// -------------------
///
/// Base IO interface WIC implementation.
///
/// Copyright (c) Domagoj Saric 2010.-2011.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef wic_reader_hpp__8C7EFA72_9E1A_4FD4_AF3F_B7921B320C1C
#define wic_reader_hpp__8C7EFA72_9E1A_4FD4_AF3F_B7921B320C1C
#pragma once
//------------------------------------------------------------------------------
#include "backend_reader.hpp"
#include "detail/io_error.hpp"
#include "detail/wic_extern_lib_guard.hpp"
#include "detail/windows_shared.hpp"
#include "detail/windows_shared_istreams.hpp"
#include "wic_image.hpp"

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
namespace io
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class wic_image::native_reader
///
////////////////////////////////////////////////////////////////////////////////

class wic_image::native_reader
    :
    public wic_image,
    public detail::backend_reader<wic_image>
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
    explicit native_reader( wchar_t const * const filename )
    {
        create_decoder_from_filename( filename );
    }

    explicit native_reader( char const * const filename )
    {
        create_decoder_from_filename( detail::wide_path( filename ) );
    }

    // The passed IStream object must outlive the wic_image object (to support
    // lazy evaluation).
    explicit native_reader( IStream & stream )
    {
        using namespace detail;
        ensure_result( wic_factory::singleton().CreateDecoderFromStream( &stream, NULL, WICDecodeMetadataCacheOnDemand, &lib_object().second ) );
        create_first_frame_decoder();
    }

    explicit native_reader( HANDLE const file_handle )
    {
        using namespace detail;
        ensure_result( wic_factory::singleton().CreateDecoderFromFileHandle( reinterpret_cast<ULONG_PTR>( file_handle ), NULL, WICDecodeMetadataCacheOnDemand, &lib_object().second ) );
        create_first_frame_decoder();
    }

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
    class sequential_row_read_state
        :
        private detail::cumulative_result
    {
    public:
        using detail::cumulative_result::failed;
        void throw_if_error() const { detail::cumulative_result::throw_if_error( "WIC failure" ); }

        BOOST_STATIC_CONSTANT( bool, throws_on_error = false );

    private:
        sequential_row_read_state( wic_image::native_reader const & source_image )
            :
            roi_   ( 0, 0, source_image.dimensions().x, 1 ),
            stride_( roi_.X * source_image.pixel_size()   )
        {}

    private: friend wic_image;
        detail::wic_roi       roi_   ;
        UINT            const stride_;
    };

    sequential_row_read_state begin_sequential_row_read() const { return sequential_row_read_state( *this ); }

    void read_row( sequential_row_read_state & state, unsigned char * const p_row_storage ) const
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
    friend class detail::backend_reader<wic_image>;

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
            BOOST_ASSERT(( locked_format == gil_to_wic_format<typename View::value_type, is_planar<View>::value>::value ));
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
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // wic_reader_hpp
