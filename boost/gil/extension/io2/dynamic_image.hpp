////////////////////////////////////////////////////////////////////////////////
///
/// \file formatted_image.hpp
/// -------------------------
///
/// Base CRTP class for all image implementation classes/backends.
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
#ifndef dynamic_image_hpp__85C4E125_7906_42D5_B25B_4D6D82DE13E0
#define dynamic_image_hpp__85C4E125_7906_42D5_B25B_4D6D82DE13E0
#pragma once
//------------------------------------------------------------------------------
#include "format_tags.hpp"
#include "detail/platform_specifics.hpp"
#include "detail/io_error.hpp"
#include "detail/switch.hpp"

#include "boost/gil/extension/dynamic_image/any_image.hpp"
#include "boost/gil/extension/io/dynamic_io.hpp" //...zzz...
#include "boost/gil/packed_pixel.hpp"
#include "boost/gil/planar_pixel_iterator.hpp"
#include "boost/gil/planar_pixel_reference.hpp"
#include "boost/gil/typedefs.hpp"

#include <boost/compressed_pair.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/vector_c.hpp>
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
    struct assert_on_type_mismatch
    {
        typedef bool result_type;
        template <typename Index>
        result_type operator()( Index const & ) const { BOOST_ASSERT( !"input view format does not match source image format" ); return false; }
    };

    struct throw_on_type_mismatch
    {
        typedef void result_type;
        result_type operator()() const { do_ensure_formats_match( true ); }
    };

    template <typename Type, typename SupportedPixelFormats>
    struct check_type_match
    {
        typedef bool result_type;
        template <typename SupportedFormatIndex>
        result_type operator()( SupportedFormatIndex ) const
        {
            return is_same<Type, typename mpl::at<SupportedPixelFormats, SupportedFormatIndex>::type>::value;
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
            return image_. BOOST_NESTED_TEMPLATE _dynamic_cast<Image>();
        }

    protected:
        any_image<Images> & image_;
    };


    typedef any_image<supported_pixel_formats> dynamic_image_t;

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
        void apply() { impl_.copy_to( base:: BOOST_NESTED_TEMPLATE apply<Image>(), dimensions_policy(), formats_policy() ); }

        template <typename SupportedFormatIndex>
        void operator()( SupportedFormatIndex ) { this->apply<typename mpl::at<SupportedFormatIndex>::type>(); }

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

    template <typename Images, typename dimensions_policy, typename formats_policy>
    void copy_to_image( any_image<Images> & im ) const
    {
        switch_<valid_type_id_range_t>
        (
            impl().current_image_format_id(),
            read_dynamic_image<Images, dimensions_policy, formats_policy>( im, *this ),
            throw_on_type_mismatch()
        );
    }

    struct write_is_supported
    {
        template <typename View>
        struct apply : public has_supported_format<View> {};
    };

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
//------------------------------------------------------------------------------
} // namespace detail    

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // dynamic_image_hpp
