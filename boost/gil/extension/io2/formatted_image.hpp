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
#include "format_tags.hpp"
#include "detail/platform_specifics.hpp"
#include "detail/io_error.hpp"
#include "detail/switch.hpp"

#include "boost/gil/extension/dynamic_image/any_image.hpp"
#include "boost/gil/packed_pixel.hpp"
#include "boost/gil/planar_pixel_iterator.hpp"
#include "boost/gil/planar_pixel_reference.hpp"
#include "boost/gil/typedefs.hpp"

#include <boost/compressed_pair.hpp>
#ifdef _DEBUG
#include <boost/detail/endian.hpp>
#endif // _DEBUG
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/vector_c.hpp>
#ifdef _DEBUG
#include <boost/numeric/conversion/converter.hpp>
#include <boost/numeric/conversion/converter_policies.hpp>
#endif // _DEBUG
#include <boost/range/iterator_range.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/decay.hpp>
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

        if ( std::tr1::is_unsigned<SrcChannelV>::value )
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
};


////////////////////////////////////////////////////////////////////////////////
/// \class formatted_image_traits
/// ( forward declaration )
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
struct formatted_image_traits;

namespace detail
{
//------------------------------------------------------------------------------

#ifndef _UNICODE
    typedef char    TCHAR;
#else
    typedef wchar_t TCHAR;
#endif
typedef iterator_range<TCHAR const *> string_chunk_t;


template <class View, typename Offset>
View const & original_view( offset_view_t<View, Offset> const & offset_view ) { return offset_view.original_view(); }

template <class View>
View const & original_view( View const & view ) { return view; }

template <typename Offset, class View>
Offset         get_offset( View                        const &             ) { return Offset();            }
template <typename Offset, class View>
Offset const & get_offset( offset_view_t<View, Offset> const & offset_view ) { return offset_view.offset(); }

template <typename Offset>
Offset         get_offset_x( Offset         const &        ) { return Offset(); }
template <typename Offset>
Offset const & get_offset_x( point2<Offset> const & offset ) { return offset.x; }

template <typename Offset>
Offset         get_offset_y( Offset         const & offset ) { return offset;   }
template <typename Offset>
Offset const & get_offset_y( point2<Offset> const & offset ) { return offset.y; }


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


////////////////////////////////////////////////////////////////////////////////
// Wrappers that normalize wrapper interfaces.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
///
/// \class configure_on_write_writer
/// \internal
/// \brief Helper wrapper for backends/writers that first need to open the
/// target/file and then be configured for the desired view.
///
////////////////////////////////////////////////////////////////////////////////

struct configure_on_write_writer
{
    template <typename Writer, typename WriterTarget, typename ViewDataHolder, format_tag DefaultFormat>
    class wrapper : public Writer
    {
    public:
        template <typename View>
        wrapper( WriterTarget const & target, View const & view, format_tag const specified_format = DefaultFormat )
            :
            Writer    ( target, specified_format ),
            view_data_( view                     )
        {}

        void write_default() { Writer::write_default( view_data_ ); }
        void write        () { Writer::write        ( view_data_ ); }

    private:
        ViewDataHolder const view_data_;
    };

    template <typename Writer, typename WriterTarget, format_tag OnlyFormat>
    class single_format_writer_wrapper : public Writer
    {
    public:
        single_format_writer_wrapper( WriterTarget const & target, format_tag const specified_format = OnlyFormat )
            :
            Writer( target )
        {
            BOOST_VERIFY( specified_format == OnlyFormat );
        }
    };
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class open_on_write_writer
/// \internal
/// \brief Helper wrapper for backends/writers that first need to be configured
/// for/created from the desired view and then open the target/file.
///
////////////////////////////////////////////////////////////////////////////////

struct open_on_write_writer
{
    template <typename Writer, typename WriterTarget, typename ViewDataHolder, format_tag DefaultFormat>
    class wrapper : public Writer
    {
    public:
        template <typename View>
        wrapper( WriterTarget const & target, View const & view, format_tag const specified_format = DefaultFormat )
            :
            Writer           ( view             ),
            target_          ( target           ),
            specified_format_( specified_format )
        {}

        void write_default() { Writer::write_default( target_, specified_format_ ); }
        void write        () { Writer::write        ( target_, specified_format_ ); }

    private:
        typename call_traits<WriterTarget>::param_type const target_;
                             format_tag                const specified_format_;
    };

    template <typename Writer, typename WriterTarget, format_tag OnlyFormat>
    class single_format_writer_wrapper : public Writer
    {
    public:
        template <typename View>
        single_format_writer_wrapper( View const & view ) : Writer( view ) {}

        void write_default( WriterTarget const & target, format_tag const specified_format = OnlyFormat ) { BOOST_VERIFY( specified_format == OnlyFormat ); Writer::write_default( target ); }
        void write        ( WriterTarget const & target, format_tag const specified_format = OnlyFormat ) { BOOST_VERIFY( specified_format == OnlyFormat ); Writer::write        ( target ); }
    };
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class formatted_image_base
///
////////////////////////////////////////////////////////////////////////////////

class formatted_image_base : noncopyable
{
public:
    typedef point2<std::ptrdiff_t> dimensions_t;

