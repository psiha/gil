////////////////////////////////////////////////////////////////////////////////
///
/// \file backend.hpp
/// -----------------
///
/// LibTIFF backend.
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
#ifndef backend_hpp__0808D24E_CED1_4921_A832_3C12DAE93EF7
#define backend_hpp__0808D24E_CED1_4921_A832_3C12DAE93EF7
#pragma once
//------------------------------------------------------------------------------
#include "boost/gil/extension/io2/backends/detail/backend.hpp"

#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/libx_shared.hpp"
#include "boost/gil/extension/io2/detail/platform_specifics.hpp"
#include "boost/gil/extension/io2/detail/shared.hpp"

#if BOOST_MPL_LIMIT_VECTOR_SIZE < 35
    #error libtiff support requires mpl vectors of size 35 or greater...
#endif

#include <boost/array.hpp>
#include <boost/mpl/set/set10.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/smart_ptr/scoped_array.hpp>

extern "C"
{
    #include "tiff.h"
    #include "tiffio.h"
}
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

#define LIBTIFF_SPP(         param ) ( ( param ) << ( 0 ) )
#define LIBTIFF_BPS(         param ) ( ( param ) << ( 0 + 3 ) )
#define LIBTIFF_FMT(         param ) ( ( param ) << ( 0 + 3 + 5 ) )
#define LIBTIFF_PLANAR(      param ) ( ( param ) << ( 0 + 3 + 5 + 3 ) )
#define LIBTIFF_PHOTOMETRIC( param ) ( ( param ) << ( 0 + 3 + 5 + 3 + 2 ) )
#define LIBTIFF_INKSET(      param ) ( ( param ) << ( 0 + 3 + 5 + 3 + 2 + 3 ) )

#define LIBTIFF_FORMAT( spp, bps, sample_format, planar_config, photometric_interpretation, inkset ) \
(                                                                                                    \
        LIBTIFF_SPP(         spp                        ) |                                          \
        LIBTIFF_BPS(         bps                        ) |                                          \
        LIBTIFF_FMT(         sample_format              ) |                                          \
        LIBTIFF_PLANAR(      planar_config              ) |                                          \
        LIBTIFF_PHOTOMETRIC( photometric_interpretation ) |                                          \
        LIBTIFF_INKSET(      inkset                     )                                            \
)


union full_format_t
{
    struct format_bitfield
    {
        unsigned int samples_per_pixel    : 3;
        unsigned int bits_per_sample      : 5;
        unsigned int sample_format        : 3;
        unsigned int planar_configuration : 2;
        unsigned int photometric          : 3;
        unsigned int ink_set              : 2;
    };

    typedef unsigned int format_id;

    format_id       number;
    format_bitfield bits  ;
};


template <typename Pixel, bool isPlanar>
struct gil_to_libtiff_format
    :
    mpl::int_
    <
        LIBTIFF_FORMAT
        (
            num_channels<Pixel>::value,
            sizeof( typename channel_type<Pixel>::type ) * 8,
            (
                ( is_unsigned      <typename channel_type<Pixel>::type>::value * SAMPLEFORMAT_UINT   ) |
                ( is_signed        <typename channel_type<Pixel>::type>::value * SAMPLEFORMAT_INT    ) |
                ( is_floating_point<typename channel_type<Pixel>::type>::value * SAMPLEFORMAT_IEEEFP )
            ),
            isPlanar ? PLANARCONFIG_SEPARATE : PLANARCONFIG_CONTIG,
            (
                ( is_same<typename color_space_type<Pixel>::type, rgb_t >::value * PHOTOMETRIC_RGB        ) |
                ( is_same<typename color_space_type<Pixel>::type, cmyk_t>::value * PHOTOMETRIC_SEPARATED  ) |
                ( is_same<typename color_space_type<Pixel>::type, gray_t>::value * PHOTOMETRIC_MINISBLACK )
            ),
            ( is_same<typename color_space_type<Pixel>::type, cmyk_t>::value ? INKSET_CMYK : 0 )
        )
    >
{};


