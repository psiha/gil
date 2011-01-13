////////////////////////////////////////////////////////////////////////////////
///
/// \file libtiff_image.hpp
/// -----------------------
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
#ifndef libtiff_image_hpp__0808D24E_CED1_4921_A832_3C12DAE93Ef7
#define libtiff_image_hpp__0808D24E_CED1_4921_A832_3C12DAE93Ef7
//------------------------------------------------------------------------------
#include "formatted_image.hpp"

#include "detail/platform_specifics.hpp"
#include "detail/io_error.hpp"
#include "detail/libx_shared.hpp"

#if BOOST_MPL_LIMIT_VECTOR_SIZE < 35
    #error libtiff support requires mpl vectors of size 35 or greater...
#endif

#include <boost/array.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/smart_ptr/scoped_array.hpp>

extern "C"
{
    #include "tiff.h"
    #include "tiffio.h"
}

#include <cstdio>
#include <set>
#ifdef _MSC_VER
    #include "io.h"
#else
    #include "sys/types.h"
    #include "sys/stat.h"
    #include "unistd.h"
#endif // _MSC_VER
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

    format_bitfield bits  ;
    format_id       number;
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


inline tsize_t FILE_read_proc( thandle_t const handle, tdata_t const buf, tsize_t const size )
{
    return static_cast<tsize_t>( std::fread( buf, 1, size,  reinterpret_cast<FILE *>( handle ) ) );
}

inline tsize_t FILE_write_proc( thandle_t const handle, tdata_t const buf, tsize_t const size )
{
    return static_cast<tsize_t>( std::fwrite( buf, 1, size,  reinterpret_cast<FILE *>( handle ) ) );
}

inline toff_t FILE_seek_proc( thandle_t const handle, toff_t const off, int const whence )
{
    return static_cast<tsize_t>( std::fseek( reinterpret_cast<FILE *>( handle ), off, whence ) );
}

inline int FILE_close_proc( thandle_t const handle )
{
    return std::fclose( reinterpret_cast<FILE *>( handle ) );
}

inline int FILE_close_proc_nop( thandle_t /*handle*/ )
{
    return 0;
}

inline toff_t FILE_size_proc( thandle_t const fd )
{
    #ifdef _MSC_VER
        return /*std*/::_filelength( /*std*/::_fileno( gil_reinterpret_cast<FILE *>( fd ) ) );
    #else
        struct stat file_status;
        BOOST_VERIFY( ::fstat( ::fileno( gil_reinterpret_cast<FILE *>( fd ) ), &file_status ) == 0 );
        return file_status.st_size;
    #endif // _MSC_VER
}

inline int FILE_map_proc( thandle_t /*handle*/, tdata_t * /*pbase*/, toff_t * /*psize*/ )
{
    return false;
}

inline void FILE_unmap_proc( thandle_t /*handle*/, tdata_t /*base*/, toff_t /*size*/ )
{
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
    *pbase = static_cast<tdata_t>( const_cast<memory_chunk_t::value_type *>( gil_reinterpret_cast<memory_chunk_t *>( handle )->begin() ) );
    *psize = gil_reinterpret_cast<memory_chunk_t *>( handle )->size ();
    return true;
}

inline void memory_unmap_proc( thandle_t /*handle*/, tdata_t /*base*/, toff_t /*size*/ )
{
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
        #ifdef _DEBUG
            BOOST_ASSERT( ( format_id_ == format ) && !"libtiff does not provide builtin conversion." );
        #endif // _DEBUG
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

private: // this should probably go to the base formatted_image class...
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
        plane_buffers_[ 0 ] = formatted_image_base::get_raw_data( view );
    }

    void operator=( tiff_view_data_t const & );
};


struct tiff_writer_view_data_t : public tiff_view_data_t
{
    template <class View>
    tiff_writer_view_data_t( View const & view )
        :
        tiff_view_data_t( view, 0 )
    {
        format_.number = gil_to_libtiff_format<typename View::value_type, is_planar<View>::value>::value;
    }

    full_format_t format_;
};


class libtiff_base
{
public:
    TIFF & lib_object() const { BOOST_ASSERT( p_tiff_ ); return *p_tiff_; }

protected:
    libtiff_base( char const * const file_name, char const * const access_mode )
        :
        p_tiff_( ::TIFFOpen( file_name, access_mode ) )
    {
        BOOST_ASSERT( file_name );
        construction_check();
    }

    explicit libtiff_base( FILE & file )
        :
        p_tiff_
        (
            ::TIFFClientOpen
            (
                "", "",
                    &file,
                    &detail::FILE_read_proc,
                    &detail::FILE_write_proc,
                    &detail::FILE_seek_proc,
                    &detail::FILE_close_proc_nop,
                    &detail::FILE_size_proc,
                    &detail::FILE_map_proc,
                    &detail::FILE_unmap_proc
            )
        )
    {
        construction_check();
    }

    explicit libtiff_base( memory_chunk_t & in_memory_image )
        :
        p_tiff_
        (
            ::TIFFClientOpen
            (
                "", "M",
                    &in_memory_image,
                    &detail::memory_read_proc,
                    &detail::memory_write_proc,
                    &detail::memory_seek_proc,
                    &detail::memory_close_proc,
                    &detail::memory_size_proc,
                    &detail::memory_map_proc,
                    &detail::memory_unmap_proc
            )
        )
    {
        construction_check();
    }

    BF_NOTHROW ~libtiff_base()
    {
        ::TIFFClose( &lib_object() );
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
        io_error_if_not( p_tiff_, "Failed to open TIFF input file" );
    }

private:
    TIFF * const p_tiff_;
};


