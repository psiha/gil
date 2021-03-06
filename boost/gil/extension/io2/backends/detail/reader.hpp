////////////////////////////////////////////////////////////////////////////////
///
/// \file reader.hpp
/// ----------------
///
/// Base CRTP class for backend readers.
///
/// Copyright (c) Domagoj Saric 2010.-2013.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef reader_hpp__3E2ACB3B_F45F_404B_A603_0396518B183C
#define reader_hpp__3E2ACB3B_F45F_404B_A603_0396518B183C
#pragma once
//------------------------------------------------------------------------------
#include "reader_for.hpp"

#include "boost/gil/extension/io2/format_tags.hpp"
#include "boost/gil/extension/io2/detail/platform_specifics.hpp"
#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/switch.hpp"

#include "boost/gil/planar_pixel_iterator.hpp"
#include "boost/gil/planar_pixel_reference.hpp"
#include "boost/gil/typedefs.hpp"

#include <boost/compressed_pair.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/size_t_fwd.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/decay.hpp>
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

struct assert_dimensions_match {};
struct ensure_dimensions_match {};
struct synchronize_dimensions  {};

struct assert_formats_match {};
struct ensure_formats_match {};
struct synchronize_formats  {};


////////////////////////////////////////////////////////////////////////////////
///
/// \class offset_view_t
/// \todo document properly...
///
////////////////////////////////////////////////////////////////////////////////

template <class View, typename Offset>
class offset_view_t
{
public:
    typedef View   view_t  ;
    typedef Offset offset_t;

public:
    offset_view_t( View const & view, Offset const & offset ) : view_( view ), offset_( offset ) {}

    typename View::point_t dimensions() const
    {
        return offset_dimensions( original_view().dimensions(), offset() );
    }

    View   const & original_view() const { return view_  ; }
    Offset const & offset       () const { return offset_; }

private:
    static typename View::point_t offset_dimensions
    (
        typename View::point_t                   view_dimensions,
        typename View::point_t::value_type const offset
    )
    {
        //return typename View::point_t( view_dimensions.x, view_dimensions.y + offset );
        view_dimensions.y += offset;
        return view_dimensions;
    }

    static typename View::point_t offset_dimensions
    (
        typename View::point_t         view_dimensions,
        typename View::point_t const & offset
    )
    {
        return view_dimensions += offset;
    }

private:
    View   const & view_  ;
    Offset         offset_;
}; // class offset_view_t


////////////////////////////////////////////////////////////////////////////////
/// \class backend_traits
/// ( forward declaration )
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
struct backend_traits;

namespace detail
{
//------------------------------------------------------------------------------

template <class View, typename Offset> View const & original_view( offset_view_t<View, Offset> const & offset_view ) { return offset_view.original_view(); }
template <class View                 > View const & original_view( View                        const & view        ) { return view;                        }

template <typename Offset, class View> Offset         get_offset( View                        const &             ) { return Offset();             }
template <typename Offset, class View> Offset const & get_offset( offset_view_t<View, Offset> const & offset_view ) { return offset_view.offset(); }

template <typename Offset> Offset         get_offset_x( Offset         const &        ) { return Offset(); }
template <typename Offset> Offset const & get_offset_x( point2<Offset> const & offset ) { return offset.x; }

template <typename Offset> Offset         get_offset_y( Offset         const & offset ) { return offset;   }
template <typename Offset> Offset const & get_offset_y( point2<Offset> const & offset ) { return offset.y; }


template <class NewView, class View, typename Offset>
offset_view_t<NewView, Offset> offset_new_view( NewView const & new_view, offset_view_t<View, Offset> const & offset_view )
{
    return offset_view_t<NewView, Offset>( new_view, offset_view.offset() );
}

template <class NewView, class View>
NewView const & offset_new_view( NewView const & new_view, View const & ) { return new_view; }


template <class View>
struct is_offset_view : mpl::false_ {};

template <class View, typename Offset>
struct is_offset_view<offset_view_t<View, Offset> > : mpl::true_ {};


template <typename View                 > struct get_original_view_t;
template <typename Locator              > struct get_original_view_t<image_view<Locator>         > { typedef image_view<Locator> type; };
template <typename View, typename Offset> struct get_original_view_t<offset_view_t<View, Offset> > { typedef View                type; };

class backend_reader_base
{
public:
    typedef point2<unsigned int> dimensions_t;

