////////////////////////////////////////////////////////////////////////////////
///
/// \file formatted_image.hpp
/// -------------------------
///
/// (to be) Base CRTP class for all image implementation classes/backends.
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
#ifndef formatted_image_hpp__C34C1FB0_A4F5_42F3_9318_5805B88CFE49
#define formatted_image_hpp__C34C1FB0_A4F5_42F3_9318_5805B88CFE49
//------------------------------------------------------------------------------
#include "../../gil_all.hpp"
#include "io_error.hpp"

#include "detail/switch.hpp"

#include <boost/gil/extension/dynamic_image/any_image.hpp>

#ifdef _DEBUG
#include <boost/detail/endian.hpp>
#endif // _DEBUG
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/integral_c.hpp>
#ifdef _DEBUG
#include <boost/numeric/conversion/converter.hpp>
#include <boost/numeric/conversion/converter_policies.hpp>
#endif // _DEBUG
#include <boost/range/iterator_range.hpp>
#include <boost/static_assert.hpp>
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------

struct assert_dimensions_match {};
struct ensure_dimensions_match {};
struct synchronize_dimensions  {};

struct assert_formats_match {};
struct ensure_formats_match {};
struct synchronize_formats  {};


//...zzz...test unrolled 24bit-16bit conversion
//template <typename ChannelValue, typename Layout, typename BitField, typename ChannelRefVec>
//void color_convert
//(
//    pixel       <ChannelValue, Layout>            const & src,
//    packed_pixel<BitField, ChannelRefVec, Layout>       & dst
//)
//{
//    struct bfield_t
//    {
//        BitField r : 5;
//        BitField g : 6;
//        BitField b : 5;
//    };
//    //BitField & fast_dst( reinterpret_cast<BitField &>( dst ) );
//    bfield_t & fast_dst( reinterpret_cast<bfield_t &>( dst._bitfield ) );
//
//    unsigned char const source_max     ( detail::unsigned_integral_max_value<ChannelValue>::value );
//    //unsigned char const rb_max( (1<<5)-1 );
//    //unsigned char const g_max( (1<<6)-1 );
//    unsigned int const rtmp( ( static_cast<unsigned short>( src[ 0 ] ) << 5 ) - src[ 0 ] );
//    unsigned int const btmp( ( static_cast<unsigned short>( src[ 2 ] ) << 5 ) - src[ 2 ] );
//    unsigned int const gtmp( ( static_cast<unsigned short>( src[ 1 ] ) << 6 ) - src[ 1 ] );
//
//    fast_dst.r = ( rtmp / source_max ) + ( ( rtmp % source_max ) > ( source_max / 2 ) );
//    fast_dst.b = ( btmp / source_max ) + ( ( btmp % source_max ) > ( source_max / 2 ) );
//    fast_dst.g = ( gtmp / source_max ) + ( ( gtmp % source_max ) > ( source_max / 2 ) );
//}


#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

// An integer version of the unsigned channel downsampling/quantizing 'overload'...
template <typename SrcChannelV, int DestPackedNumBits>
struct channel_converter<SrcChannelV, packed_channel_value<DestPackedNumBits> >
    : public std::unary_function<SrcChannelV, packed_channel_value<DestPackedNumBits> >
{
    packed_channel_value<DestPackedNumBits> operator()( SrcChannelV const src ) const
    {
        typedef packed_channel_value<DestPackedNumBits> DstChannelV;

        typedef detail::channel_convert_to_unsigned  <SrcChannelV> to_unsigned;
        typedef detail::channel_convert_from_unsigned<DstChannelV> from_unsigned;

        if ( is_unsigned<SrcChannelV>::value )
        {
            SrcChannelV  const source_max( detail::unsigned_integral_max_value<SrcChannelV>::value );
            unsigned int const tmp       ( ( static_cast<unsigned short>( src ) << DestPackedNumBits ) - src );
            typename DstChannelV::integer_t const result
            (
                static_cast<typename DstChannelV::integer_t>( tmp / source_max ) + ( ( tmp % source_max ) > ( source_max / 2 ) )
            );
            #ifdef _DEBUG
            unsigned char const destination_max( detail::unsigned_integral_max_value<DstChannelV>::value );
            DstChannelV const debug_result
            (
                numeric::converter<unsigned, double, numeric::conversion_traits<unsigned, double>, numeric::def_overflow_handler, numeric::RoundEven<double> >::convert
                (
                    src * double( destination_max ) / double( source_max )
                )
            );
            BOOST_ASSERT( result == debug_result );
            #endif // _DEBUG
            return *gil_reinterpret_cast_c<DstChannelV const *>( &result );
        }
        else
        {
            typedef channel_converter_unsigned<typename to_unsigned::result_type, typename from_unsigned::argument_type> converter_unsigned;
            return from_unsigned()(converter_unsigned()(to_unsigned()(src))); 
        }
    }
};

