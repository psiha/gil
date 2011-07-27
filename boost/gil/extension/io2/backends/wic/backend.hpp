////////////////////////////////////////////////////////////////////////////////
///
/// \file backend.hpp
/// -----------------
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
#ifndef backend_hpp__78D710F7_11C8_4023_985A_22B180C9A476
#define backend_hpp__78D710F7_11C8_4023_985A_22B180C9A476
#pragma once
//------------------------------------------------------------------------------
#include "extern_lib_guard.hpp"

#include "boost/gil/extension/io2/backends/detail/backend.hpp"

#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/shared.hpp"
#include "boost/gil/extension/io2/detail/windows_shared.hpp"
#include "boost/gil/extension/io2/detail/windows_shared_istreams.hpp"

#include <boost/mpl/vector_c.hpp> //...missing from metafuncitons.hpp...
#include "boost/gil/metafunctions.hpp"
#include "boost/gil/rgb.hpp"
#include "boost/gil/typedefs.hpp"

#include <boost/array.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/set/set10.hpp>
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

template <typename Pixel, bool IsPlanar, typename Alloc=std::allocator<unsigned char> >
class image;

namespace io
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

    static wic_format_t const value;
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
        p_roi_ ( 0                                                                           ),
        format_( gil_to_wic_format<typename View::value_type, is_planar<View>::value>::value )
    {
        set_bitmapdata_for_view( view );
    }

    template <typename View>
    wic_view_data_t( View const & view, wic_roi::offset_t const & offset )
        :
        p_roi_ ( static_cast<wic_roi const *>( optional_roi_.address() )                     ),
        format_( gil_to_wic_format<typename View::value_type, is_planar<View>::value>::value )
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
        //format_     = gil_to_wic_format<typename View::value_type, is_planar<View>::value>::value;
        p_buffer_   = detail::backend_base::get_raw_data( view );
    }

    void operator=( wic_view_data_t const & );

private:
    aligned_storage<sizeof( wic_roi ), alignment_of<wic_roi>::value>::type optional_roi_;
};


template <template <typename Handle> class IODeviceStream, class BackendWriter>
class device_stream_wrapper;

//------------------------------------------------------------------------------
} // namespace detail

class wic_image;

template <>
struct backend_traits<wic_image>
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

    typedef mpl::set4
    <
        char    const *,
        wchar_t const *,
        HANDLE         ,
        IStream       &
    > native_sources;

    typedef mpl::set1
    <
        IStream       &
    > native_sinks;

    typedef mpl::vector5_c<format_tag, png, bmp, gif, jpeg, tiff> supported_image_formats;

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
    public  detail::backend<wic_image>
{
public:
    typedef detail::wic_user_guard guard;

    class native_reader;
    typedef detail::device_stream_wrapper<detail::input_device_stream , native_reader> device_reader;

    class native_writer;
    typedef detail::device_stream_wrapper<detail::output_device_stream, native_writer> device_writer;

public: // Low-level (row, strip, tile) access
    static bool can_do_roi_access() { return true; }
};

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // backend_hpp