class libtiff_writer
    :
    public libtiff_base,
    public configure_on_write_writer
{
public:
    explicit libtiff_writer( char const * const file_name ) : detail::libtiff_base( file_name, "w" ) {}

    explicit libtiff_writer( FILE & file ) : detail::libtiff_base( file ) {}

    void write_default( tiff_writer_view_data_t const & view )
    {
        full_format_t::format_bitfield const format_bits( view.format_.bits );
        //BOOST_ASSERT( ( format_bits.planar_configuration == PLANARCONFIG_CONTIG ) && "Add planar support..." );

        set_field( TIFFTAG_IMAGEWIDTH     , view.dimensions_.x               );
        set_field( TIFFTAG_IMAGELENGTH    , view.dimensions_.y               );
        set_field( TIFFTAG_BITSPERSAMPLE  , format_bits.bits_per_sample      );
        set_field( TIFFTAG_SAMPLESPERPIXEL, format_bits.samples_per_pixel    );
        set_field( TIFFTAG_PLANARCONFIG   , format_bits.planar_configuration );
        set_field( TIFFTAG_PHOTOMETRIC    , format_bits.photometric          );
        set_field( TIFFTAG_INKSET         , format_bits.ink_set              );
        set_field( TIFFTAG_SAMPLEFORMAT   , format_bits.sample_format        );

        set_field( TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT );

        if ( format_bits.samples_per_pixel == 4 && format_bits.photometric == PHOTOMETRIC_RGB )
        {
            uint16 const type( EXTRASAMPLE_UNASSALPHA );
            set_field( TIFFTAG_EXTRASAMPLES, 1, &type );
        }

        write( view );
    }

    void write( tiff_writer_view_data_t const & view )
    {
        cumulative_result result;

        for ( unsigned int plane( 0 ); plane < view.number_of_planes_; ++plane )
        {
            unsigned char * buf( view.plane_buffers_[ plane ] );

            unsigned int       row       ( 0                  );
            unsigned int const target_row( view.dimensions_.y );
            while ( row < target_row )
            {
                result.accumulate_greater( ::TIFFWriteScanline( &lib_object(), buf, row++, static_cast<tsample_t>( plane ) ), 0 );
                buf += view.stride_;
            }
        }

        result.throw_if_error();
    }

private:
    template <typename T>
    void set_field( ttag_t const tag, T value )
    {
        #ifdef _MSC_VER
            BOOST_VERIFY( ::TIFFVSetField( &lib_object(), tag, gil_reinterpret_cast<va_list>( &value ) ) );
        #else
            BOOST_VERIFY( ::TIFFSetField ( &lib_object(), tag,                                 value   ) );
        #endif // _MSC_VER
    }

    template <typename T1, typename T2>
    void set_field( ttag_t const tag, T1 const value1, T2 const value2 )
    {
        BOOST_VERIFY( ::TIFFSetField( &lib_object(), tag, value1, value2 ) );
    }
};

//------------------------------------------------------------------------------
} // namespace detail

class libtiff_image;

template <>
struct formatted_image_traits<libtiff_image>
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

    typedef mpl::map2
    <
        mpl::pair<char const *, libtiff_image>,
        mpl::pair<FILE        , libtiff_image>
    > readers;

    typedef mpl::map2
    <
        mpl::pair<char const *, detail::libtiff_writer>,
        mpl::pair<FILE        , detail::libtiff_writer>
    > writers;

    typedef mpl::vector1_c<format_tag, tiff> supported_image_formats;

    typedef detail::tiff_writer_view_data_t writer_view_data_t;

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment  = sizeof( void * ) );
    BOOST_STATIC_CONSTANT( bool        , builtin_conversion = false            );
};