#if defined(BOOST_MSVC)
#   pragma warning( pop )
#endif


namespace detail
{
//------------------------------------------------------------------------------

typedef iterator_range<TCHAR const *> string_chunk_t;


////////////////////////////////////////////////////////////////////////////////
///
/// \class formatted_image_base
///
////////////////////////////////////////////////////////////////////////////////

class formatted_image_base
{
public:
    typedef point2<std::ptrdiff_t> dimensions_t;

    typedef unsigned int image_type_id;
    static image_type_id const unsupported_format = static_cast<image_type_id>( -1 );

protected:
    static bool dimensions_mismatch( dimensions_t const & mine, dimensions_t const & other ) { return mine != other; }

    template <class View>
    static bool dimensions_mismatch( dimensions_t const & mine, View         const & view  )
    {
        return dimensions_mismatch( mine, view.dimensions() );
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

    static void do_ensure_formats_match( bool const formats_mismatch )
    {
        io_error_if( formats_mismatch, "input view format does not match source image format" );
    }

    template <typename Image>
    static void do_synchronize_dimensions( Image & image, dimensions_t const & my_dimensions, unsigned int const alignment = 0 )
    {
        image.recreate( my_dimensions, alignment );
    }

public: //...zzz...
    template <typename View>
    static unsigned char * get_raw_data( View const & view )
    {
        // A private implementation of interleaved_view_get_raw_data() that
        // works with packed pixel views.
        BOOST_STATIC_ASSERT(( !is_planar<View>::value /*&& view_is_basic<View>::value*/ ));
        BOOST_STATIC_ASSERT(( is_pointer<typename View::x_iterator>::value ));

        return gil_reinterpret_cast<unsigned char *>( &gil::at_c<0>( view( 0, 0 ) ) );
    }

protected:
    template <typename View>
    struct is_plain_in_memory_view
        :
        mpl::bool_
        <
            is_pointer<typename View::x_iterator>::value ||
            ( is_planar<View>::value && view_is_basic<View>::value )
        > {};

    struct assert_type_mismatch
    {
        typedef bool result_type;
        template <typename Index>
        result_type operator()( Index const & ) const { BOOST_ASSERT( !"input view format does not match source image format" ); return false; }
    };

    struct throw_type_mismatch
    {
        typedef void result_type;
        result_type operator()() const { do_ensure_formats_match( true ); }
    };

    template <typename Type, typename SupportedPixelFormats>
    struct check_type_match
    {
        typedef bool result_type;
        template <typename SupportedFormatIndex>
        result_type operator()( SupportedFormatIndex const & ) const
        {
            return is_same<Type, typename mpl::at<SupportedPixelFormats, SupportedFormatIndex>::type>::value;
        }
    };

    template <typename ResultType>
    struct assert_default_case_not_reached
    {
        typedef ResultType result_type;
        template <typename Index>
        result_type operator()( Index const & ) const
        {
            BOOST_ASSERT( !"Default case must not have been reached!" );
            __assume( false );
            return result_type();
        }
    };

    template <typename Images>
    class make_dynamic_image
    {
    public:
        make_dynamic_image( any_image<Images> & image ) : image_( image ) {}

        template <class Image>
        Image & apply()
        {
            image_.move_in( Image() );
            return image_._dynamic_cast<Image>();
        }

    protected:
        any_image<Images> & image_;
    };

};


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
    offset_view_t( View const & view, Offset const & offset ) : view_( view ), offset_( offset ) {}

