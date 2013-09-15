////////////////////////////////////////////////////////////////////////////////
///
/// \file gp/writer.hpp
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
#ifndef writer_hpp__3B1ED5BC_42C6_4EC6_B700_01C1B8646431
#define writer_hpp__3B1ED5BC_42C6_4EC6_B700_01C1B8646431
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

class gp_writer
    :
    public gp_image,
    public detail::backend_writer<gp_image>,
    public open_on_write_writer
{
public:
    typedef std::pair
            <
                Gdiplus::GpBitmap          *,
                Gdiplus::EncoderParameters *
            > lib_object_t;

    typedef std::pair
            <
                lib_object_t::first_type  const,
                lib_object_t::second_type const
            > const_lib_object_t;

    // The passed View object must outlive the GpBitmap object (GDI+ uses lazy
    // evaluation).
    template <class View>
    explicit gp_writer( View & view )
        :
        p_encoder_parameters_( NULL )
    {
        BOOST_STATIC_ASSERT(( gp_is_supported<typename View::value_type, is_planar<View>::value>::value ));

        // http://msdn.microsoft.com/en-us/library/ms536315(VS.85).aspx
        // stride has to be a multiple of 4 bytes
        BOOST_ASSERT( !( view.pixels().row_size() % sizeof( Gdiplus::ARGB ) ) );

        ensure_result
        (
            Gdiplus::DllExports::GdipCreateBitmapFromScan0
            (
                view.width (),
                view.height(),
                view.pixels().row_size(),
                gil_to_gp_format<typename View::value_type, is_planar<View>::value>::value,
                backend_base::get_raw_data( view ),
                &gp_image::lib_object()
            )
        );
        BOOST_ASSERT( lib_object().first );
    }

    ~gp_writer()
    {
        detail::verify_result( Gdiplus::DllExports::GdipDisposeImage( lib_object().first ) );
    }

    template <typename Target>
    void write_default( Target const & target, format_tag const format )
    {
        BOOST_ASSERT( lib_object().second == NULL );
        write( target, format );
    }

    void write( char    const * const pFilename, format_tag const format ) const { write( detail::wide_path( pFilename ), format ); }
    void write( wchar_t const * const pFilename, format_tag const format ) const
    {
        detail::ensure_result
        (
            Gdiplus::DllExports::GdipSaveImageToFile
            (
                lib_object().first,
                pFilename,
                &encoder_id( format ),
                lib_object().second
            )
        );
    }

    lib_object_t       lib_object()       { return lib_object_t      ( &gp_image::lib_object(), p_encoder_parameters_ ); }
    const_lib_object_t lib_object() const { return const_lib_object_t( &gp_image::lib_object(), p_encoder_parameters_ ); }

private:
    static CLSID const & encoder_id( format_tag const format )
    {
        static CLSID const ids[ number_of_known_formats ] =
        {
            { 0x557CF400, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // BMP
            { 0x557CF402, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // GIF
            { 0x557CF401, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // JPEG
            { 0x557CF406, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // PNG
            { 0x557CF405, 0x1A04, 0x11D3, 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E }, // TIFF
            CLSID_NULL // TGA
        };
        BOOST_ASSERT( ids[ format ] != CLSID_NULL );
        return ids[ format ];
    }

private:
    Gdiplus::EncoderParameters * p_encoder_parameters_;
}; // class gp_writer

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // writer_hpp
