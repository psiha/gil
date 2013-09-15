////////////////////////////////////////////////////////////////////////////////
///
/// \file gp/reader.hpp
/// -------------------
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
#ifndef reader_hpp__3B1ED5BC_42C6_4EC6_B700_01C1B8646431
#define reader_hpp__3B1ED5BC_42C6_4EC6_B700_01C1B8646431
#pragma once
//------------------------------------------------------------------------------
#include "backend.hpp"
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

//------------------------------------------------------------------------------
} // namespace detail

#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

class gp_image::native_reader
    :
    public gp_image,
    public detail::backend_reader<gp_image>
{
public:
    typedef detail::gp_user_guard guard;

public: /// \ingroup Construction
    explicit native_reader( wchar_t const * const filename )
    {
        detail::ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromFile( filename, &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

    explicit native_reader( char const * const filename )
    {
        detail::ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromFile( detail::wide_path( filename ), &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

    // The passed IStream object must outlive the GpBitmap object (GDI+ uses
    // lazy evaluation).
    explicit native_reader( IStream & stream )
    {
        detail::ensure_result( Gdiplus::DllExports::GdipCreateBitmapFromStream( &stream, &pBitmap_ ) );
        BOOST_ASSERT( pBitmap_ );
    }

public: // Low-level (row, strip, tile) access
    static bool can_do_roi_access() { return true; }

    class sequential_row_read_state
        :
        private detail::cumulative_result
    {
    public:
        using detail::cumulative_result::failed;
        void throw_if_error() const { detail::cumulative_result::throw_if_error( "GDI+ failure" ); }

        BOOST_STATIC_CONSTANT( bool, throws_on_error = false );

    private:
        sequential_row_read_state( gp_image const & source_image )
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
    }; // class sequential_row_read_state

    sequential_row_read_state begin_sequential_row_read() const { return sequential_row_read_state( *this ); }

    /// \todo Kill duplication with raw_convert_to_prepared_view().
    ///                                       (04.01.2011.) (Domagoj Saric)
    void read_row( sequential_row_read_state & state, unsigned char * const p_row_storage ) const
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

private: // Private backend_base interface.
    friend class base_t;

    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
        BOOST_ASSERT( !dimensions_mismatch( view ) );
        //BOOST_ASSERT( !formats_mismatch   ( view ) );

        using namespace detail ;
        using namespace Gdiplus;

        point2<std::ptrdiff_t> const & targetDimensions( original_view( view ).dimensions() );
        gp_roi const roi( get_offset<gp_roi::offset_t>( view ), targetDimensions.x, targetDimensions.y );
        format_t const my_format( gil_to_gp_format<typename View::value_type, is_planar<View>::value>::value );
        BitmapData bitmapData;
        ensure_result
        (
            DllExports::GdipBitmapLockBits
            (
                pBitmap_,
                &roi,
                ImageLockModeRead,
                my_format,
                &bitmapData
            )
        );
        BOOST_ASSERT( bitmapData.PixelFormat == my_format );
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


    static unsigned int cached_format_size( format_t const format )
    {
        return Gdiplus::GetPixelFormatSize( format );
    }

private:
    template <Gdiplus::PixelFormat desired_format>
    void pre_palettized_conversion( mpl::true_ /*is_indexed*/ )
    {
    #if ( GDIPVER >= 0x0110 )
        // A GDI+ 1.1 (a non-distributable version, distributed with MS Office
        // 2003 and MS Windows Vista and MS Windows 7) 'enhanced'/'tuned'
        // version of the conversion routine for indexed/palettized image
        // formats. Unless/until proven useful, pretty much still a GDI+ 1.1
        // tester...

        BOOST_ASSERT( !has_alpha<desired_format>::value && "Is this possible for indexed formats?" );
        unsigned int const number_of_pixel_bits     ( pixel_size<desired_format>::value );
        unsigned int const number_of_palette_colours( static_cast<unsigned int>( ( 2 << ( number_of_pixel_bits - 1 ) ) - 1 ) );
        unsigned int const palette_size             ( sizeof( ColorPalette ) + number_of_palette_colours * sizeof( ARGB ) );
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
    #endif // GDIPVER >= 0x0110
    }

    template <Gdiplus::PixelFormat desired_format>
    void pre_palettized_conversion( mpl::false_ /*not is_indexed*/ ) const {}

private:
    friend class detail::gp_view_base;

    Gdiplus::GpBitmap * /*const*/ pBitmap_;
}; // class gp_image::native_reader

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
}; // class gp_view_base



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
//typename backend<Impl, SupportedPixelFormats, ROI>::view_t
//view( backend<Impl, SupportedPixelFormats, ROI> & img );// { return img._view; }

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // reader_hpp