    typename View::point_t dimensions() const
    {
        return offset_dimensions( offset_ );
    }

    View   const & original_view() const { return view_  ; }
    Offset const & offset       () const { return offset_; }

private:
    typename View::point_t offset_dimensions( typename View::point_t::value_type const offset ) const
    {
        typename View::point_t view_dimensions( view_.dimensions() );
        return view_dimensions += typename View::point_t( 0, offset );
    }

    typename View::point_t  offset_dimensions( typename View::point_t const & offset ) const
    {
        typename View::point_t view_dimensions( view_.dimensions() );
        return view_dimensions += offset;
    }

private:
    View                                     const & view_  ;
    typename call_traits<Offset>::param_type         offset_;
};

template <class View, typename Offset>
View const & original_view( offset_view_t<View, Offset> const & offset_view ) { return offset_view.original_view(); }

template <class View>
View const & original_view( View const & view ) { return view; }

template <typename Offset, class View>
Offset const & get_offset( offset_view_t<View, Offset> const & offset_view ) { return offset_view.offset(); }

template <typename Offset, class View>
Offset get_offset( View const & ) { return Offset(); }


template <class NewView, class View, typename Offset>
offset_view_t<NewView, Offset> offset_new_view( NewView const & new_view, offset_view_t<View, Offset> const & offset_view )
{
    return offset_view_t<NewView, Offset>( new_view, offset_view.offset_ );
}

template <class NewView, class View>
NewView const & offset_new_view( NewView const & new_view, View const & ) { return new_view; }


////////////////////////////////////////////////////////////////////////////////
/// \class formatted_image_traits
/// ( forward declaration )
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
struct formatted_image_traits;


////////////////////////////////////////////////////////////////////////////////
///
/// \class formatted_image
///
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
class formatted_image : public formatted_image_base
{
public:
    typedef typename formatted_image_traits<Impl>::format_t format_t;

    typedef typename formatted_image_traits<Impl>::supported_pixel_formats_t supported_pixel_formats;
    typedef typename formatted_image_traits<Impl>::roi_t                     roi;
    typedef typename roi::offset_t                                           offset_t;

    typedef any_image<supported_pixel_formats>     dynamic_image_t;
    typedef typename dynamic_image_t::const_view_t const_view_t;
    typedef typename dynamic_image_t::      view_t       view_t;

    BOOST_STATIC_CONSTANT( bool, has_full_roi = (is_same<roi::offset_t, roi::point_t>::value) );

protected:
    typedef formatted_image base_t;

private:
    template <typename Images, typename dimensions_policy, typename formats_policy>
    class read_dynamic_image : make_dynamic_image<Images>
    {
    private:
        typedef make_dynamic_image<Images> base;
    public:
        typedef void result_type;

        read_dynamic_image( any_image<Images> & image, Impl & impl )
            :
            base ( image ),
            impl_( impl  )
            {}

        template <class Image>
        void apply() { impl_.copy_to( base::apply<Image>(), dimensions_policy(), formats_policy() ); }

        template <typename SupportedFormatIndex>
        void operator()( SupportedFormatIndex const & ) { apply<typename mpl::at<SupportedFormatIndex>::type>(); }

    private:
        Impl & impl_;
    };

    template <typename dimensions_policy, typename formats_policy>
    class write_dynamic_view
    {
    public:
        typedef void result_type;

        write_dynamic_view( Impl & impl ) : impl_( impl  ) {}

        template <class View>
        void apply( View const & view ) { impl_.copy_from( view, dimensions_policy(), formats_policy() ); }

    private:
        Impl & impl_;
    };

    struct write_is_supported
    {
        template<typename View>
        struct apply : public Impl::is_supported<View> {};
    };

