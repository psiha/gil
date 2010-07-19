////////////////////////////////////////////////////////////////////////////////
///
/// \file libtiff_private_base.hpp
/// ------------------------------
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
#ifndef libtiff_private_base_hpp__ARGH6951_A00F_4E0D_9783_488A49B1CA2B
#define libtiff_private_base_hpp__ARGH6951_A00F_4E0D_9783_488A49B1CA2B
//------------------------------------------------------------------------------
#include "formatted_image.hpp"
#include "io_error.hpp"

#include "detail/libx_shared.hpp"

#if BOOST_MPL_LIMIT_VECTOR_SIZE < 35
    ...error...libtiff support requires mpl vectors of size 35...
#endif

#include <boost/array.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/smart_ptr/scoped_array.hpp>

#include "tiff.h"
#include "tiffio.h"

#include <cstdio>
#include <set>
#include "io.h"
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

unsigned short const unknown( static_cast<unsigned short>( -1 ) );

#define LIBTIFF_SPP(         param ) ( ( param ) << ( 0 ) )
#define LIBTIFF_BPS(         param ) ( ( param ) << ( 0 + 3 ) )
#define LIBTIFF_FMT(         param ) ( ( param ) << ( 0 + 3 + 5 ) )
#define LIBTIFF_PLANAR(      param ) ( ( param ) << ( 0 + 3 + 5 + 3 ) )
#define LIBTIFF_PHOTOMETRIC( param ) ( ( param ) << ( 0 + 3 + 5 + 3 + 2 ) )
#define LIBTIFF_INKSET(      param ) ( ( param ) << ( 0 + 3 + 5 + 3 + 2 + 2 ) )

#define LIBTIFF_FORMAT( spp, bps, sample_format, planar_config, photometric_interpretation, inkset )    \
    (                                                                                                   \
        LIBTIFF_SPP(         spp                        ) |                                             \
        LIBTIFF_BPS(         bps                        ) |                                             \
        LIBTIFF_FMT(         sample_format              ) |                                             \
        LIBTIFF_PLANAR(      planar_config              ) |                                             \
        LIBTIFF_PHOTOMETRIC( photometric_interpretation ) |                                             \
        LIBTIFF_INKSET(      inkset                     )                                               \
    )



union full_format_t
{
    struct format_bitfield
    {
        unsigned int samples_per_pixel    : 3;
        unsigned int bits_per_sample      : 5;
        unsigned int sample_format        : 3;
        unsigned int planar_configuration : 2;
        unsigned int photometric          : 2;
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


struct view_libtiff_format
{
    template <class View>
    struct apply : gil_to_libtiff_format<typename View::value_type, is_planar<View>::value> {};
};


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
    return /*std*/::_filelength( /*std*/::_fileno( gil_reinterpret_cast<FILE *>( fd ) ) );
}

inline int FILE_map_proc( thandle_t /*handle*/, tdata_t * /*pbase*/, toff_t * /*psize*/ )
{
    return 0;
}

inline void FILE_unmap_proc( thandle_t /*handle*/, tdata_t /*base*/, toff_t /*size*/ )
{
}


struct libtiff_guard {};


#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif


struct tiff_view_data_t
{
    template <class View>
    tiff_view_data_t( View const & view, generic_vertical_roi::offset_t const offset )
        :
        offset_    ( offset                                     ),
        dimensions_( view.dimensions()                          ),
        stride_    ( view.pixels().row_size()                   )
        #ifdef _DEBUG
            ,format_( view_libtiff_format::apply<View>::value   )
        #endif
    {
        set_buffers( view, is_planar<View>() );
    }

    void set_format( full_format_t::format_id const format )
    {
        //BOOST_ASSERT( ( format_ == format ) && !"libtiff does not provide builtin conversion." );
        ignore_unused_variable_warning( format );
    }

    generic_vertical_roi::offset_t         offset_;
    point2<std::ptrdiff_t>         const & dimensions_;
    unsigned int                           stride_;
    unsigned int                           number_of_planes_;
    array<unsigned char *, 4>              plane_buffers_;