    typedef unsigned int image_type_id;
    static image_type_id const unsupported_format = static_cast<image_type_id>( -1 );

    struct sequential_row_access_state { BOOST_STATIC_CONSTANT( bool, throws_on_error = true ); };

public:
    static sequential_row_access_state begin_sequential_row_access() { return sequential_row_access_state(); }

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

        unsigned int const width ( zero_x_offset ? target_dimensions.x : (std::min)( my_dimensions.x - get_offset_x( offset_view.offset() ), target_dimensions.x ) );
        unsigned int const height(                                       (std::min)( my_dimensions.y - get_offset_y( offset_view.offset() ), target_dimensions.y ) );
        
        return subimage_view( offset_view.original_view(), 0, 0, width, height );
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
            BF_UNREACHABLE_CODE
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
/// \class formatted_image
///
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
class formatted_image : public formatted_image_base
{
private:
    template <typename T> struct gil_to_native_format;

    template <typename PixelType, typename IsPlanar>
    struct gil_to_native_format_aux
        : formatted_image_traits<Impl>::gil_to_native_format:: BOOST_NESTED_TEMPLATE apply<PixelType, IsPlanar::value>::type
    {};

public:
    typedef typename formatted_image_traits<Impl>::format_t format_t;

    typedef typename formatted_image_traits<Impl>::supported_pixel_formats_t supported_pixel_formats;
    typedef typename formatted_image_traits<Impl>::roi_t                     roi;
    typedef typename roi::offset_t                                           offset_t;

    template <typename PixelType, typename IsPlanar>
    struct gil_to_native_format<mpl::pair<PixelType, IsPlanar > > : gil_to_native_format_aux<PixelType, IsPlanar> {};

    template <typename PixelType, bool IsPlanar>
    struct gil_to_native_format<image<PixelType, IsPlanar> > : gil_to_native_format_aux<PixelType, mpl::bool_<IsPlanar> > {};

    template <typename Locator>
    struct gil_to_native_format<image_view<Locator> > : gil_to_native_format_aux<typename image_view<Locator>::value_type, is_planar<image_view<Locator> > > {};

    template <class View>
    struct has_supported_format
    {
    private:
        typedef typename get_original_view_t<View>::type original_view_t;

    public:
        typedef typename formatted_image_traits<Impl>:: BOOST_NESTED_TEMPLATE is_supported
        <
            typename original_view_t::value_type,
            is_planar<original_view_t>::value
        > type;
        BOOST_STATIC_CONSTANT( bool, value = type::value );
    };
    
    typedef any_image<supported_pixel_formats> dynamic_image_t;

    template <typename Source>
    struct reader_for
        : public mpl::at<typename formatted_image_traits<Impl>::readers, Source>
    {
        // The backend does not seem to provide a reader for the specified target...
        BOOST_STATIC_ASSERT(( !is_same<typename reader_for<Source>::type, mpl::void_>::value ));
    };

    template <typename Target>
    struct writer_for
    {
    private:
        typedef typename formatted_image_traits<Impl>::supported_image_formats supported_image_formats;

        BOOST_STATIC_CONSTANT( format_tag, default_format = mpl::front<supported_image_formats>::type::value );
        BOOST_STATIC_CONSTANT( bool      , single_format  = mpl::size <supported_image_formats>::value == 1  );

        typedef typename mpl::at
        <
            typename formatted_image_traits<Impl>::writers,
            Target
        >::type base_writer_t;

        // The backend does not seem to provide a writer for the specified target...
        BOOST_STATIC_ASSERT(( !is_same<base_writer_t, mpl::void_>::value ));

        typedef typename mpl::if_c
        <
            single_format,
            typename base_writer_t:: BOOST_NESTED_TEMPLATE single_format_writer_wrapper<base_writer_t, Target, default_format>,
            base_writer_t
        >::type first_layer_wrapper;

    public:
        typedef typename base_writer_t::wrapper
        <
            first_layer_wrapper,
            Target,
            typename formatted_image_traits<Impl>::writer_view_data_t,
            default_format
        > type;
    };

    BOOST_STATIC_CONSTANT( bool, has_full_roi = (is_same<roi::offset_t, roi::point_t>::value) );

protected:
    typedef          formatted_image                           base_t;

    typedef typename formatted_image_traits<Impl>::view_data_t view_data_t;

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
        template <typename View>
        struct apply : public has_supported_format<View> {};
    };

    typedef mpl::range_c<std::size_t, 0, mpl::size<supported_pixel_formats>::value> valid_type_id_range_t;

    struct image_id_finder
    {
        image_id_finder( format_t const format ) : format_( format ), image_id_( static_cast<unsigned int>( -1 ) ) {}

        template <typename ImageIndex>
        void operator()( ImageIndex )
        {
            typedef typename mpl::at<supported_pixel_formats, ImageIndex>::type pixel_format_t;
            format_t const image_format( gil_to_native_format<pixel_format_t>::value );
            if ( image_format == this->format_ )
            {
                BOOST_ASSERT( image_id_ == -1 );
                image_id_ = ImageIndex::value;
            }
        }