typedef mpl::vector35
<
    image<rgb8_pixel_t , false>,
    image<rgb8_pixel_t , true >,
    image<rgba8_pixel_t, false>,
    image<rgba8_pixel_t, true >,
    image<cmyk8_pixel_t, false>,
    image<cmyk8_pixel_t, true >,
    image<gray8_pixel_t, false>,

    image<rgb16_pixel_t , false>,
    image<rgb16_pixel_t , true >,
    image<rgba16_pixel_t, false>,
    image<rgba16_pixel_t, true >,
    image<cmyk16_pixel_t, false>,
    image<cmyk16_pixel_t, true >,
    image<gray16_pixel_t, false>,

    image<rgb8s_pixel_t , false>,
    image<rgb8s_pixel_t , true >,
    image<rgba8s_pixel_t, false>,
    image<rgba8s_pixel_t, true >,
    image<cmyk8s_pixel_t, false>,
    image<cmyk8s_pixel_t, true >,
    image<gray8s_pixel_t, false>,

    image<rgb16s_pixel_t , false>,
    image<rgb16s_pixel_t , true >,
    image<rgba16s_pixel_t, false>,
    image<rgba16s_pixel_t, true >,
    image<cmyk16s_pixel_t, false>,
    image<cmyk16s_pixel_t, true >,
    image<gray16s_pixel_t, false>,

    image<rgb32f_pixel_t , false>,
    image<rgb32f_pixel_t , true >,
    image<rgba32f_pixel_t, false>,
    image<rgba32f_pixel_t, true >,
    image<cmyk32f_pixel_t, false>,
    image<cmyk32f_pixel_t, true >,
    image<gray32f_pixel_t, false>
> libtiff_supported_pixel_formats;


template <typename Handle>
toff_t seek( thandle_t const handle, toff_t const off, int const whence )
{
    return static_cast<tsize_t>( device<Handle>::seek( static_cast<device_base::seek_origin>( whence ), off, reinterpret_cast<Handle>( handle ) ) );
}

template <typename Handle>
int close( thandle_t const handle )
{
    device<Handle>::close( reinterpret_cast<Handle>( handle ) );
    return 0;
}

inline int nop_close( thandle_t /*handle*/ )
{
    return 0;
}

template <typename Handle>
toff_t size( thandle_t const fd )
{
    return static_cast<toff_t>( device<Handle>::size( reinterpret_cast<Handle>( handle ) ) );
}


inline tsize_t memory_read_proc( thandle_t /*handle*/, tdata_t /*buf*/, tsize_t /*size*/ )
{
    BF_UNREACHABLE_CODE
    return 0;
}

inline tsize_t memory_write_proc( thandle_t /*handle*/, tdata_t /*buf*/, tsize_t /*size*/ )
{
    BF_UNREACHABLE_CODE
    return 0;
}

inline toff_t memory_seek_proc( thandle_t /*handle*/, toff_t /*off*/, int /*whence*/ )
{
    BF_UNREACHABLE_CODE
    return 0;
}

inline int memory_close_proc( thandle_t /*handle*/ )
{
    return 0;
}

inline toff_t memory_size_proc( thandle_t /*handle*/ )
{
    BF_UNREACHABLE_CODE
    return 0;
}

inline int memory_map_proc( thandle_t const handle, tdata_t * const pbase, toff_t * const psize )
{
    BOOST_ASSERT( handle );
    BOOST_ASSERT( pbase  );
    BOOST_ASSERT( psize  );
	//...zzz...
    //*pbase = static_cast<tdata_t>( const_cast<memory_range_t::value_type *>( gil_reinterpret_cast<memory_range_t *>( handle )->begin() ) );
    //*psize = gil_reinterpret_cast<memory_range_t *>( handle )->size ();
    return false;
}


#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

struct tiff_view_data_t
{
    template <class View>
    explicit tiff_view_data_t( View const & view, generic_vertical_roi::offset_t const offset = 0 )
        :
        dimensions_( view.dimensions()        ),
        stride_    ( view.pixels().row_size() ),
        offset_    ( offset                   )
        #ifdef _DEBUG
            ,format_id_( gil_to_libtiff_format<typename View::value_type, is_planar<View>::value>::value )
        #endif
    {
        set_buffers( view, is_planar<View>() );
    }

    void set_format( full_format_t::format_id const format )
    {
        BOOST_ASSERT_MSG( format_id_ == format, "LibTIFF does not provide builtin conversion." );
        ignore_unused_variable_warning( format );
    }

    point2<std::ptrdiff_t>         const & dimensions_      ;
    unsigned int                           stride_          ;
    unsigned int                           number_of_planes_;
    array<unsigned char *, 4>              plane_buffers_   ;
    generic_vertical_roi::offset_t         offset_          ;

    #ifdef _DEBUG
        unsigned int format_id_;
    #endif

private: // this should probably go to the base backend class...
    template <class View>
    void set_buffers( View const & view, mpl::true_ /*is planar*/ )
    {
        for ( number_of_planes_ = 0; number_of_planes_ < num_channels<View>::value; ++number_of_planes_ )
        {
            plane_buffers_[ number_of_planes_ ] = gil_reinterpret_cast<unsigned char *>( planar_view_get_raw_data( view, number_of_planes_ ) );
        }
        BOOST_ASSERT( number_of_planes_ == num_channels<View>::value );
    }

    template <class View>
    void set_buffers( View const & view, mpl::false_ /*is not planar*/ )
    {
        number_of_planes_ = 1;
        plane_buffers_[ 0 ] = backend_base::get_raw_data( view );
    }