    #ifdef _DEBUG
        unsigned int format_;
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

class libtiff_image;

template <>
struct formatted_image_traits<libtiff_image>
{
    typedef full_format_t::format_id format_t;

    typedef libtiff_supported_pixel_formats supported_pixel_formats_t;

    typedef generic_vertical_roi roi_t;

    typedef view_libtiff_format view_to_native_format;

    typedef tiff_view_data_t view_data_t;

    template <class View>
    struct is_supported : mpl::true_ {};

    BOOST_STATIC_CONSTANT( unsigned int, desired_alignment = 0 );

    BOOST_STATIC_CONSTANT( bool, builtin_conversion = false );
};


class libtiff_image
    :
    public  detail::formatted_image<libtiff_image>
{
public: /// \ingroup Construction
    explicit libtiff_image( char const * const file_name )
        :
        p_tiff_( ::TIFFOpen( file_name, "r" ) )
    {
        BOOST_ASSERT( file_name );
        constructor_tail();
    }

    explicit libtiff_image( FILE * const p_file )
        :
        p_tiff_
        (
            ::TIFFClientOpen
            (
                "", "",
                p_file,
                &FILE_read_proc,
                &FILE_write_proc,
                &FILE_seek_proc,
                &FILE_close_proc_nop,
                &FILE_size_proc,
                &FILE_map_proc,
                &FILE_unmap_proc
            )
        )
    {
        BOOST_ASSERT( p_file );
        constructor_tail();
    }

    ~libtiff_image()
    {
        ::TIFFClose( p_tiff_ );
    }

public:
    point2<std::ptrdiff_t> dimensions() const
    {
        return point2<std::ptrdiff_t>( get_field<uint32>( TIFFTAG_IMAGEWIDTH ), get_field<uint32>( TIFFTAG_IMAGELENGTH ) );
    }

    format_t const & format                      () const { return format_.number; }
    format_t const & closest_gil_supported_format() const { return format()      ; }

    static std::size_t format_size( format_t const format )
    {
        full_format_t::format_bitfield const & bits( reinterpret_cast<full_format_t const &>( format ).bits );
        return bits.bits_per_sample * ( ( bits.planar_configuration == PLANARCONFIG_CONTIG ) ? bits.samples_per_pixel : 1 ) / 8;
    }

    full_format_t::format_bitfield const & format_bits() const {return format_.bits; }


    TIFF       & lib_object()       { return *p_tiff_; }
    TIFF const & lib_object() const { return const_cast<libtiff_image &>( *this ).lib_object(); }

private:
    template <typename T>
    T get_field( ttag_t const tag ) const
    {
        T value; T * p_value( &value );
        BOOST_VERIFY( ::TIFFVGetFieldDefaulted( p_tiff_, tag, gil_reinterpret_cast<va_list>( &p_value ) ) );
        return value;
    }

    template <typename T1, typename T2>
    std::pair<T1, T2> get_field( ttag_t const tag, int & cumulative_result ) const
    {
        std::pair<T1, T2> value;
        cumulative_result &= ::TIFFGetFieldDefaulted( p_tiff_, tag, &value.first, &value.second );
        return value;
    }

    void get_format()
    {
        int cumulative_result( true );

        unsigned const samples_per_pixel   ( get_field<uint16>( TIFFTAG_SAMPLESPERPIXEL ) );
        unsigned const bits_per_sample     ( get_field<uint16>( TIFFTAG_BITSPERSAMPLE   ) );
        unsigned const sample_format       ( get_field<uint16>( TIFFTAG_SAMPLEFORMAT    ) );
        unsigned const planar_configuration( get_field<uint16>( TIFFTAG_PLANARCONFIG    ) );
        //...mrmlj...
        unsigned /*const*/ photometric         ( get_field<uint16>( TIFFTAG_PHOTOMETRIC     ) ); if ( photometric == PHOTOMETRIC_MINISWHITE ) photometric = PHOTOMETRIC_MINISBLACK;

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

        io_error_if( !cumulative_result || unsupported_format || ( orientation != ORIENTATION_TOPLEFT ), "TeeF" );

        format_.number = LIBTIFF_FORMAT( samples_per_pixel, bits_per_sample, ( sample_format == SAMPLEFORMAT_VOID ) ? SAMPLEFORMAT_UINT : sample_format, planar_configuration, photometric, ink_set );
    }


private:
    void construction_check() const
    {
        io_error_if( !p_tiff_, "Failed to open TIFF input file" );
    }

    void constructor_tail()
    {
        construction_check();
        get_format        ();
    }

    static unsigned int round_up_divide( unsigned int const dividend, unsigned int const divisor )
    {
        return ( dividend + divisor - 1 ) / divisor;
    }

private: // Private formatted_image_base interface.
    friend base_t;
    struct tile_setup_t : noncopyable
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
            BOOST_ASSERT( static_cast<tsize_t>( tile_width_bytes ) == ::TIFFTileRowSize( source.p_tiff_ ) );
            BOOST_ASSERT( static_cast<tsize_t>( tile_size_bytes  ) == ::TIFFTileSize   ( source.p_tiff_ ) );
            BOOST_ASSERT( starting_tile + number_of_tiles <= ::TIFFNumberOfTiles( source.p_tiff_ ) );
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

            #ifdef _DEBUG
                std::memset( p_tile_buffer.get(), 0xFF, tile_size_bytes * ( nptcc ? source.format_bits().samples_per_pixel : 1 ) );
            #endif // _DEBUG
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

    class cumulative_result
    {
    public:
        cumulative_result() : result_( true ) {}

        void accumulate( bool const new_result ) { result_ &= new_result; }
        template <typename T1, typename T2>
        void accumulate_equal( T1 const new_result, T2 const desired_result ) { accumulate( new_result == desired_result ); }
        template <typename T>
        void accumulate_different( T const new_result, T const undesired_result ) { accumulate( new_result != undesired_result ); }

        void throw_if_error() const { io_error_if( result_ != true, "Error reading TIFF file" ); }

        bool & get() { return result_; }

    private:
        bool result_;
    };

    void raw_copy_to_prepared_view( tiff_view_data_t const view_data ) const
    {
        cumulative_result result;

        if ( ::TIFFIsTiled( p_tiff_ ) )
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

                    result.accumulate_equal( ::TIFFReadEncodedTile( p_tiff_, current_tile, setup.p_tile_buffer.get(), setup.tile_size_bytes ), setup.tile_size_bytes );
                    unsigned char const * p_tile_buffer_location( setup.p_tile_buffer.get() + ( setup.rows_to_skip * this_tile_width_bytes )           );
                    unsigned char       * p_target_local        ( p_target                                                                             );
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
                    BOOST_ASSERT( p_tile_buffer_location == ( setup.p_tile_buffer.get() + this_tile_size_bytes ) || ( setup.rows_to_read_per_tile != setup.tile_height ) );
                    BOOST_ASSERT( p_target <= view_data.plane_buffers_[ plane ] + ( view_data.stride_ * view_data.dimensions_.y ) );
                }
                BOOST_ASSERT( p_target == view_data.plane_buffers_[ plane ] + ( view_data.stride_ * view_data.dimensions_.y ) );
            }
        }
        else
        {
            BOOST_ASSERT( ::TIFFScanlineSize( p_tiff_ ) <= static_cast<tsize_t>( view_data.stride_ ) );
            for ( unsigned int plane( 0 ); plane < view_data.number_of_planes_; ++plane )
            {
                unsigned char * buf( view_data.plane_buffers_[ plane ] );
                skip_row_results_t skip_result( skip_to_row( view_data.offset_, plane, buf, result ) );
                ttile_t const number_of_strips( ( view_data.dimensions_.y - skip_result.rows_to_read_using_scanlines + skip_result.rows_per_strip - 1 ) / skip_result.rows_per_strip );
                skip_result.rows_to_read_using_scanlines = std::min<unsigned int>( skip_result.rows_to_read_using_scanlines, view_data.dimensions_.y );

                unsigned int row( view_data.offset_ );
                while ( row != ( view_data.offset_ + skip_result.rows_to_read_using_scanlines ) )
                {
                    result.accumulate_equal( ::TIFFReadScanline( p_tiff_, buf, row++, static_cast<tsample_t>( plane ) ), 1 );
                    buf += view_data.stride_;
                }

                unsigned int const view_strip_increment( view_data.stride_ * skip_result.rows_per_strip );
                if ( view_strip_increment == static_cast<unsigned int>( ::TIFFStripSize( p_tiff_ ) ) )
                {
                    for ( unsigned int strip( skip_result.starting_strip ); strip < number_of_strips; ++strip )
                    {
                        result.accumulate_different( ::TIFFReadEncodedStrip( p_tiff_, strip, buf, view_strip_increment ), -1 );
                        buf += view_strip_increment;
                        row += skip_result.rows_per_strip;
                    }
                }

                unsigned int const target_row( view_data.offset_ + view_data.dimensions_.y );
                while ( row < target_row )
                {
                    result.accumulate_equal( ::TIFFReadScanline( p_tiff_, buf, row++, static_cast<tsample_t>( plane ) ), 1 );
                    buf += view_data.stride_;
                }
            }
        }