    ////////////////////////////////////////////////////////////////////////////
    //
    // image_type_id_t
    // -------------
    //
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Locally adjusts a view with a ROI/offset to prevent it exceeding
    /// the image dimensions when 'merged' with its offset. Done here so that
    /// backend classes do not need to handle this.
    /// \internal
    /// \throws nothing
    ///
    ////////////////////////////////////////////////////////////////////////////

    typedef unsigned int image_type_id_t;
    static image_type_id_t const unsupported_format = static_cast<image_type_id_t>( -1 );

protected:
    static bool dimensions_mismatch( dimensions_t const & mine, dimensions_t const & other ) { return mine != other; }
    template <class View>
    static bool dimensions_mismatch( dimensions_t const & mine, View         const & view  ) { return dimensions_mismatch( mine, view.dimensions() ); }

    template <class View, typename Offset>
    static bool dimensions_mismatch( dimensions_t const & mine, offset_view_t<View, Offset> const & offset_view )
    {
        // Implementation note:
        //   For offset target views all dimensions are allowed as they are
        // intended to load an image in steps so they must be allowed to be
        // smaller. They are also allowed to be bigger to allow GIL users to use
        // fixed 'sub' view, while loading an image in steps, whose size is not
        // an exact divisor of the source image dimensions.
        //   The exception is for backends that support only vertical 'ROIs'/
        // offsets, for those the horizontal dimensions must match.
        //                                    (19.09.2010.) (Domagoj Saric)
        //dimensions_t const other( offset_view.dimensions() );
        //return ( other.x < mine.x ) || ( other.y < mine.y );
        return is_pod<Offset>::value ? ( mine.x != offset_view.dimensions().x ) : false;
    }

    static void do_ensure_dimensions_match( dimensions_t const & mine, dimensions_t const & other )
    {
        io_error_if( dimensions_mismatch( mine, other ), "input view size does not match source image size" );
    }

    template <class View>
    static void do_ensure_dimensions_match( dimensions_t const & mine, View const & view )
    {
        do_ensure_dimensions_match( mine, view.dimensions() );
    }

    template <class View, typename Offset>
    static void do_ensure_dimensions_match( dimensions_t const & mine, offset_view_t<View, Offset> const & offset_view )
    {
        io_error_if( dimensions_mismatch( mine, offset_view ), "input view size does not match source image size" );
    }

    static void do_ensure_formats_match( bool const formats_mismatch )
    {
        io_error_if( formats_mismatch, "input view format does not match source image format" );
    }

    template <typename Image>
    static void do_synchronize_dimensions( Image & image, dimensions_t const & my_dimensions, unsigned int const alignment = 0 )
    {
        image.recreate( my_dimensions, alignment );
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    // subview_for_offset()
    // --------------------
    //
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Locally adjusts a view with a ROI/offset to prevent it exceeding
    /// the image dimensions when 'merged' with its offset. Done here so that
    /// backend classes do not need to handle this.
    /// \internal
    /// \throws nothing
    ///
    ////////////////////////////////////////////////////////////////////////////

    template <typename View>
    static View const & subview_for_offset( View const & view ) { return view; }

    template <typename View, typename Offset>
    static View subview_for_offset( dimensions_t const & my_dimensions, offset_view_t<View, Offset> const & offset_view )
    {
        dimensions_t const & target_dimensions( offset_view.original_view().dimensions() );

        bool const zero_x_offset( is_pod<Offset>::value );
        if ( zero_x_offset )
        {
            BOOST_ASSERT( get_offset_x( offset_view.offset() ) == 0 );
            BOOST_ASSERT( my_dimensions.x == target_dimensions.x    );
        }

        unsigned int const width ( zero_x_offset ? target_dimensions.x : std::min<unsigned int>( my_dimensions.x - get_offset_x( offset_view.offset() ), target_dimensions.x ) );
        unsigned int const height(                                       std::min<unsigned int>( my_dimensions.y - get_offset_y( offset_view.offset() ), target_dimensions.y ) );
        
        return subimage_view( offset_view.original_view(), 0, 0, width, height );
    }
}; // class backend_reader_base


////////////////////////////////////////////////////////////////////////////////
///
/// \class backend_reader
///
////////////////////////////////////////////////////////////////////////////////

template <class Backend>
class backend_reader : public backend_reader_base
{
protected:
    template <class View>
    struct has_supported_format
    {
    private:
        typedef typename get_original_view_t<View>::type original_view_t;

