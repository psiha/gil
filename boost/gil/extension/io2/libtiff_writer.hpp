////////////////////////////////////////////////////////////////////////////////
///
/// \file libtiff_writer.hpp
/// ------------------------
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
#ifndef libtiff_writer_hpp__FD402B04_E934_4E73_8839_001A8633B5D3
#define libtiff_writer_hpp__FD402B04_E934_4E73_8839_001A8633B5D3
#pragma once
//------------------------------------------------------------------------------
#include "libtiff_image.hpp"
#include "backend_writer.hpp"

#include "detail/io_error.hpp"
#include "detail/libx_shared.hpp"
#include "detail/platform_specifics.hpp"
#include "detail/shared.hpp"

#include <boost/array.hpp>
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

inline tsize_t FILE_write_proc( thandle_t const handle, tdata_t const buf, tsize_t const size )
{
    return static_cast<tsize_t>( std::fwrite( buf, 1, size,  reinterpret_cast<FILE *>( handle ) ) );
}

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

//------------------------------------------------------------------------------
} // namespace detail

class libtiff_writer
    :
    public libtiff_image,
    public detail::backend_writer<libtiff_image>,
    public configure_on_write_writer
{
public:
    explicit libtiff_writer( char const * const file_name ) : libtiff_image( file_name, "w" ) {}

    explicit libtiff_writer( FILE & file ) : libtiff_image( file ) {}

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
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // libtiff_writer_hpp