    typedef mpl::range_c<std::size_t, 0, mpl::size<supported_pixel_formats>::value> valid_type_id_range_t;

private:
    Impl       & impl()       { return static_cast<Impl       &>( *this ); }
    Impl const & impl() const { return static_cast<Impl const &>( *this ); }

protected:
    template <typename View>
    bool dimensions_mismatch( View const & view ) const
    {
        return dimensions_mismatch( view.dimensions() );
    }

    bool dimensions_mismatch( dimensions_t const & other_dimensions ) const
    {
        return formatted_image_base::dimensions_mismatch( impl().dimensions(), other_dimensions );
    }

    template <class View>
    void do_ensure_dimensions_match( View const & view ) const
    {
        formatted_image_base::do_ensure_dimensions_match( impl().dimensions(), view.dimensions() );
    }

    template <typename View>
    bool formats_mismatch() const
    {
        return formats_mismatch( formatted_image_traits<Impl>::view_to_native_format::apply<View>::value );
    }

    bool formats_mismatch( typename formatted_image_traits<Impl>::format_t const other_format ) const
    {
        return other_format != impl().closest_gil_supported_format();
    }

    template <class View>
    void do_ensure_formats_match() const { formatted_image_base::do_ensure_formats_match( formats_mismatch<View>() ); }

    template <typename View>
    bool can_do_inplace_transform() const
    {
        return can_do_inplace_transform<View>( format() );
    }

    template <typename View>
    bool can_do_inplace_transform( typename formatted_image_traits<Impl>::format_t const my_format ) const
    {
        return ( Impl::format_size( my_format ) == sizeof( typename View::value_type ) );
    }

public: // Views...
    template <typename View>
    void copy_to( View & view, assert_dimensions_match, assert_formats_match ) const
    {
        BOOST_STATIC_ASSERT( formatted_image_traits<Impl>::is_supported<View>::value );
        BOOST_ASSERT( !impl().dimensions_mismatch( view ) );
        BOOST_ASSERT( !impl().formats_mismatch<View>()    );
        impl().raw_convert_to_prepared_view
        (
            formatted_image_traits<Impl>::view_data_t
            (
                original_view       ( view ),
                get_offset<offset_t>( view )
            )
        );
    }

    template <typename View>
    void copy_to( View & view, assert_dimensions_match, ensure_formats_match ) const
    {
        impl().do_ensure_formats_match<View>();
        impl().copy_to( view, assert_dimensions_match(), assert_formats_match() );
    }

    template <typename View>
    void copy_to( View & view, ensure_dimensions_match, assert_formats_match ) const
    {
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), assert_formats_match() );
    }

    template <typename View>
    void copy_to( View & view, ensure_dimensions_match, ensure_formats_match ) const
    {
        impl().do_ensure_formats_match<View>();
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), ensure_formats_match() );
    }

    template <typename View>
    void copy_to( View & view, ensure_dimensions_match, synchronize_formats ) const
    {
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), synchronize_formats() );
    }

    template <typename View>
    void copy_to( View & view, assert_dimensions_match, synchronize_formats ) const
    {
        BOOST_ASSERT( !impl().dimensions_mismatch( view ) );
        impl().raw_convert_to_prepared_view
        (
            formatted_image_traits<Impl>::view_data_t
            (
                original_view       ( view ),
                get_offset<offset_t>( view )
            )
        );
    }

    template <typename FormatConverter, typename View>
    void copy_to( View & view, ensure_dimensions_match, FormatConverter const & format_converter ) const
    {
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), format_converter );
    }

    template <typename FormatConverter, typename View>
    void copy_to( View & view, assert_dimensions_match, FormatConverter const & format_converter ) const
    {
        BOOST_ASSERT( !impl().dimensions_mismatch( view ) );
        impl().convert_to_prepared_view( view, format_converter );
    }