class libtiff_image
    :
    public detail::libtiff_base,
    public detail::formatted_image<libtiff_image>
{
private:
    typedef detail::full_format_t full_format_t;

public:
    struct guard {};

public: /// \ingroup Construction
    explicit libtiff_image( char const * const file_name )
        :
        detail::libtiff_base( file_name, "r" )
    {
        get_format();
    }

    explicit libtiff_image( FILE & file )
        :
        detail::libtiff_base( file )
    {
        get_format();
    }

public:
    point2<std::ptrdiff_t> dimensions() const
    {
        return point2<std::ptrdiff_t>( get_field<uint32>( TIFFTAG_IMAGEWIDTH ), get_field<uint32>( TIFFTAG_IMAGELENGTH ) );
    }

    format_t const & format                      () const { return format_.number; }
    format_t const & closest_gil_supported_format() const { return format()      ; }

public: // Low-level (row, strip, tile) access
    bool can_do_row_access  () const { return !can_do_tile_access(); }
    bool can_do_strip_access() const { return /*...yet to implement...can_do_row_access();*/ false; }
    bool can_do_tile_access () const { return ::TIFFIsTiled( &lib_object() ) != 0; }

    std::size_t tile_size    () const { return ::TIFFTileSize   ( &lib_object() ); }
    std::size_t tile_row_size() const { return ::TIFFTileRowSize( &lib_object() ); }
    point2<std::ptrdiff_t> tile_dimensions() const
    {
        BOOST_ASSERT( can_do_tile_access() );
        return point2<std::ptrdiff_t>
        (
            get_field<uint32>( TIFFTAG_TILEWIDTH  ),
            get_field<uint32>( TIFFTAG_TILELENGTH )
        );
    }

    class sequential_row_access_state
        :
        private detail::libtiff_base::cumulative_result
    {
    public:
        using detail::libtiff_base::cumulative_result::failed;
        using detail::libtiff_base::cumulative_result::throw_if_error;

        BOOST_STATIC_CONSTANT( bool, throws_on_error = false );

    private: friend class libtiff_image;
        sequential_row_access_state() : position_( 0 ) {}

        unsigned int position_;
    };


    static sequential_row_access_state begin_sequential_row_access() { return sequential_row_access_state(); }

    void read_row( sequential_row_access_state & state, unsigned char * const p_row_storage, unsigned int const plane = 0 ) const
    {
        state.accumulate_greater
        (
            ::TIFFReadScanline( &lib_object(), p_row_storage, state.position_++, static_cast<tsample_t>( plane ) ),
            0
        );
    }


    typedef sequential_row_access_state sequential_tile_access_state;

    static sequential_tile_access_state begin_sequential_tile_access() { return begin_sequential_row_access(); }

    void read_tile( sequential_row_access_state & state, unsigned char * const p_tile_storage ) const
    {
        state.accumulate_greater
        (
            ::TIFFReadEncodedTile( &lib_object(), state.position_++, p_tile_storage, -1 ),
            0
        );
    }

private:
    template <typename T>
    T get_field( ttag_t const tag ) const
    {
        T value;
        #ifdef _MSC_VER
            T * p_value( &value );
            BOOST_VERIFY( ::TIFFVGetFieldDefaulted( &lib_object(), tag, gil_reinterpret_cast<va_list>( &p_value ) ) );
        #else
            BOOST_VERIFY( ::TIFFGetFieldDefaulted ( &lib_object(), tag,                                 &value    ) );
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

    void get_format()
    {
        int cumulative_result( true );

        unsigned const samples_per_pixel   ( get_field<uint16>( TIFFTAG_SAMPLESPERPIXEL ) );
        unsigned const bits_per_sample     ( get_field<uint16>( TIFFTAG_BITSPERSAMPLE   ) );
        unsigned const sample_format       ( get_field<uint16>( TIFFTAG_SAMPLEFORMAT    ) );
        unsigned const planar_configuration( get_field<uint16>( TIFFTAG_PLANARCONFIG    ) );
        unsigned const photometric         ( get_field<uint16>( TIFFTAG_PHOTOMETRIC     ) );
        unsigned const orientation         ( get_field<uint16>( TIFFTAG_ORIENTATION     ) );

        unsigned ink_set( 0 );
        bool unsupported_format( false );
        if ( samples_per_pixel == 4 )
        {
            switch ( photometric )
            {
                case PHOTOMETRIC_RGB:
                {
                    std::pair<uint16, uint16 *> const extrasamples( get_field<uint16, uint16 *>( TIFFTAG_EXTRASAMPLES, cumulative_result ) );
                    unsupported_format = ( extrasamples.first != 1 || *extrasamples.second != EXTRASAMPLE_UNASSALPHA );
                    break;
                }

                case PHOTOMETRIC_SEPARATED:
                {
                    ink_set = get_field<uint16>( TIFFTAG_INKSET );
                    unsupported_format = ( ink_set != INKSET_CMYK );
                    break;
                }

                default:
                    unsupported_format = true;
            }
        }

        detail::io_error_if( !cumulative_result || unsupported_format || ( orientation != ORIENTATION_TOPLEFT ), "Unsupported TIFF format" );

        format_.number = LIBTIFF_FORMAT( samples_per_pixel, bits_per_sample, ( sample_format == SAMPLEFORMAT_VOID ) ? SAMPLEFORMAT_UINT : sample_format, planar_configuration, photometric, ink_set );
    }

private:
    detail::full_format_t::format_bitfield const & format_bits() const { return format_.bits; }

    static unsigned int round_up_divide( unsigned int const dividend, unsigned int const divisor )
    {
        return ( dividend + divisor - 1 ) / divisor;
    }

private: // Private formatted_image_base interface.
    // Implementation note:
    //   MSVC 10 accepts friend base_t and friend class base_t, Clang 2.8
    // accepts friend class base_t, Apple Clang 1.6 and GCC (4.2 and 4.6) accept
    // neither.
    //                                        (13.01.2011.) (Domagoj Saric)
    friend class detail::formatted_image<libtiff_image>;

    struct tile_setup_t
        #ifndef __GNUC__
            : boost::noncopyable
        #endif // __GNUC__
    {
        tile_setup_t( libtiff_image const & source, point2<std::ptrdiff_t> const & dimensions, offset_t const offset, bool const nptcc )
            :
            tile_height                ( source.get_field<uint32>( TIFFTAG_TILELENGTH ) ),
            tile_width                 ( source.get_field<uint32>( TIFFTAG_TILEWIDTH  ) ),
            row_tiles                  ( source.get_field<uint32>( TIFFTAG_IMAGEWIDTH ) / tile_width ),
            size_of_pixel              ( ( source.format_bits().planar_configuration == PLANARCONFIG_CONTIG ? source.format_bits().samples_per_pixel : 1 ) * source.format_bits().bits_per_sample / 8 ),
            tile_width_bytes           ( tile_width * size_of_pixel ),
            tile_size_bytes            ( tile_width_bytes * tile_height ),
            p_tile_buffer              ( new unsigned char[ tile_size_bytes * ( nptcc ? source.format_bits().samples_per_pixel : 1 ) ] ),
            last_row_tile_width        ( modulo_unless_zero( dimensions.x, tile_width ) /*dimensions.x % tile_width*/ ),
            tiles_per_row              ( ( dimensions.x / tile_width ) + /*( last_row_tile_width != 0 )*/ ( ( dimensions.x % tile_width ) != 0 ) ),
            last_row_tile_width_bytes  ( last_row_tile_width * size_of_pixel ),
            last_row_tile_size_bytes   ( last_row_tile_width_bytes * tile_height  ),
            current_row_tiles_remaining( tiles_per_row ),
            starting_tile              ( offset / tile_height * tiles_per_row ),
            rows_to_skip               ( offset % tile_height ),
            number_of_tiles            ( round_up_divide( dimensions.y, tile_height ) * tiles_per_row * ( source.format_bits().planar_configuration == PLANARCONFIG_SEPARATE ? source.format_bits().samples_per_pixel : 1 ) )
        {
            BOOST_ASSERT( static_cast<tsize_t>( tile_width_bytes ) == ::TIFFTileRowSize( &source.lib_object() ) );
            BOOST_ASSERT( static_cast<tsize_t>( tile_size_bytes  ) == ::TIFFTileSize   ( &source.lib_object() ) );
            BOOST_ASSERT( starting_tile + number_of_tiles <= ::TIFFNumberOfTiles( &source.lib_object() ) );
            if ( tile_height > static_cast<uint32>( dimensions.y ) )
            {
                rows_to_read_per_tile = end_rows_to_read = dimensions.y;
            }
            else
            {
                end_rows_to_read = modulo_unless_zero( dimensions.y - ( tile_height - rows_to_skip ), tile_height );
                bool const starting_at_last_row( ( number_of_tiles - starting_tile ) <= tiles_per_row );
                rows_to_read_per_tile = starting_at_last_row ? end_rows_to_read : tile_height;
            }
        }

        uint32 const tile_height;
        uint32 const tile_width ;
        uint32 const row_tiles  ;

        unsigned int const size_of_pixel   ;
        size_t       const tile_width_bytes;
        tsize_t      const tile_size_bytes ;

        scoped_array<unsigned char> p_tile_buffer;

        unsigned int const last_row_tile_width        ;
        unsigned int const tiles_per_row              ;
        unsigned int const last_row_tile_width_bytes  ;
        unsigned int const last_row_tile_size_bytes   ;
        unsigned int       current_row_tiles_remaining;
        ttile_t      const starting_tile              ;
        unsigned int       rows_to_skip               ;
        ttile_t      const number_of_tiles            ;
        unsigned int /*const*/ end_rows_to_read           ;
        unsigned int       rows_to_read_per_tile      ;

    private:
        static unsigned int modulo_unless_zero( unsigned int const dividend, unsigned int const divisor )
        {
            unsigned int const modulo( dividend % divisor );
            return ( modulo != 0 ) ? modulo : divisor;
        }
    };

    struct skip_row_results_t
    {
        unsigned int rows_per_strip;
        unsigned int rows_to_read_using_scanlines;
        unsigned int starting_strip;
    };

    void raw_copy_to_prepared_view( view_data_t const & view_data ) const
    {
        cumulative_result result;

        if ( can_do_tile_access() )
        {
            tile_setup_t setup( *this, view_data.dimensions_, view_data.offset_, false );

            ttile_t current_tile( 0 );

            for ( unsigned int plane( 0 ); plane < view_data.number_of_planes_; ++plane )
            {
                unsigned char * p_target( view_data.plane_buffers_[ plane ] );
                for ( current_tile += setup.starting_tile; current_tile < setup.number_of_tiles; ++current_tile )
                {
                    bool         const last_row_tile        ( !--setup.current_row_tiles_remaining                                     );
                    unsigned int const this_tile_width_bytes( last_row_tile ? setup.last_row_tile_width_bytes : setup.tile_width_bytes );

                    result.accumulate_equal( ::TIFFReadEncodedTile( &lib_object(), current_tile, setup.p_tile_buffer.get(), setup.tile_size_bytes ), setup.tile_size_bytes );
                    unsigned char const * p_tile_buffer_location( setup.p_tile_buffer.get() + ( setup.rows_to_skip * this_tile_width_bytes ) );
                    unsigned char       * p_target_local        ( p_target                                                                   );
                    for ( unsigned int row( setup.rows_to_skip ); row < setup.rows_to_read_per_tile; ++row )
                    {
                        std::memcpy( p_target_local, p_tile_buffer_location, this_tile_width_bytes );
                        memunit_advance( p_tile_buffer_location, setup.tile_width_bytes );
                        memunit_advance( p_target_local        , view_data.stride_      );
                    }
                    memunit_advance( p_target, this_tile_width_bytes );
                    if ( last_row_tile )
                    {
                        p_target += ( ( setup.rows_to_read_per_tile - 1 - setup.rows_to_skip ) * view_data.stride_ );
                        setup.rows_to_skip = 0;
                        setup.current_row_tiles_remaining = setup.tiles_per_row;
                        bool const next_row_is_last_row( ( setup.number_of_tiles - ( current_tile + 1 ) ) == setup.tiles_per_row );
                        if ( next_row_is_last_row )
                            setup.rows_to_read_per_tile = setup.end_rows_to_read;
                    }
                    //BOOST_ASSERT( p_tile_buffer_location == ( setup.p_tile_buffer.get() + this_tile_size_bytes ) || ( setup.rows_to_read_per_tile != setup.tile_height ) );
                    BOOST_ASSERT( p_target <= view_data.plane_buffers_[ plane ] + ( view_data.stride_ * view_data.dimensions_.y ) );
                }
                BOOST_ASSERT( p_target == view_data.plane_buffers_[ plane ] + ( view_data.stride_ * view_data.dimensions_.y ) );
            }
        }
        else
        {
            BOOST_ASSERT( ::TIFFScanlineSize( &lib_object() ) <= static_cast<tsize_t>( view_data.stride_ ) );
            for ( unsigned int plane( 0 ); plane < view_data.number_of_planes_; ++plane )
            {
                unsigned char * buf( view_data.plane_buffers_[ plane ] );
                skip_row_results_t skip_result( skip_to_row( view_data.offset_, plane, buf, result ) );
                ttile_t const number_of_strips( ( view_data.dimensions_.y - skip_result.rows_to_read_using_scanlines + skip_result.rows_per_strip - 1 ) / skip_result.rows_per_strip );
                skip_result.rows_to_read_using_scanlines = std::min<unsigned int>( skip_result.rows_to_read_using_scanlines, view_data.dimensions_.y );

                unsigned int row( view_data.offset_ );
                while ( row != ( view_data.offset_ + skip_result.rows_to_read_using_scanlines ) )
                {
                    result.accumulate_greater( ::TIFFReadScanline( &lib_object(), buf, row++, static_cast<tsample_t>( plane ) ), 0 );
                    buf += view_data.stride_;
                }

                unsigned int const view_strip_increment( view_data.stride_ * skip_result.rows_per_strip );
                if ( view_strip_increment == static_cast<unsigned int>( ::TIFFStripSize( &lib_object() ) ) )
                {
                    for ( unsigned int strip( skip_result.starting_strip ); strip < number_of_strips; ++strip )
                    {
                        result.accumulate_greater( ::TIFFReadEncodedStrip( &lib_object(), strip, buf, view_strip_increment ), 0 );
                        buf += view_strip_increment;
                        row += skip_result.rows_per_strip;
                    }
                }

                unsigned int const target_row( view_data.offset_ + view_data.dimensions_.y );
                while ( row < target_row )
                {
                    result.accumulate_greater( ::TIFFReadScanline( &lib_object(), buf, row++, static_cast<tsample_t>( plane ) ), 0 );
                    buf += view_data.stride_;
                }
            }
        }

        result.throw_if_error();
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    // generic_convert_to_prepared_view()
    // ----------------------------------
    //
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \todo Cleanup, document, split up and refactor this humongous beast
    /// (remove duplication, extract non-template and shared functionality...).
    ///                                    (16.09.2010.) (Domagoj Saric)
    ///
    ////////////////////////////////////////////////////////////////////////////

    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
        using namespace detail;

        typedef typename get_original_view_t<TargetView>::type original_target_view_t;

        typedef typename mpl::eval_if
            <
                is_planar<MyView>,
                nth_channel_view_type<original_target_view_t>,
                get_original_view_t<TargetView>
            >::type local_target_view_t;
        typedef typename get_original_view_t<local_target_view_t>::type::x_iterator target_x_iterator;
        typedef typename get_original_view_t<local_target_view_t>::type::y_iterator target_y_iterator;

        typedef typename mpl::eval_if
            <
                is_planar<MyView>,
                nth_channel_view_type<MyView>,
                mpl::identity<MyView>
            >::type::value_type my_pixel_t;

        typedef mpl::bool_
        <
             is_planar<MyView                >::value &&
            !is_planar<original_target_view_t>::value &&
            (
                !is_same<typename color_space_type    <MyView>::type, typename color_space_type    <original_target_view_t>::type>::value ||
                !is_same<typename channel_mapping_type<MyView>::type, typename channel_mapping_type<original_target_view_t>::type>::value
            )
        > nondirect_planar_to_contig_conversion_t;

        cumulative_result result;

        typedef mpl::int_<is_planar<MyView>::value ? num_channels<MyView>::value : 1> number_of_planes_t;

        dimensions_t const & dimensions( original_view( view ).dimensions() );

        ////////////////////////////////////////////////////////////////////////
        // Tiled
        ////////////////////////////////////////////////////////////////////////

        if ( ::TIFFIsTiled( &lib_object() ) )
        {
            tile_setup_t setup
            (
                *this,
                dimensions,
                get_offset<offset_t>( view ),
                nondirect_planar_to_contig_conversion_t::value
            );

            unsigned int const tiles_per_plane( setup.number_of_tiles / number_of_planes_t::value );
            ttile_t            current_tile   ( 0                                                 );

            if ( nondirect_planar_to_contig_conversion_t::value )
            {
                // For NPTCC there is no need for target view
                // planar<->non-planar adjustment because here we read whole
                // pixels before copying to the target view...
                typename original_target_view_t::y_iterator p_target( original_view( view ).y_at( 0, 0 ) );

                typename MyView::x_iterator const buffer_iterator
                (
                    make_planar_buffer_iterator<typename MyView::x_iterator>
                    (
                        setup.p_tile_buffer.get(),
                        setup.tile_size_bytes,
                        number_of_planes_t()
                    )
                );

                unsigned int horizontal_offset( 0 );

                for ( current_tile += setup.starting_tile; current_tile < tiles_per_plane; ++current_tile )
                {
                    bool         const last_row_tile  ( !--setup.current_row_tiles_remaining                         );
                    unsigned int const this_tile_width( last_row_tile ? setup.last_row_tile_width : setup.tile_width );

                    for ( unsigned int channel_tile( 0 ); channel_tile < number_of_planes_t::value; ++channel_tile )
                    {
                        ttile_t const raw_tile_number( current_tile + ( channel_tile * tiles_per_plane ) );
                        result.accumulate_equal
                        (
                            ::TIFFReadEncodedTile
                            (
                                &lib_object(),
                                raw_tile_number,
                                //buffer_iterator.at_c_dynamic( channel_tile ),
                                &(*buffer_iterator)[ channel_tile ],
                                setup.tile_size_bytes
                            ),
                            setup.tile_size_bytes
                        );
                    }

                    typename          MyView       ::x_iterator p_source_pixel( buffer_iterator + ( setup.rows_to_skip * this_tile_width ) );
                    typename original_target_view_t::x_iterator p_target_pixel( p_target.base() + horizontal_offset                        );
                    for ( unsigned int row( setup.rows_to_skip ); row < setup.rows_to_read_per_tile; ++row )
                    {
                        typename MyView::x_iterator const local_end( p_source_pixel + this_tile_width );
                        while ( p_source_pixel < local_end )
                        {
                            converter( *p_source_pixel, *p_target_pixel );
                            ++p_source_pixel;
                            ++p_target_pixel;
                        }
                        BOOST_ASSERT( p_source_pixel == local_end );
                        p_source_pixel += setup.tile_width - this_tile_width;
                        memunit_advance( p_target_pixel, original_view( view ).pixels().row_size() );
                        p_target_pixel -= this_tile_width;
                    }
                    if ( last_row_tile )
                    {
                        p_target += ( setup.rows_to_read_per_tile /*- 1*/ - setup.rows_to_skip );
                        setup.rows_to_skip = 0;
                        setup.current_row_tiles_remaining = setup.tiles_per_row;
                        bool const next_row_is_last_row( ( tiles_per_plane - ( current_tile + 1 ) ) == setup.tiles_per_row );
                        if ( next_row_is_last_row )
                            setup.rows_to_read_per_tile = setup.end_rows_to_read;
                    }
                    else
                        horizontal_offset += setup.tile_width;
                    //...make NPTCC version...BOOST_ASSERT( p_source_pixel == reinterpret_cast<my_pixel_t const *>( setup.p_tile_buffer.get() + this_tile_size_bytes ) || ( setup.rows_to_read_per_tile != setup.tile_height ) );
                    BOOST_ASSERT( p_target <= original_view( view ).col_end( 0 ) );
                }
                BOOST_ASSERT( p_target == original_view( view ).col_end( 0 ) );
            }
            else // non NPTCC...
            {
                for
                (
                    unsigned int plane( 0 ), current_plane_end_tile( tiles_per_plane );
                    plane < number_of_planes_t::value;
                    ++plane, current_plane_end_tile += tiles_per_plane
                )
                {
                    local_target_view_t const & target_view( adjust_target_to_my_view( original_view( view ), plane, is_planar<MyView>() ) );
                    target_y_iterator p_target( target_view.y_at( 0, 0 ) );
                    BOOST_ASSERT( p_target.step() == original_view( view ).pixels().row_size() );

                    unsigned int horizontal_offset                  ( 0                           );
                    unsigned int current_plane_rows_to_read_per_tile( setup.rows_to_read_per_tile );
                    unsigned int current_plane_rows_to_skip         ( setup.rows_to_skip          );

                    for ( current_tile += setup.starting_tile; current_tile < current_plane_end_tile; ++current_tile )
                    {
                        bool         const last_row_tile        ( !--setup.current_row_tiles_remaining                                     );
                        //unsigned int const this_tile_size_bytes ( last_row_tile ? setup.last_row_tile_size_bytes  : setup.tile_size_bytes  );
                        unsigned int const this_tile_width_bytes( last_row_tile ? setup.last_row_tile_width_bytes : setup.tile_width_bytes );

                        result.accumulate_equal( ::TIFFReadEncodedTile( &lib_object(), current_tile, setup.p_tile_buffer.get(), setup.tile_size_bytes ), setup.tile_size_bytes );

                        my_pixel_t const * p_source_pixel( gil_reinterpret_cast_c<my_pixel_t const *>( setup.p_tile_buffer.get() + ( current_plane_rows_to_skip * this_tile_width_bytes ) ) );
                        target_x_iterator  p_target_pixel( p_target.base() + horizontal_offset );
                        for ( unsigned int row( current_plane_rows_to_skip ); row < current_plane_rows_to_read_per_tile; ++row )
                        {
                            my_pixel_t const * const local_end( memunit_advanced( p_source_pixel, this_tile_width_bytes ) );
                            while ( p_source_pixel < local_end )
                            {
                                converter( *p_source_pixel, *p_target_pixel );
                                ++p_source_pixel;
                                ++p_target_pixel;
                            }
                            BOOST_ASSERT( p_source_pixel == local_end );
                            memunit_advance( p_source_pixel, setup.tile_width_bytes - this_tile_width_bytes );
                            memunit_advance( p_target_pixel, original_view( view ).pixels().row_size() - ( this_tile_width_bytes * memunit_step( p_target_pixel ) / memunit_step( p_source_pixel ) ) );
                        }
                        BOOST_ASSERT
                        (
                            ( p_source_pixel == reinterpret_cast<my_pixel_t const *>( setup.p_tile_buffer.get() + setup.tile_size_bytes ) ) ||
                            ( current_plane_rows_to_read_per_tile != setup.tile_height )
                        );
                        if ( last_row_tile )
                        {
                            p_target += ( current_plane_rows_to_read_per_tile /*- 1*/ - current_plane_rows_to_skip );
                            current_plane_rows_to_skip = 0;
                            horizontal_offset          = 0;
                            setup.current_row_tiles_remaining = setup.tiles_per_row;
                            bool const next_row_is_last_row( ( current_plane_end_tile - ( current_tile + 1 ) ) == setup.tiles_per_row );
                            if ( next_row_is_last_row )
                                current_plane_rows_to_read_per_tile = setup.end_rows_to_read;
                        }
                        else
                        {
                            BOOST_ASSERT( this_tile_width_bytes == setup.tile_width_bytes );
                            horizontal_offset += setup.tile_width;
                        }
                        BOOST_ASSERT( p_target.base() <= target_view.end().x() );
                    }
                    BOOST_ASSERT( p_target.base() == target_view.end().x() );
                }
            }
        }
        else
        ////////////////////////////////////////////////////////////////////////
        // Striped
        ////////////////////////////////////////////////////////////////////////
        {
            scanline_buffer_t<my_pixel_t> const scanline_buffer( *this, nondirect_planar_to_contig_conversion_t::value() );

            if ( nondirect_planar_to_contig_conversion_t::value )
            {
                typename original_target_view_t::y_iterator p_target( original_view( view ).y_at( 0, 0 ) );
                unsigned int       row       ( get_offset<offset_t>( view ) );
                unsigned int const target_row( row + dimensions.y           );

                typename MyView::x_iterator const buffer_iterator
                (
                    make_planar_buffer_iterator<typename MyView::x_iterator>
                    (
                        scanline_buffer.begin(),
                        dimensions.x,
                        number_of_planes_t()
                    )
                );

                while ( row != target_row )
                {
                    for ( unsigned int plane( 0 ); plane < number_of_planes_t::value; ++plane )
                    {
                        tdata_t const p_buffer( &(*buffer_iterator)[ plane ] );
                        //...zzz...yup...not the most efficient thing in the universe...
                        skip_to_row( get_offset<offset_t>( view ) + row, plane, p_buffer, result );
                        result.accumulate_greater( ::TIFFReadScanline( &lib_object(), p_buffer, row, static_cast<tsample_t>( plane ) ), 0 );
                    }
                    typename          MyView       ::x_iterator p_source_pixel( buffer_iterator );
                    typename original_target_view_t::x_iterator p_target_pixel( p_target.base() );
                    while ( &(*p_source_pixel)[ 0 ] < &(*scanline_buffer.end())[ 0 ] )
                    {
                        converter( *p_source_pixel, *p_target_pixel );
                        ++p_source_pixel;
                        ++p_target_pixel;
                    }
                    ++p_target;
                    ++row;
                }
            }
            else
            {
                for ( unsigned int plane( 0 ); plane < number_of_planes_t::value; ++plane )
                {
                    if ( is_offset_view<TargetView>::value )
                        skip_to_row( get_offset<offset_t>( view ), plane, scanline_buffer.begin(), result );

                    local_target_view_t const & target_view( adjust_target_to_my_view( original_view( view ), plane, is_planar<MyView>() ) );
                    target_y_iterator p_target( target_view.y_at( 0, 0 ) );
                    unsigned int       row       ( get_offset<offset_t>( view ) );
                    unsigned int const target_row( row + dimensions.y           );
                    while ( row != target_row )
                    {
                        result.accumulate_greater( ::TIFFReadScanline( &lib_object(), scanline_buffer.begin(), row++, static_cast<tsample_t>( plane ) ), 0 );
                        my_pixel_t const * p_source_pixel( scanline_buffer.begin() );
                        target_x_iterator  p_target_pixel( p_target.base()         );
                        while ( p_source_pixel != scanline_buffer.end() )
                        {
                            converter( *p_source_pixel, *p_target_pixel );
                            ++p_source_pixel;
                            ++p_target_pixel;
                        }
                        ++p_target;
                    }
                }
            }
        }

        result.throw_if_error();
    }

    static std::size_t cached_format_size( format_t const format )
    {
        full_format_t::format_bitfield const & bits( reinterpret_cast<full_format_t const &>( format ).bits );
        return bits.bits_per_sample * ( ( bits.planar_configuration == PLANARCONFIG_CONTIG ) ? bits.samples_per_pixel : 1 ) / 8;
    }

private:
    template <typename View>
    static typename nth_channel_view_type<View>::type
    adjust_target_to_my_view( View const & view, unsigned int const plane, mpl::true_ /*source is planar*/ )
    {
        return nth_channel_view( view, plane );
    }

    template <typename View>
    static View const &
    adjust_target_to_my_view( View const & view, unsigned int const plane, mpl::false_ /*source is not planar*/ )
    {
        BOOST_ASSERT( plane == 0 );
        ignore_unused_variable_warning( plane );
        return view;
    }

    template <typename PixelIterator>
    static PixelIterator
    make_planar_buffer_iterator( void * const p_buffer, unsigned int /*row_width*/, mpl::int_<1> )
    {
        BOOST_ASSERT( !"Should not get called." );
        return static_cast<PixelIterator>( p_buffer );
    }

    template <typename PlanarPixelIterator>
    static PlanarPixelIterator
    make_planar_buffer_iterator( void * const p_buffer, unsigned int const row_width, mpl::int_<2> )
    {
        typedef typename channel_type<PlanarPixelIterator>::type * ptr_t;
        unsigned int const stride( row_width * memunit_step( ptr_t() ) );
        ptr_t const first ( static_cast<ptr_t>( p_buffer ) );
        ptr_t const second( first + stride                 );
        return PlanarPixelIterator( first, second );
    }

    template <typename PlanarPixelIterator>
    static PlanarPixelIterator
    make_planar_buffer_iterator( void * const p_buffer, unsigned int const row_width, mpl::int_<3> )
    {
        typedef typename channel_type<PlanarPixelIterator>::type * ptr_t;
        unsigned int const stride( row_width * memunit_step( ptr_t() ) );
        ptr_t const first ( static_cast<ptr_t>( p_buffer ) );
        ptr_t const second( first  + stride                );
        ptr_t const third ( second + stride                );
        return PlanarPixelIterator( first, second, third );
    }

    template <typename PlanarPixelIterator>
    static PlanarPixelIterator
    make_planar_buffer_iterator( void * const p_buffer, unsigned int const row_width, mpl::int_<4> )
    {
        //...eradicate this triplication...
        typedef typename channel_type<PlanarPixelIterator>::type * ptr_t;
        unsigned int const stride( row_width * memunit_step( ptr_t() ) );
        ptr_t const first ( static_cast<ptr_t>( p_buffer ) );
        ptr_t const second( memunit_advanced( first , stride ) );
        ptr_t const third ( memunit_advanced( second, stride ) );
        ptr_t const fourth( memunit_advanced( third , stride ) );
        return PlanarPixelIterator( first, second, third, fourth );
    }

    // Implementation note:
    //   Because of plain RVO incapable compilers (like Clang or not the latest
    // GCCs) a simple std::pair with a scoped_array cannot be used.
    //                                        (11.01.2011.) (Domagoj Saric)
    //typedef std::pair
    //<
    //    scoped_array<unsigned char> const,
    //    unsigned char const * const
    //> generic_scanline_buffer_t;

    class scanline_buffer_base_t : boost::noncopyable
    {
    protected:
        explicit scanline_buffer_base_t( std::size_t const size )
            :
            p_begin_( new unsigned char[ size ] ),
            p_end_  ( p_begin_ + size           )
        {}

        // This one makes the end pointer point to the end of the scanline/row
        // of the first plane not the end of the buffer itself...ugh...to be cleaned up...
        explicit scanline_buffer_base_t( std::pair<std::size_t, std::size_t> const size_to_allocate_size_to_report_pair )
            :
            p_begin_( new unsigned char[ size_to_allocate_size_to_report_pair.first ] ),
            p_end_  ( p_begin_ + size_to_allocate_size_to_report_pair.second          )
        {}

        ~scanline_buffer_base_t() { delete[] p_begin_; }

        BF_NOTHROWNORESTRICTNOALIAS unsigned char       * begin() const { return p_begin_; }
        BF_NOTHROWNORESTRICTNOALIAS unsigned char const * end  () const { return p_end_  ; }

        static std::size_t scanline_buffer_construction( libtiff_image const & tiff )
        {
            return ( ::TIFFScanlineSize( &tiff.lib_object() ) );
        }

        static std::pair<std::size_t, std::size_t> planar_scanline_buffer_construction( libtiff_image const & tiff )
        {
            std::size_t const scanline_size  ( tiff.format_bits().bits_per_sample * tiff.dimensions().x / 8 );
            std::size_t const allocation_size( scanline_size * tiff.format_bits().samples_per_pixel         );
            return std::pair<std::size_t, std::size_t>( allocation_size, scanline_size );
        }

    private:
        unsigned char       * const p_begin_;
        unsigned char const * const p_end_  ;
    };

    template <typename Pixel>
    class scanline_buffer_t : public scanline_buffer_base_t
    {
    public:
        scanline_buffer_t( libtiff_image const & tiff, mpl::true_ /*      nptcc  */ ) : scanline_buffer_base_t( planar_scanline_buffer_construction( tiff ) ) {}
        scanline_buffer_t( libtiff_image const & tiff, mpl::false_ /* not nptcc  */ ) : scanline_buffer_base_t( scanline_buffer_construction       ( tiff ) ) {}

        Pixel       * begin() const { return gil_reinterpret_cast  <Pixel       *>( scanline_buffer_base_t::begin() ); }
        Pixel const * end  () const { return gil_reinterpret_cast_c<Pixel const *>( scanline_buffer_base_t::end  () ); }
    };

    template <typename Pixel>
    class planar_scanline_buffer_t : scanline_buffer_t<Pixel>
    {
    public:
        planar_scanline_buffer_t( libtiff_image const & image ) : scanline_buffer_t<Pixel>( tiff, mpl::true_() ) {}
    };

    skip_row_results_t BF_NOTHROWNOALIAS skip_to_row
    (
        unsigned int        const row_to_skip_to,
        unsigned int        const sample,
        tdata_t             const buffer,
        cumulative_result &       error_result
    ) const
    {
        BOOST_ASSERT( !::TIFFIsTiled( &lib_object() ) );

        skip_row_results_t result;
        result.rows_per_strip               = get_field<uint32>( TIFFTAG_ROWSPERSTRIP );
        unsigned int const number_of_rows_to_skip_using_scanlines( row_to_skip_to % result.rows_per_strip );
        result.starting_strip               = ( row_to_skip_to / result.rows_per_strip ) + ( number_of_rows_to_skip_using_scanlines != 0 ) + sample * get_field<uint32>( TIFFTAG_IMAGEWIDTH );
        result.rows_to_read_using_scanlines = row_to_skip_to ? ( result.rows_per_strip - number_of_rows_to_skip_using_scanlines - 1 ) : 0;

        bool const canSkipDirectly
        (
            ( result.rows_per_strip                    == 1                ) ||
            ( get_field<uint16>( TIFFTAG_COMPRESSION ) == COMPRESSION_NONE )
        );
        unsigned int row
        (
            canSkipDirectly
                ? row_to_skip_to
                : ( row_to_skip_to - number_of_rows_to_skip_using_scanlines )
        );
        while ( row < row_to_skip_to )
        {
            error_result.accumulate_greater( ::TIFFReadScanline( &lib_object(), buffer, row++, static_cast<tsample_t>( sample ) ), 0 );
        }

        //BOOST_ASSERT( !row_to_skip_to || ( ::TIFFCurrentRow( &lib_object() ) == row_to_skip_to ) );

        return result;
    }

private:
    full_format_t format_;
};

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif


//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libtiff_image_hpp
