////////////////////////////////////////////////////////////////////////////////
///
/// \file backend.hpp
/// -----------------
///
/// Base CRTP class for all image implementation classes/backends.
///
/// Copyright (c) Domagoj Saric 2010.-2013.
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
#ifndef backend_hpp__C34C1FB0_A4F5_42F3_9318_5805B88CFE49
#define backend_hpp__C34C1FB0_A4F5_42F3_9318_5805B88CFE49
#pragma once
//------------------------------------------------------------------------------
#include "boost/gil/extension/io2/format_tags.hpp"
#include "boost/gil/extension/io2/detail/platform_specifics.hpp"
#include "boost/gil/extension/io2/detail/io_error.hpp"
#include "boost/gil/extension/io2/detail/switch.hpp"

#include "boost/gil/packed_pixel.hpp"
#include "boost/gil/planar_pixel_iterator.hpp"
#include "boost/gil/planar_pixel_reference.hpp"
#include "boost/gil/typedefs.hpp"

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
#include <boost/type_traits/is_unsigned.hpp>
#include <boost/noncopyable.hpp>
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------

#if defined(BOOST_MSVC)
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // "conditional expression is constant"
#endif

template <typename SrcChannelV, typename DstChannelV> // Model ChannelValueConcept
struct channel_converter;

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

namespace io
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// \class backend_traits
/// ( forward declaration )
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
struct backend_traits;

template <class Backend, typename Source>
struct reader_for;

template <class Backend, typename Target>
struct writer_for;

namespace detail
{
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// \internal
/// \class backend_base
////////////////////////////////////////////////////////////////////////////////

class backend_base : noncopyable
{
public: // Low-level (row, strip, tile) access
    struct sequential_row_read_state { BOOST_STATIC_CONSTANT( bool, throws_on_error = true ); };

    static sequential_row_read_state begin_sequential_row_read() { return sequential_row_read_state(); }

    static bool can_do_row_access  () { return true ; }
    static bool can_do_strip_access() { return false; }
    static bool can_do_tile_access () { return false; }

    static bool can_do_roi_access         () { return false; }
    static bool can_do_vertical_roi_access() { return true ; }

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
}; // class backend_base


////////////////////////////////////////////////////////////////////////////////
///
/// \class backend
///
/// \brief Base CRTP class for all image IO implementation classes/backends.
///
/// Outlines the basic interface model that is provided by all IO backends. Each
/// IO backend is a wrapper (i.e. interface adaptor) for a particular 3rd party
/// image IO library or builtin OS API (e.g. LibPNG, LibTIFF, Windows GDI+, OSX
/// CoreGraphics, ...). As such individual backends do not map to a particular
/// image file format (e.g. JPG, PNG, ...), some backends are specialized only
/// for a single file format (e.g. LibJPEG) while others form more generic
/// frameworks that can read and write multiple file formats (e.g. WIC).
///
/// Particular file formats as well as image IO libraries substantially differ
/// in significant parts of their design, interface and intended workflow. Some
/// offer fully generic 2D ROI access while others only sequential row access.
/// Some have builtin support for non-trivial image and pixel transformations
/// while others have none. GIL.IO2 does not aim to provide a one-size-fits-all
/// interface because this is for most parts a very difficult task (e.g. a
/// compression quality interface for non-lossy formats makes no sense) with a
/// questionable purpose and gain. For example, emulating 2D ROI access requires
/// a non-trivial amount of code and logic while at the same proves to be a
/// self contradicting effort as the extra code (and temporary storage
/// allocations) defeat the main purpose of ROI access - avoiding extra work
/// for efficiency reasons.
///
////////////////////////////////////////////////////////////////////////////////

template <class Impl>
class backend : public backend_base
{
public:
	/// \name Pixel format support
	/// @{

    //typedef typename backend_traits<Impl>::format_t format_t;

	/// MPL vector of pixel<> specializations supported by the backend.
    typedef typename backend_traits<Impl>::supported_pixel_formats_t supported_pixel_formats;

    template <typename PixelType, bool IsPlanar>
    struct native_format
        : backend_traits<Impl>::gil_to_native_format:: BOOST_NESTED_TEMPLATE apply<PixelType, IsPlanar>::type
    {};

	template <typename T> struct get_native_format;

#ifndef DOXYGEN_ONLY
	template <typename PixelType, typename IsPlanar>
	struct get_native_format<mpl::pair<PixelType, IsPlanar> > : native_format<PixelType, IsPlanar::value> {};

	template <typename PixelType, bool IsPlanar, class Allocator>
	struct get_native_format<image<PixelType, IsPlanar, Allocator> > : native_format<PixelType, IsPlanar> {};

	template <typename Locator>
	struct get_native_format<image_view<Locator> > : native_format<typename image_view<Locator>::value_type, is_planar<image_view<Locator> >::value> {};
#endif // DOXYGEN_ONLY

	/// @} // pixel format support
    
	/// \name Region Of Interest access
	/// @{

	typedef typename backend_traits<Impl>::roi_t roi; ///< Region-Of-Interest
    typedef typename roi::offset_t               offset_t; ///< Starting ROI offset

    static bool const has_full_roi = is_same<typename roi::offset_t, typename roi::point_t>::value;

	/// @} // ROI

    // https://bugzilla.gnome.org/show_bug.cgi?id=705937
	/// \name IO: readers and writers
	/// @{

    template <typename Source> struct reader_for : gil::io::reader_for<Impl, Source> {}; ///< Returns the class capable of reading images from the Source device
    template <typename Target> struct writer_for : gil::io::writer_for<Impl, Target> {}; ///< Returns the class capable of writing images to the Target device

#ifdef DOXYGEN_ONLY
	class native_reader;
	class device_reader;

	class native_writer;
	class device_writer;
#endif // DOXYGEN_ONLY

	// \todo Backend-independent metafunctions that will automatically choose
	// the backend...

	/// @} // IO

protected:
    typedef          backend                           base_t;

    //typedef typename backend_traits<Impl>::view_data_t view_data_t;

private:
    // MSVC++ 10 generates code to check whether this == 0.
    Impl       & impl()       { BF_ASSUME( this != 0 ); return static_cast<Impl       &>( *this ); }
    Impl const & impl() const { BF_ASSUME( this != 0 ); return static_cast<Impl const &>( *this ); }
}; // class backend

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // backend_hpp
