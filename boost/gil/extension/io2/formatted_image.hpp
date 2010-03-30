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

#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/integral_c.hpp>
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


namespace detail
{
//------------------------------------------------------------------------------


typedef iterator_range<TCHAR const *> string_chunk_t;


template <typename Pixel, bool planar>
struct pixel_format_type : mpl::pair<Pixel, mpl::bool_<planar>> {};


struct get_planar_pixel_iterator
{
    template <typename PixelFormatType>
    struct apply
    {
        typedef planar_pixel_iterator
                <
                    typename channel_type    <typename PixelFormatType::first>::type,
                    typename color_space_type<typename PixelFormatType::first>::type
                > type;
    };
};

struct get_plain_pixel_iterator
{
    template <typename PixelFormatType>
    struct apply
    {
        typedef typename PixelFormatType::first * type;
    };
};


template <typename PixelFormat>
struct view_for_pixel_format
{
    typedef typename type_from_x_iterator
            <
                typename mpl::if_
                <
                    typename PixelFormat::second,
                    get_planar_pixel_iterator,
                    get_plain_pixel_iterator

                >::type::apply<PixelFormat>::type
            >::view_t type;
};

template <typename PixelFormats>
struct views_for_pixel_formats
    : public mpl::transform<PixelFormats, view_for_pixel_format<mpl::_1> > {};


template <typename PixelFormat>
struct const_view_for_pixel_format
    :
    public view_for_pixel_format<pixel_format_type<typename PixelFormat::first const, PixelFormat::second::value> >
{};

template <typename PixelFormats>
struct const_views_for_pixel_formats
    : public mpl::transform<PixelFormats, const_view_for_pixel_format<mpl::_1> > {};



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
    static image_type_id const unsupported_format = -1;

protected:
    static bool dimensions_match( dimensions_t const & mine, dimensions_t const & other ) { return mine == other; }
    template <class View>
    static bool dimensions_match( dimensions_t const & mine, View const & view ) { return dimensions_match( mine, view.dimensions(); ) }

    static void do_ensure_dimensions_match( dimensions_t const & mine, dimensions_t const & other )
    {
        io_error_if( !dimensions_match( mine, other ), "input view size does not match source image size" );
    }
    template <class View>
    static void do_ensure_dimensions_match( dimensions_t const & mine, View const & view )
    {
        do_ensure_dimensions_match( mine, view.dimensions() );
    }

    static void do_ensure_formats_match( bool const formats_match )
    {
        io_error_if( !formats_match, "input view format does not match source image format" );
    }

protected:
    template <typename Image>
    void do_synchronize_dimensions( Image & image, dimensions_t const & my_dimensions, unsigned int const alignment = 0 )
    {
        image.recreate( my_dimensions, alignment );
    }

protected:
    struct assert_type_mismatch
    {
        typedef bool result_type;
        template <typename Index>
        result_type operator()( Index const & ) const { BOOST_ASSERT( !"input view format does not match source image format" ); return false; }
    };

    struct throw_type_mismatch
    {
        typedef void result_type;
        result_type operator()() const { do_ensure_formats_match( false ); }
    };

    template <typename Type, typename SupportedPixelFormats>
    struct check_type_match
    {
        typedef bool result_type;
        template <typename SupportedFormatIndex>
        result_type operator()( SupportedFormatIndex const & ) const
        {
            return worker<SupportedFormatIndex>( mpl::less<SupportedFormatIndex, mpl::size<SupportedPixelFormats> >() );
        }

        template <typename Index>
        bool worker( mpl::true_ /*in-range*/ ) const
        {
            return is_same<Type, typename mpl::at<SupportedPixelFormats, Index>::type>::value;
        }
        template <typename Index>
        bool worker( mpl::false_ /*not-in-range*/ ) const
        {
            return false;
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

template <class Impl, class SupportedPixelFormats, class ROI>
class formatted_image : public formatted_image_base
{
public:
    //typedef typename any_image<typename Impl::supported_pixel_formats> any_image_t;
    typedef SupportedPixelFormats supported_pixel_formats;
    typedef ROI                   roi;

    typedef any_image_view<typename const_views_for_pixel_formats<typename supported_pixel_formats>::type> const_view_t;
    typedef any_image_view<typename views_for_pixel_formats      <typename supported_pixel_formats>::type>       view_t;

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
        void apply()
        {
            impl_.copy_to( base::apply<Image>(), dimensions_policy(), formats_policy() );
        }

        template <typename SupportedFormatIndex>
        void operator()( SupportedFormatIndex const & )
        {
            apply<typename mpl::at<SupportedFormatIndex>::type>();
        }

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
        void apply( View const & view )
        {
            impl_.copy_from( view, dimensions_policy(), formats_policy() );
        }

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
    template <typename View>
    bool formats_match()
    {
        Gdiplus::PixelFormat tzt = view_gp_format<View>::value;
        return switch_<valid_type_id_range_t>
        (
            impl().current_image_type_id(),
            check_type_match
            <
                pixel_format_type
                <
                    typename View::value_type,
                    is_planar<typename View>::value
                >,
                supported_pixel_formats
            >(),
            assert_type_mismatch()
        );
    }

    Impl & impl() { return static_cast<Impl &>( *this ); }

public:
    template <typename Image, typename DimensionsPolicy, typename FormatsPolicy>
    void copy_to_image( Image & image, DimensionsPolicy const dimensions_policy, FormatsPolicy const formats_policy )
    {
        impl().copy_to( view( image ), dimensions_policy, formats_policy );
    }

    template <typename Image, typename FormatsPolicy>
    void copy_to_image( Image & image, synchronize_dimensions, FormatsPolicy const formats_policy )
    {
        do_synchronize_dimensions( image, impl().dimensions(), Impl::desired_alignment );
        impl().copy_to( view( image ), assert_dimensions_match(), formats_policy );
    }

    template <typename View>
    void copy_to( View & view, assert_dimensions_match, assert_formats_match )
    {
        BOOST_ASSERT( dimensions_match( view.dimensions(), impl().dimensions() ) );
        BOOST_ASSERT( formats_match<View>()                                      );
        impl().copy_to_prepared_view( view );
    }

    template <typename View>
    void copy_to( View & view, assert_dimensions_match, ensure_formats_match )
    {
        do_ensure_formats_match( formats_match<View>() );
        copy_to( view, assert_dimensions_match(), assert_formats_match() );
    }

    template <typename View>
    void copy_to( View & view, ensure_dimensions_match, assert_formats_match )
    {
        do_ensure_dimensions_match( formats_match<View>() );
        copy_to( view, assert_dimensions_match, assert_formats_match );
    }

    template <typename Images, typename dimensions_policy, typename formats_policy>
    void copy_to( any_image<Images> & im )
    {
        typedef mpl::range_c<std::size_t, 0, typename Impl::supported_pixel_formats> valid_range_t;
        switch_<valid_range_t>
        (
            Impl::current_image_type_id(),
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

    void save_to( std::string const & path )
    {
        impl().save_to( path.c_str() );
    }

    void save_to( std::wstring const & path )
    {
        impl().save_to( path.c_str() );
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