    public:
        typedef typename backend_traits<Backend>:: BOOST_NESTED_TEMPLATE is_supported
        <
            typename original_view_t::value_type,
            is_planar<original_view_t>::value
        > type;
        BOOST_STATIC_CONSTANT( bool, value = type::value );
    };

protected:
    typedef typename backend_traits<Backend>::format_t    format_t;
    typedef typename backend_traits<Backend>::view_data_t view_data_t;

private:
    typedef mpl::range_c<std::size_t, 0, mpl::size<typename Backend::supported_pixel_formats>::value> valid_type_id_range_t;

    struct image_id_finder
    {
        image_id_finder( format_t const format ) : format_( format ), image_id_( unsupported_format ) {}

        template <typename ImageIndex>
        void operator()( ImageIndex )
        {
            typedef typename mpl::at<typename Backend::supported_pixel_formats, ImageIndex>::type pixel_format_t;
            format_t const image_format( typename Backend:: template get_native_format<pixel_format_t>::value );
            if ( image_format == this->format_ )
            {
                BOOST_ASSERT_MSG( image_id_ == unsupported_format, "Image ID already found." );
                image_id_ = ImageIndex::value;
            }
        }

        format_t     const format_  ;
        unsigned int       image_id_;

    private:
        void operator=( image_id_finder const & );
    };

private:
    // MSVC++ (8,9,10) generates code to check whether this == 0.
    typedef typename Backend::native_reader Impl;
    Impl       & impl()       { BF_ASSUME( this != 0 ); return static_cast<Impl       &>( *this ); }
    Impl const & impl() const { BF_ASSUME( this != 0 ); return static_cast<Impl const &>( *this ); }

protected:
    template <typename View>
    bool dimensions_mismatch( View const & view ) const
    {
        return backend_base::dimensions_mismatch( impl().dimensions(), view );
    }

    bool dimensions_mismatch( dimensions_t const & other_dimensions ) const
    {
        return backend_base::dimensions_mismatch( impl().dimensions(), other_dimensions );
    }

    template <class View>
    void do_ensure_dimensions_match( View const & view ) const
    {
        backend_base::do_ensure_dimensions_match( impl().dimensions(), view );
    }

    template <typename View>
    bool formats_mismatch() const
    {
        return formats_mismatch( get_native_format<typename get_original_view_t<View>::type>::value );
    }

    bool formats_mismatch( format_t const other_format ) const
    {
        return ( other_format != impl().closest_gil_supported_format() ) != false;
    }

    template <class View>
    void do_ensure_formats_match() const { backend_base::do_ensure_formats_match( formats_mismatch<View>() ); }

    template <typename View>
    bool can_do_inplace_transform() const
    {
        return can_do_inplace_transform<View>( impl().format() );
    }

    template <typename View>
    bool can_do_inplace_transform( format_t const my_format ) const
    {
        return ( impl().cached_format_size( my_format ) == static_cast<std::size_t>( memunit_step( get_original_view_t<View>::type::x_iterator() ) ) );
    }

    template <typename View>
    static View const & subview_for_offset( View const & view ) { return view; }

