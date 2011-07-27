////////////////////////////////////////////////////////////////////////////////
///
/// \file reader.hpp
/// ----------------
///
/// LibTIFF reader
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
#ifndef reader_hpp__24E9D122_DAC7_415B_8880_9BFA03916E9F
#define reader_hpp__24E9D122_DAC7_415B_8880_9BFA03916E9F
#pragma once
//------------------------------------------------------------------------------
#include "backend.hpp"
#include "../detail/reader.hpp"

#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/libx_shared.hpp"
#include "boost/gil/extension/io2/detail/platform_specifics.hpp"
#include "boost/gil/extension/io2/detail/shared.hpp"

#include "boost/gil/image_view_factory.hpp"

#include <boost/array.hpp>
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
namespace io
{
//------------------------------------------------------------------------------
namespace detail
{
//------------------------------------------------------------------------------

template <typename Handle>
tsize_t read( thandle_t const handle, tdata_t const buf, tsize_t const size )
{
    return static_cast<tsize_t>( input_device<Handle>::read( buf, size, reinterpret_cast<Handle>( handle ) ) );
}

template <typename Handle>
int map( thandle_t /*handle*/, tdata_t * /*pbase*/, toff_t * /*psize*/ )
{
    return false;
}

template <typename Handle>
void unmap( thandle_t /*handle*/, tdata_t /*base*/, toff_t /*size*/ )
{
}

//------------------------------------------------------------------------------
} // namespace detail


class libtiff_image::native_reader
    :
    public libtiff_image,
    public detail::backend_reader<libtiff_image>
{
public: /// \ingroup Construction
    explicit native_reader( char const * const file_name )
        :
        libtiff_image( file_name, "r" ),
        format_      ( get_format()   )
    {}

    template <typename DeviceHandle>
    explicit native_reader( DeviceHandle const handle )
        :
        libtiff_image
        (
            handle,
            &input_device<DeviceHandle>::read,
            NULL,
            NULL,
            NULL
        ),
        format_( get_format() )
    {}

public:
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

    class sequential_row_read_state
        :
        private libtiff_image::cumulative_result
    {
    public:
        using libtiff_image::cumulative_result::failed;
        using libtiff_image::cumulative_result::throw_if_error;

        BOOST_STATIC_CONSTANT( bool, throws_on_error = false );

    private: friend class libtiff_image::native_reader;
        sequential_row_read_state() : position_( 0 ) {}

        unsigned int position_;
    };


    static sequential_row_read_state begin_sequential_row_read() { return sequential_row_read_state(); }

    void read_row( sequential_row_read_state & state, unsigned char * const p_row_storage, unsigned int const plane = 0 ) const
    {
        state.accumulate_greater
        (
            ::TIFFReadScanline( &lib_object(), p_row_storage, state.position_++, static_cast<tsample_t>( plane ) ),
            0
        );
    }


    typedef sequential_row_read_state sequential_tile_read_state;

    static sequential_tile_read_state begin_sequential_tile_access() { return begin_sequential_row_read(); }

    void read_tile( sequential_row_read_state & state, unsigned char * const p_tile_storage ) const
    {
        state.accumulate_greater
        (
            ::TIFFReadEncodedTile( &lib_object(), state.position_++, p_tile_storage, -1 ),
            0
        );
    }

private:
    full_format_t get_format()
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

        full_format_t const result =
        {
            LIBTIFF_FORMAT( samples_per_pixel, bits_per_sample, ( sample_format == SAMPLEFORMAT_VOID ) ? SAMPLEFORMAT_UINT : sample_format, planar_configuration, photometric, ink_set )
        };
        return result;
    }

private:
    detail::full_format_t::format_bitfield const & format_bits() const { return format_.bits; }

    static unsigned int round_up_divide( unsigned int const dividend, unsigned int const divisor )
    {
        return ( dividend + divisor - 1 ) / divisor;
    }

private: // Private backend_base interface.
    // Implementation note:
    //   MSVC 10 accepts friend base_t and friend class base_t, Clang 2.8
    // accepts friend class base_t, Apple Clang 1.6 and GCC (4.2 and 4.6) accept
    // neither.
    //                                        (13.01.2011.) (Domagoj Saric)
    friend class detail::backend<libtiff_image::native_reader>;

    struct tile_setup_t
        #ifndef __GNUC__
            : boost::noncopyable
        #endif // __GNUC__
    {
        tile_setup_t( native_reader const & source, point2<std::ptrdiff_t> const & dimensions, offset_t const offset, bool const nptcc )
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
    /// \todo Cleanup, document, split up and refactor this humongous monster
    /// (remove duplication, extract non-template and shared functionality...).
    ///                                    (16.09.2010.) (Domagoj Saric)
    ///
    ////////////////////////////////////////////////////////////////////////////

    #if defined(BOOST_MSVC)
    #   pragma warning( push )
    #   pragma warning( disable : 4127 ) // "conditional expression is constant"
    #endif

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

    #if defined(BOOST_MSVC)
    #   pragma warning( pop )
    #endif

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
    //   Because of plain RVO incapable compilers (like Clang or even the latest
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

        static std::pair<std::size_t, std::size_t> planar_scanline_buffer_construction( native_reader const & tiff )
        {
            full_format_t::format_bitfield const format_bits( tiff.format_bits() );
            std::size_t const scanline_size  ( format_bits.bits_per_sample * tiff.dimensions().x / 8 );
            std::size_t const allocation_size( scanline_size * format_bits.samples_per_pixel         );
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
    full_format_t const format_;
};

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // reader_hpp