        format_t     const format_  ;
        unsigned int       image_id_;

    private:
        void operator=( image_id_finder const & );
    };

private:
    // ...zzz...MSVC++ 10 generates code to check whether this == 0...investigate...
    Impl       & impl()       { BF_ASSUME( this != 0 ); return static_cast<Impl       &>( *this ); }
    Impl const & impl() const { BF_ASSUME( this != 0 ); return static_cast<Impl const &>( *this ); }

protected:
    template <typename View>
    bool dimensions_mismatch( View const & view ) const
    {
        return formatted_image_base::dimensions_mismatch( impl().dimensions(), view );
    }

    bool dimensions_mismatch( dimensions_t const & other_dimensions ) const
    {
        return formatted_image_base::dimensions_mismatch( impl().dimensions(), other_dimensions );
    }

    template <class View>
    void do_ensure_dimensions_match( View const & view ) const
    {
        formatted_image_base::do_ensure_dimensions_match( impl().dimensions(), view );
    }

    template <typename View>
    bool formats_mismatch() const
    {
        return formats_mismatch( gil_to_native_format<get_original_view_t<View>::type>::value );
    }

    bool formats_mismatch( typename formatted_image_traits<Impl>::format_t const other_format ) const
    {
        return ( other_format != impl().closest_gil_supported_format() ) != false;
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
        return ( impl().format_size( my_format ) == static_cast<std::size_t>( memunit_step( get_original_view_t<View>::type::x_iterator() ) ) );
    }

    // A generic implementation...impl classes are encouraged to provide more
    // efficient overrides...
    static image_type_id image_format_id( format_t const closest_gil_supported_format )
    {
        // This (linear search) will be transformed into a switch...
        image_id_finder finder( closest_gil_supported_format );
        mpl::for_each<valid_type_id_range_t>( ref( finder ) );
        BOOST_ASSERT( finder.image_id_ != -1 );
        return finder.image_id_;
    }

    template <typename View>
    static View const & subview_for_offset( View const & view ) { return view; }

    template <typename View, typename Offset>
    View subview_for_offset( offset_view_t<View, Offset> const & offset_view ) const
    {
        return formatted_image_base::subview_for_offset( impl().dimensions(), offset_view );
    }

    template <typename View>
    static view_data_t get_view_data( View const & view )
    {
        return view_data_t( view );
    }

    template <typename View>
    view_data_t get_view_data( offset_view_t<View, offset_t> const & offset_view ) const
    {
        return view_data_t
        (
            subview_for_offset( offset_view ),
            offset_view.offset()
        );
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
        bool const can_use_raw
        (
            is_supported          <View>::value &&
            formatted_image_traits<Impl>::builtin_conversion
        );
        default_convert_to_worker( view, mpl::bool_<can_use_raw>() );
    }
    
    template <typename FormatConverter, typename View>
    void copy_to( View const & view, ensure_dimensions_match, FormatConverter const & format_converter ) const
    {
        impl().do_ensure_dimensions_match( view );
        impl().copy_to( view, assert_dimensions_match(), format_converter );
    }

    template <typename FormatConverter, typename View>
    void copy_to( View const & view, assert_dimensions_match, FormatConverter const & format_converter ) const
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
        switch_<valid_type_id_range_t>
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

public: // Offset factory
    template <class View>
    static
    offset_view_t<View, offset_t>
    offset_view( View const & view, offset_t const offset ) { return offset_view_t<View, offset_t>( view, offset ); }

public: // Utility 'quick-wrappers'...
    template <class Source, class Image>
    static void read( Source const & target, Image & image )
    {
        typedef typename reader_for<typename decay<Source>::type>::type reader_t;
        // The backend does not know how to read from the specified source type.
        BOOST_STATIC_ASSERT(( !is_same<reader_t, mpl::void_>::value ));
        reader_t( target ).copy_to_image( image, synchronize_dimensions(), synchronize_formats() );
    }

    template <class Target, class View>
    static void write( Target & target, View const & view )
    {
        typedef typename writer_for<typename decay<Target>::type>::type writer_t;
        // The backend does not know how to write to the specified target type.
        BOOST_STATIC_ASSERT(( !is_same<writer_t, mpl::void_>::value ));
        writer_t( target, view ).write_default();
    }

private:
    template <class View, typename CC>
    class in_place_converter_t
    {
    public:
        typedef void result_type;

        in_place_converter_t( CC const & cc, View const & view ) : members_( cc, view ) {}

        template <std::size_t index>
        void operator()( mpl::integral_c<std::size_t, index> ) const
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
        typename enable_if<is_pixel<SrcP>>::type
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
        void operator()( mpl::integral_c<std::size_t, index> const & ) const
        {
            typedef typename mpl::at_c<supported_pixel_formats, index>::type::view_t my_view_t;
            impl_.generic_convert_to_prepared_view<my_view_t>( view(), cc() );
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
                    // See the note for formatted_image_base::subview_for_offset()...
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
                is_supported           <                             View       >::value
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
            if ( formatted_image_traits<Impl>::builtin_conversion )
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
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // formatted_image_hpp