    template <typename View, typename Offset>
    View subview_for_offset( offset_view_t<View, Offset> const & offset_view ) const
    {
        return backend_base::subview_for_offset( impl().dimensions(), offset_view );
    }

    template <typename View>
    static view_data_t get_view_data( View const & view )
    {
        return view_data_t( view );
    }

    template <typename View>
    view_data_t get_view_data( offset_view_t<View, typename Backend::offset_t> const & offset_view ) const
    {
        return view_data_t
        (
            subview_for_offset( offset_view ),
            offset_view.offset()
        );
    }

public: // Low-level (row, strip, tile) access
    // Generic implementations: impl classes are encouraged to provide more
    // efficient overrides.

    std::size_t pixel_size() const
    {
        return impl().cached_format_size( impl().format() );
    }

    std::size_t row_size() const
    {
        return impl().pixel_size() * impl().dimensions().x;
    }

    static image_type_id_t image_format_id( format_t const closest_gil_supported_format )
    {
        // This (linear search) will be transformed into a switch...
        image_id_finder finder( closest_gil_supported_format );
        mpl::for_each<valid_type_id_range_t>( ref( finder ) );
        BOOST_ASSERT( finder.image_id_ != unsupported_format );
        return finder.image_id_;
    }

public: // Views...
    template <typename View>
    void copy_to( View const & view, assert_dimensions_match, assert_formats_match ) const
    {
        BOOST_STATIC_ASSERT( get_original_view_t<View>::type::value_type::is_mutable );
        BOOST_STATIC_ASSERT( has_supported_format<View>::value                       );
        BOOST_ASSERT( !impl().dimensions_mismatch( view )                            );
        BOOST_ASSERT( !impl().formats_mismatch<View>()                               );
        impl().raw_copy_to_prepared_view( get_view_data( view ) );
    }

    template <typename View>
    void copy_to( View const & view, assert_dimensions_match, ensure_formats_match ) const
    {
        impl().do_ensure_formats_match<View>();
        impl().copy_to( view, assert_dimensions_match(), assert_formats_match() );
    }

    template <typename View>
    void copy_to( View const & view, ensure_dimensions_match, assert_formats_match ) const
    {
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), assert_formats_match() );
    }

    template <typename View>
    void copy_to( View const & view, ensure_dimensions_match, ensure_formats_match ) const
    {
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), ensure_formats_match() );
    }

    template <typename View>
    void copy_to( View const & view, ensure_dimensions_match, synchronize_formats ) const
    {
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), synchronize_formats() );
    }

    template <typename View>
    void copy_to( View const & view, assert_dimensions_match, synchronize_formats ) const
    {
        BOOST_ASSERT( !impl().dimensions_mismatch( view ) );
        typedef mpl::bool_
        <
            has_supported_format  <View   >::value &&
            backend_traits<Backend>::builtin_conversion
        > can_use_raw_t;
        default_convert_to_worker( view, can_use_raw_t() );
    }
    
    template <typename FormatConverter, typename View>
    void copy_to( View const & view, ensure_dimensions_match, FormatConverter & format_converter ) const
    {
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), format_converter );
    }

    template <typename FormatConverter, typename View>
    void copy_to( View const & view, assert_dimensions_match, FormatConverter & format_converter ) const
    {
        BOOST_ASSERT( !impl().dimensions_mismatch( view ) );
        impl().convert_to_prepared_view( view, format_converter );
    }

public: // Images...
    template <typename Image, typename FormatsPolicy>
    Image copy_to_image( FormatsPolicy const formats_policy ) const
    {
        Image image( impl().dimensions(), backend_traits<Backend>::desired_alignment );
        impl().copy_to( view( image ), assert_dimensions_match(), formats_policy );
        return image;
    }

    template <typename Image, typename DimensionsPolicy, typename FormatsPolicy>
    void copy_to_image( Image & image, DimensionsPolicy const dimensions_policy, FormatsPolicy const formats_policy ) const
    {
        impl().copy_to( view( image ), dimensions_policy, formats_policy );
    }

    template <typename Image, typename FormatsPolicy>
    void copy_to_image( Image & image, synchronize_dimensions, FormatsPolicy const formats_policy ) const
    {
        impl().do_synchronize_dimensions( image, impl().dimensions(), backend_traits<Backend>::desired_alignment );
        impl().copy_to( view( image ), assert_dimensions_match(), formats_policy );
    }