public: // Images...
    template <typename Image, typename FormatsPolicy>
    Image copy_to_image( FormatsPolicy const formats_policy ) const
    {
        Image image( impl().dimensions(), formatted_image_traits<Impl>::desired_alignment );
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
        impl().do_synchronize_dimensions( image, impl().dimensions(), formatted_image_traits<Impl>::desired_alignment );
        impl().copy_to( view( image ), assert_dimensions_match(), formats_policy );
    }

    template <typename Images, typename dimensions_policy, typename formats_policy>
    void copy_to_image( any_image<Images> & im ) const
    {
        typedef mpl::range_c<std::size_t, 0, typename Impl::supported_pixel_formats> valid_range_t;
        switch_<valid_range_t>
        (
            impl().current_image_format_id(),
            read_dynamic_image<Images, dimensions_policy, formats_policy>( im, *this ),
            throw_type_mismatch()
        );
    }

    template <typename Views, typename dimensions_policy, typename formats_policy>
    void copy_from( any_image_view<Views> const & runtime_view, dimensions_policy, formats_policy )
    {
        typedef write_dynamic_view<dimensions_policy, formats_policy> op_t;
        op_t op( *this );
        apply_operation
        (
            runtime_view,
            dynamic_io_fnobj<write_is_supported, op_t>( &op )
        );
    }

private:
    template <class View, typename CC>
    struct in_place_converter_t
    {
        typedef void result_type;

        in_place_converter_t( CC const & cc, View const & view ) : cc_( cc ), view_( view ) {}

        template <std::size_t index>
        void operator()( mpl::integral_c<std::size_t, index> const & ) const
        {
            typedef typename mpl::at_c<supported_pixel_formats, index>::type::view_t view_t;
            BOOST_STATIC_ASSERT( sizeof( view_t ) == sizeof( View ) );
            for_each_pixel( *gil_reinterpret_cast_c<view_t const *>( &view_ ), *this );
        }

        template <typename SrcP>
        typename enable_if<is_pixel<SrcP>>::type
        operator()( SrcP & srcP )
        {
            BOOST_ASSERT( sizeof( SrcP ) == sizeof( typename View::value_type ) );
            cc_( srcP, *gil_reinterpret_cast<typename View::value_type *>( &srcP ) );
        }

        void operator=( in_place_converter_t const & other )
        {
            BOOST_ASSERT( this->view_ == other.view_ );
            this->cc_ = other.cc_;
        }

        CC cc_;
        View const & view_;
    };

    template <class View, typename CC>
    struct generic_converter_t
    {
        typedef void result_type;

        generic_converter_t( Impl const & impl, CC const & cc, View const & view )
            : impl_( impl ), cc_( cc ), view_( view ) {}

        template <std::size_t index>
        void operator()( mpl::integral_c<std::size_t, index> const & ) const
        {
            typedef typename mpl::at_c<supported_pixel_formats, index>::type::view_t my_view_t;
            impl_.generic_convert_to_prepared_view<my_view_t>( view_, cc_ );
        }

        void operator=( generic_converter_t const & other )
        {
            BOOST_ASSERT( this->impl_ == other.impl_ );
            BOOST_ASSERT( this->view_ == other.view_ );
            this->cc_ = other.cc_;
        }

        Impl const & impl_;
        CC           cc_  ;
        View const & view_;
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
            generic_converter_t<TargetView, CC>( impl(), cc, view ),
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
                is_plain_in_memory_view<View>::value &&
                formatted_image_traits<Impl>::is_supported<View>::value
            >()
        );
    }

    template <typename View, typename CC>
    void convert_to_prepared_view_worker( View const & view, CC const & converter, mpl::true_ /*can use raw*/ ) const
    {
        format_t     const my_format              ( impl().closest_gil_supported_format() );
        unsigned int const current_image_format_id( impl().image_format_id( my_format )   );
        if ( can_do_inplace_transform<View>( my_format ) )
        {
            Impl::view_data_t view_data( original_view( view ), get_offset<offset_t>( view ) );
            view_data.set_format( my_format );
            impl().copy_to_prepared_view( view_data );
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
};


//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // formatted_image_hpp
