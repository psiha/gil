////////////////////////////////////////////////////////////////////////////////
///
/// \file writer_for.hpp
/// --------------------
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
#ifndef writer_for_hpp__CBE52E51_7B06_4018_996A_63F9B7DF05AC
#define writer_for_hpp__CBE52E51_7B06_4018_996A_63F9B7DF05AC
#pragma once
//------------------------------------------------------------------------------

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

////////////////////////////////////////////////////////////////////////////////
/// \class backend_traits
/// ( forward declaration )
////////////////////////////////////////////////////////////////////////////////

template <class Backend>
struct backend_traits;

namespace detail
{
//------------------------------------------------------------------------------


template <class Writer, typename Target>
class writer_extender
    :
    public output_device<Target>,
    public Writer
{
public:
    writer_extender( Target const target )
        :
        output_device<Target>( target                                     ),
        Writer               ( output_device<Target>::transform( target ) )
    {}

    writer_extender( Target const target, format_tag const file_format )
        :
        output_device<Target>( target                                                  ),
        Writer               ( output_device<Target>::transform( target ), file_format )
    {}
};


////////////////////////////////////////////////////////////////////////////////
// Wrappers that normalize writer interfaces.
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

//------------------------------------------------------------------------------
} // namespace detail

template <class Backend, typename Target>
struct writer_for
{
private:
    typedef typename backend_traits<Backend>::supported_image_formats supported_image_formats;

    BOOST_STATIC_CONSTANT( format_tag, default_format = mpl::front<supported_image_formats>::type::value );
    BOOST_STATIC_CONSTANT( bool      , single_format  = mpl::size <supported_image_formats>::value == 1  );

    typedef typename mpl::has_key<typename backend_traits<Backend>::native_sinks, Target>::type supported_by_native_writer_t;

    // The backend does not seem to provide a writer for the specified target...
    BOOST_STATIC_ASSERT
    ((
        supported_by_native_writer_t::value ||
        !unknown_device<Target>     ::value
    ));

    typedef typename mpl::if_
    <
        supported_by_native_writer_t,
                                typename Backend::native_writer,
        detail::writer_extender<typename Backend::device_writer, Target>
    >::type base_writer_t;

    typedef typename mpl::if_c
    <
        single_format,
        typename base_writer_t:: BOOST_NESTED_TEMPLATE single_format_writer_wrapper<base_writer_t, Target, default_format>,
        base_writer_t
    >::type first_layer_wrapper;

public:
    typedef typename base_writer_t:: BOOST_NESTED_TEMPLATE wrapper
    <
        first_layer_wrapper,
        Target,
        typename backend_traits<Backend>::writer_view_data_t,
        default_format
    > type;
};

//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // writer_for_hpp