public: // Offset factory
    template <class View>
    static
    offset_view_t<View, typename Backend::offset_t>
    offset_view( View const & view, typename Backend::offset_t const offset )
    {
        return offset_view_t<View, offset_t>( view, offset );
    }

public: // Utility 'quick-wrappers'...
    template <class Source, class Image>
    static void read( Source const & target, Image & image )
    {
        typedef typename reader_for<typename decay<Source>::type>::type reader_t;
        // The backend does not know how to read from the specified source type.
        BOOST_STATIC_ASSERT(( !is_same<reader_t, mpl::void_>::value ));
        reader_t( target ).copy_to_image( image, synchronize_dimensions(), synchronize_formats() );
    }

    template <typename char_type, class View>
    static void read( std::basic_string<char_type> const & file_name, View const & view )
    {
        read( file_name.c_str(), view );
    }

    template <typename char_type, class View>
    static void read( std::basic_string<char_type>       & file_name, View const & view )
    {
        read( file_name.c_str(), view );
    }

private:
    template <class View, typename CC>
    class in_place_converter_t
    {
    public:
        typedef void result_type;

        in_place_converter_t( CC const & cc, View const & view ) : members_( cc, view ) {}

        template <std::size_t index>
        void operator()( mpl::size_t<index> ) const
        {
            typedef typename mpl::at_c<supported_pixel_formats, index>::type::view_t view_t;
            BOOST_ASSERT( is_planar<View>::value == is_planar<view_t>::value ); //zzz...make this a static assert...
            if ( is_planar<View>::value )
            {
                for ( unsigned int plane( 0 ); plane < num_channels<View>::value; ++plane )
                {
                    BOOST_ASSERT( sizeof( view_t ) == sizeof( View ) ); //zzz...make this a static assert...
                    for_each_pixel( nth_channel_view( *gil_reinterpret_cast_c<view_t const *>( &view() ), plane ), *this );
                }
            }
            else
            {
                BOOST_ASSERT( sizeof( view_t ) == sizeof( View ) ); //zzz...make this a static assert...
                for_each_pixel( *gil_reinterpret_cast_c<view_t const *>( &view() ), *this );
            }
        }

        template <typename SrcP>
        typename enable_if<is_pixel<SrcP> >::type
        operator()( SrcP & srcP )
        {
            convert_aux( srcP, is_planar<View>() );
        }

        void operator=( in_place_converter_t const & other )
        {
            BOOST_ASSERT( this->view() == other.view() );
            this->cc() = other.cc();
        }

    private:
        CC         & cc  ()       { return members_.first (); }
        CC   const & cc  () const { return members_.first (); }
        View const & view() const { return members_.second(); }

        template <typename SrcP>
        void convert_aux( SrcP & srcP, mpl::true_ /*is planar*/ )
        {
            typedef typename nth_channel_view_type<View>::type::value_type DstP;
            BOOST_ASSERT( sizeof( SrcP ) == sizeof( DstP ) ); //zzz...make this a static assert...
            cc()( srcP, *const_cast<DstP *>( gil_reinterpret_cast_c<DstP const *>( &srcP ) ) );
        }

        template <typename SrcP>
        void convert_aux( SrcP & srcP, mpl::false_ /*is not planar*/ )
        {
            typedef typename View::value_type DstP;
            BOOST_ASSERT( sizeof( SrcP ) == sizeof( DstP ) ); //zzz...make this a static assert...
            cc()( srcP, *const_cast<DstP *>( gil_reinterpret_cast_c<DstP const *>( &srcP ) ) );
        }

    private:
        compressed_pair<CC, View const &> members_;
    };

    template <class View, typename CC>
    class generic_converter_t
    {
    public:
        typedef void result_type;

        generic_converter_t( Impl const & impl, CC const & cc, View const & view )
            : impl_( impl ), cc_view_( cc, view ) {}

        template <std::size_t index>
        void operator()( mpl::size_t<index> ) const
        {
            typedef typename mpl::at_c<supported_pixel_formats, index>::type::view_t my_view_t;
            impl_. BOOST_NESTED_TEMPLATE generic_convert_to_prepared_view<my_view_t>( view(), cc() );
        }

        void operator=( generic_converter_t const & other )
        {
            BOOST_ASSERT( this->impl_ == other.impl_ );
            BOOST_ASSERT( this->view_ == other.view_ );
            this->cc() = other.cc();
        }

    private:
        CC         & cc  ()       { return cc_view_.first (); }
        CC   const & cc  () const { return cc_view_.first (); }
        View const & view() const { return cc_view_.second(); }

    private:
        Impl                              const & impl_   ;
        compressed_pair<CC, View const &>         cc_view_;
    };

    template <class TargetView, class CC>
    static void in_place_transform( unsigned int const source_view_type_id, TargetView const & view, CC const & cc )
    {
        return switch_<valid_type_id_range_t>
        (
            source_view_type_id,
            in_place_converter_t<TargetView, CC>( cc, view ),
            assert_default_case_not_reached<void>()
        );
    }

    template <class TargetView, class CC>
    void generic_transform( unsigned int const source_view_type_id, TargetView const & view, CC const & cc ) const
    {
        return switch_<valid_type_id_range_t>
        (
            source_view_type_id,
            generic_converter_t<TargetView, CC>
            (
                impl(),
                cc,
                offset_new_view
                (
                    // See the note for backend_base::subview_for_offset()...
                    subview_for_offset( view ),
                    view
                )
            ),
            assert_default_case_not_reached<void>()
        );
    }

    template <typename View, typename CC>
    void convert_to_prepared_view( View const & view, CC const & converter ) const
    {
        BOOST_ASSERT( !dimensions_mismatch( view ) );
        convert_to_prepared_view_worker
        (
            view,
            converter,
            mpl::bool_
            <
                is_plain_in_memory_view<typename get_original_view_t<View>::type>::value &&
                has_supported_format   <                             View       >::value
            >()
        );
    }

    template <typename View, typename CC>
    void convert_to_prepared_view_worker( View const & view, CC const & converter, mpl::true_ /*can use raw*/ ) const
    {
        format_t     const current_format         ( impl().closest_gil_supported_format()    );
        unsigned int const current_image_format_id( impl().image_format_id( current_format ) );
        if ( can_do_inplace_transform<View>( current_format ) )
        {
            view_data_t view_data( get_view_data( view ) );
            if ( backend_traits<Backend>::builtin_conversion )
                view_data.set_format( current_format );
            else
                BOOST_ASSERT( current_format == impl().format() );
            impl().raw_copy_to_prepared_view( view_data );
            in_place_transform( current_image_format_id, original_view( view ), converter );
        }
        else
        {
            generic_transform( current_image_format_id, view, converter );
        }
    }

    template <typename View, typename CC>
    void convert_to_prepared_view_worker( View const & view, CC const & converter, mpl::false_ /*must use generic*/ ) const
    {
        generic_transform( Impl::image_format_id( impl().closest_gil_supported_format() ), view, converter );
    }

    template <typename View>
    void default_convert_to_worker( View const & view, mpl::true_ /*can use raw*/ ) const
    {
        impl().raw_convert_to_prepared_view( get_view_data( view ) );
    }

    template <typename View>
    void default_convert_to_worker( View const & view, mpl::false_ /*cannot use raw*/ ) const
    {
        impl().convert_to_prepared_view( view, default_color_converter() );
    }
};


//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // reader_hpp