        result.throw_if_error();
    }

    template <class MyView, class TargetView, class Converter>
    void generic_convert_to_prepared_view( TargetView const & view, Converter const & converter ) const
    {
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

        bool const nondirect_planar_to_contig_conversion
        (
             is_planar<MyView                >::value &&
            !is_planar<original_target_view_t>::value &&
            (
                !is_same<typename color_space_type    <MyView>::type, typename color_space_type    <original_target_view_t>::type>::value ||
                !is_same<typename channel_mapping_type<MyView>::type, typename channel_mapping_type<original_target_view_t>::type>::value
            )
        );

        cumulative_result result;

        unsigned int const number_of_planes( is_planar<MyView>::value ? num_channels<MyView>::value : 1 );

        point2<std::ptrdiff_t> const & dimensions( original_view( view ).dimensions() );

        ////////////////////////////////////////////////////////////////////////
        // Tiled
        ////////////////////////////////////////////////////////////////////////

        if ( ::TIFFIsTiled( p_tiff_ ) )
        {
            tile_setup_t setup
            (
                *this,
                dimensions,
                get_offset<offset_t>( view ),
                nondirect_planar_to_contig_conversion
            );

            unsigned int const tiles_per_plane( setup.number_of_tiles / number_of_planes );
            ttile_t            current_tile   ( 0                                        );

            if ( nondirect_planar_to_contig_conversion )
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
                        mpl::int_<number_of_planes>()
                    )
                );

                unsigned int horizontal_offset( 0 );

                for ( current_tile += setup.starting_tile; current_tile < tiles_per_plane; ++current_tile )
                {
                    bool         const last_row_tile  ( !--setup.current_row_tiles_remaining                         );
                    unsigned int const this_tile_width( last_row_tile ? setup.last_row_tile_width : setup.tile_width );

                    for ( unsigned int channel_tile( 0 ); channel_tile < number_of_planes; ++channel_tile )
                    {
                        ttile_t const raw_tile_number( current_tile + ( channel_tile * tiles_per_plane ) );
                        result.accumulate_equal
                        (
                            ::TIFFReadEncodedTile
                            (
                                p_tiff_,
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
                    plane < number_of_planes;
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

                        result.accumulate_equal( ::TIFFReadEncodedTile( p_tiff_, current_tile, setup.p_tile_buffer.get(), setup.tile_size_bytes ), setup.tile_size_bytes );

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
            scanline_buffer_t<my_pixel_t> const scanline_buffer( *this, mpl::bool_<nondirect_planar_to_contig_conversion>() );

            if ( nondirect_planar_to_contig_conversion )
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
                        mpl::int_<number_of_planes>()
                    )
                );

                while ( row != target_row )
                {
                    for ( unsigned int plane( 0 ); plane < number_of_planes; ++plane )
                    {
                        tdata_t const p_buffer( &(*buffer_iterator)[ plane ] );
                        skip_to_row( get_offset<offset_t>( view ), plane, p_buffer, result );
                        result.accumulate_equal( ::TIFFReadScanline( p_tiff_, p_buffer, row, static_cast<tsample_t>( plane ) ), 1 );
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
                for ( unsigned int plane( 0 ); plane < number_of_planes; ++plane )
                {
                    skip_to_row( get_offset<offset_t>( view ), plane, scanline_buffer.begin(), result );

                    local_target_view_t const & target_view( adjust_target_to_my_view( original_view( view ), plane, is_planar<MyView>() ) );
                    target_y_iterator p_target( target_view.y_at( 0, 0 ) );
                    unsigned int       row       ( get_offset<offset_t>( view ) );
                    unsigned int const target_row( row + dimensions.y           );
                    while ( row != target_row )
                    {
                        result.accumulate_equal( ::TIFFReadScanline( p_tiff_, scanline_buffer.begin(), row++, static_cast<tsample_t>( plane ) ), 1 );
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

    typedef std::pair
        <
            scoped_array<unsigned char> const,
            unsigned char const * const
        > generic_scanline_buffer_t;

    static generic_scanline_buffer_t scanline_buffer_aux( TIFF & tiff )
    {
        unsigned int const scanlineSize( ::TIFFScanlineSize( &tiff ) );
        unsigned char * const p_buffer( new unsigned char[ scanlineSize ] );
        #ifdef _DEBUG
            std::memset( p_buffer, 0xFF, scanlineSize );
        #endif // _DEBUG
        return std::make_pair( p_buffer, p_buffer + scanlineSize );
    }

    // This one makes the end pointer point to the end of the scanline/row of
    // the first plane not the end of the buffer itself...ugh...to be cleaned up...
    static generic_scanline_buffer_t planar_scanline_buffer_aux( libtiff_image const & tiff )
    {
        unsigned int const scanlineSize( tiff.format_bits().bits_per_sample * tiff.dimensions().x / 8 );
        unsigned char * const p_buffer( new unsigned char[ scanlineSize * tiff.format_bits().samples_per_pixel ] );
        #ifdef _DEBUG
            std::memset( p_buffer, 0xFF, scanlineSize * tiff.format_bits().samples_per_pixel );
        #endif // _DEBUG
        return std::make_pair( p_buffer, p_buffer + scanlineSize );
    }

    template <typename Pixel>
    class scanline_buffer_t : noncopyable
    {
    public:
        scanline_buffer_t( libtiff_image const & tiff, mpl::true_ /*      nptcc  */ ) : buffer_( planar_scanline_buffer_aux( tiff          ) ) {}
        scanline_buffer_t( libtiff_image const & tiff, mpl::false_ /* not nptcc  */ ) : buffer_( scanline_buffer_aux       ( *tiff.p_tiff_ ) ) {} 

        Pixel       * begin() const { return gil_reinterpret_cast  <Pixel       *>( buffer_.first.get() ); }
        Pixel const * end  () const { return gil_reinterpret_cast_c<Pixel const *>( buffer_.second      ); }

    private:
        generic_scanline_buffer_t const buffer_;
    };

    template <typename Pixel>
    class planar_scanline_buffer_t : noncopyable
    {
    public:
        planar_scanline_buffer_t( libtiff_image & image ) : buffer_( planar_scanline_buffer_aux( tiff ) ) {}

        Pixel       * begin() const { return gil_reinterpret_cast  <Pixel       *>( buffer_.first.get() ); }
        Pixel const * end  () const { return gil_reinterpret_cast_c<Pixel const *>( buffer_.second      ); }

    private:
        generic_scanline_buffer_t const buffer_;
    };


    skip_row_results_t skip_to_row
    (
        unsigned int        const row_to_skip_to,
        unsigned int        const sample,
        tdata_t             const buffer,
        cumulative_result &       error_result
    ) const
    {
        BOOST_ASSERT( !::TIFFIsTiled( p_tiff_ ) );

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
            error_result.accumulate_equal( ::TIFFReadScanline( p_tiff_, buffer, row++, static_cast<tsample_t>( sample ) ), 1 );
        }

        //BOOST_ASSERT( !row_to_skip_to || ( ::TIFFCurrentRow( p_tiff_ ) == row_to_skip_to ) );

        return result;
    }

private:
    TIFF * /*const*/ p_tiff_;

    full_format_t format_;
};

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif




//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libjpeg_private_base_hpp
