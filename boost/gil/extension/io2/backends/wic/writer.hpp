////////////////////////////////////////////////////////////////////////////////
///
/// \file writer.hpp
/// ----------------
///
/// WIC writer.
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
#ifndef writer_hpp__F8E533DB_13F3_495F_9F64_0C33F7C5AB95
#define writer_hpp__F8E533DB_13F3_495F_9F64_0C33F7C5AB95
#pragma once
//------------------------------------------------------------------------------
#include "backend.hpp"
#include "../detail/writer.hpp"

#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/windows_shared.hpp"
#include "boost/gil/extension/io2/detail/windows_shared_istreams.hpp"
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

class wic_image::native_writer
    :
    public wic_image,
    public detail::backend_writer<wic_image>,
    public detail::configure_on_write_writer
{
public:
    struct lib_object_t
    {
        detail::com_scoped_ptr<IWICBitmapFrameEncode>   p_frame_           ;
        detail::com_scoped_ptr<IWICBitmapEncoder    >   p_encoder_         ;
        IPropertyBag2                                 * p_frame_parameters_;
    };

    lib_object_t & lib_object() { return lib_object_; }

public:
    native_writer( IStream & target, format_tag const format )
    {
        create_encoder( target, encoder_guid( format ) );
    }

    void write_default( detail::wic_view_data_t const & view_data )
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

        detail::ensure_result( hr );
    }

    void write( detail::wic_view_data_t const & view_data ) { write_default( view_data ); }

private:
    void create_encoder( IStream & target, GUID const & format )
    {
        using namespace detail;
        HRESULT hr;
        {
            hr = wic_factory::singleton().CreateEncoder( format, NULL, &lib_object().p_encoder_ );
        }
        if ( SUCCEEDED( hr ) )
        {
            hr = encoder().Initialize( &target, WICBitmapEncoderNoCache );
        }
        if ( SUCCEEDED( hr ) )
        {
            hr = encoder().CreateNewFrame( &lib_object().p_frame_, &lib_object().p_frame_parameters_ );
        }
        ensure_result( hr );
    }

    static GUID const & encoder_guid( format_tag const format )
    {
        static GUID const * const guids[ number_of_known_formats ] =
        {
            &GUID_ContainerFormatBmp,
            &GUID_ContainerFormatGif,
            &GUID_ContainerFormatJpeg,
            &GUID_ContainerFormatPng,
            &GUID_ContainerFormatTiff,
            NULL // TGA
        };
        BOOST_ASSERT_MSG( guids[ format ] != NULL, "File format not supported by WIC." );
        return *guids[ format ];
    }

    IWICBitmapFrameEncode & frame  () { return *lib_object().p_frame_  ; }
    IWICBitmapEncoder     & encoder() { return *lib_object().p_encoder_; }
    
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
#endif // writer_hpp