    void operator=( tiff_view_data_t const & );
};

struct tiff_writer_view_data_t;

//------------------------------------------------------------------------------
} // namespace detail

class libtiff_image;

template <>
struct backend_traits<libtiff_image>
{
    typedef detail::full_format_t::format_id format_t;

    typedef detail::libtiff_supported_pixel_formats supported_pixel_formats_t;

    typedef detail::generic_vertical_roi roi_t;

    typedef detail::tiff_view_data_t view_data_t;

    struct gil_to_native_format
    {
        template <typename Pixel, bool IsPlanar>
        struct apply : detail::gil_to_libtiff_format<Pixel, IsPlanar> {};
    };

    template <typename Pixel, bool IsPlanar>
    struct is_supported : mpl::true_ {}; //...zzz...

    typedef mpl::set1
    <
        char const *
    > native_sources;

    typedef mpl::set1
    <
        char const *
    > native_sinks;

    typedef mpl::vector1_c<format_tag, tiff> supported_image_formats;

    typedef detail::tiff_writer_view_data_t writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = false            );
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class libtiff_image
///
////////////////////////////////////////////////////////////////////////////////

class libtiff_image
    :
    public detail::backend<libtiff_image>
{
public:
    struct guard {};
    
    class native_reader;
    class native_writer;

    typedef native_reader device_reader;
    typedef native_writer device_writer;

public:
    TIFF & lib_object() const { BOOST_ASSERT( p_tiff_ ); return *p_tiff_; }

protected:
    libtiff_image( char const * const file_name, char const * const access_mode )
        :
        p_tiff_( ::TIFFOpen( file_name, access_mode ) )
    {
        BOOST_ASSERT( file_name );
        construction_check();
    }

    template <typename DeviceHandle>
    explicit libtiff_image
    (
        DeviceHandle const handle,
        TIFFReadWriteProc const read_proc,
        TIFFReadWriteProc const write_proc,
        TIFFMapFileProc   const map_proc,
        TIFFUnmapFileProc const unmap_proc
    )
        :
        p_tiff_
        (
            ::TIFFClientOpen
            (
                "", "", //"M"
                    reinterpret_cast<thandle_t>( handle ),
                    read_proc,
                    write_proc,
                    &detail::seek<DeviceHandle>,
                    device<DeviceHandle>::auto_closes ? &detail::nop_close : &detail::close<<DeviceHandle>
                    &detail::size<<DeviceHandle>,
                    map_proc,
                    unmap_proc
            )
        )
    {
        construction_check();
    }

    BF_NOTHROW ~libtiff_image()
    {
        ::TIFFClose( &lib_object() );
    }

private:
    typedef detail::full_format_t full_format_t;

public:
    point2<std::ptrdiff_t> dimensions() const
    {
        return point2<std::ptrdiff_t>( get_field<uint32>( TIFFTAG_IMAGEWIDTH ), get_field<uint32>( TIFFTAG_IMAGELENGTH ) );
    }

protected:
    template <typename T>
    T get_field( ttag_t const tag ) const
    {
        // Implementation note:
        //   Vararg functions are a brickwall to most compilers so try and avoid
        // them when possible.
        //                                    (20.07.2011.) (Domagoj Saric)
        T value;
        #ifdef _MSC_VER
            T * p_value( &value );
            BOOST_VERIFY( ::TIFFVGetFieldDefaulted( &lib_object(), tag, reinterpret_cast<va_list>( &p_value ) ) );
        #else
            BOOST_VERIFY( ::TIFFGetFieldDefaulted ( &lib_object(), tag,                             &value    ) );
        #endif // _MSC_VER
        return value;
    }

    template <typename T1, typename T2>
    std::pair<T1, T2> get_field( ttag_t const tag, int & cumulative_result ) const
    {
        std::pair<T1, T2> value;
        cumulative_result &= ::TIFFGetFieldDefaulted( &lib_object(), tag, &value.first, &value.second );
        return value;
    }

    static std::size_t cached_format_size( backend_traits<libtiff_image>::format_t const format )
    {
        full_format_t::format_bitfield const & bits( reinterpret_cast<full_format_t const &>( format ).bits );
        return bits.bits_per_sample * ( ( bits.planar_configuration == PLANARCONFIG_CONTIG ) ? bits.samples_per_pixel : 1 ) / 8;
    }

protected:
    class cumulative_result : public detail::cumulative_result
    {
    public:
        void throw_if_error() const { detail::cumulative_result::throw_if_error( "Error reading TIFF file" ); }
    };

private:
    void construction_check() const
    {
        detail::io_error_if_not( p_tiff_, "Failed to create a LibTIFF object." );
    }

private:
    TIFF * const p_tiff_;
};

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // backend_hpp
