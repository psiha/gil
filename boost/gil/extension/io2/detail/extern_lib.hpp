////////////////////////////////////////////////////////////////////////////////
///
/// \file extern_lib.hpp
/// --------------------
///
/// BOOST_DELAYED_EXTERN_LIB_GUARD helper macro for reducing boilerplate code
/// related to dynamic/run-time linking.
///
/// Copyright (c) Domagoj Saric 2010.
///
///  Use, modification and distribution is subject to the Boost Software License, Version 1.0.
///  (See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt)
///
/// For more information, see http://www.boost.org
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#pragma once
#ifndef extern_lib_hpp__EAE6695D_91B6_4E14_A3B9_9931BA3FB3B3
#define extern_lib_hpp__EAE6695D_91B6_4E14_A3B9_9931BA3FB3B3
//------------------------------------------------------------------------------
#include <boost/assert.hpp>
#include <boost/concept_check.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/control/expr_if.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/facilities/expand.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/utility/in_place_factory.hpp>

#ifdef _WIN32
    #include "windows.h"
#else // _WIN32
    ...do implement...
#endif // platform switch
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
//
// Configuration macros
//
////////////////////////////////////////////////////////////////////////////////

#define BOOST_LIB_LINK_LOADTIME_OR_STATIC    1
#define BOOST_LIB_LINK_RUNTIME_ASSUME_LOADED 2
#define BOOST_LIB_LINK_RUNTIME_AUTO_LOAD     3

#define BOOST_LIB_LOADING_STATIC             1
#define BOOST_LIB_LOADING_SINGLE             2
#define BOOST_LIB_LOADING_RELOADABLE         3

#define BOOST_LIB_INIT_ASSUME                1
#define BOOST_LIB_INIT_AUTO                  2

// Usage example:
// #define BOOST_GIL_EXTERNAL_LIB ( BOOST_LIB_LINK_RUNTIME_ASSUME_LOADED, BOOST_LIB_LOADING_RELOADABLE, BOOST_LIB_INIT_AUTO )


//...or maybe provide the above options merged...something like this:
//#define StaticallyLinkAssumeInitialized       0
//#define StaticallyLinkAutoInitialized         1
//#define DynamicLinkAssumeLoadedAndInitialized 2
//#define DynamicLinkAssumeLoadedAutoInitialize 3
//#define DynamicLinkAutoLoadAutoInitialize     4


////////////////////////////////////////////////////////////////////////////////
//
// Base guard classes
//
////////////////////////////////////////////////////////////////////////////////

class win32_lib_handle : noncopyable
{
public:
    HMODULE lib_handle() const { return lib_handle_; }

    static HMODULE loaded_lib_handle( char    const * const lib_name ) { return ::GetModuleHandleA( lib_name ); }
    static HMODULE loaded_lib_handle( wchar_t const * const lib_name ) { return ::GetModuleHandleW( lib_name ); }

    template <typename T>
    static T checked_proc_address( HMODULE const lib_handle, char const * const function_name )
    {
        BOOST_ASSERT( lib_handle );
        FARPROC const funcion_address( ::GetProcAddress( lib_handle, function_name ) );
        BOOST_ASSERT( funcion_address && "Requested function not exported by the library." );
        return reinterpret_cast<T>( *funcion_address );
    }

    template <typename T>
    T checked_proc_address( char const * const function_name ) const
    {
        return checked_proc_address<T>( lib_handle(), function_name );
    }

protected:
    win32_lib_handle( HMODULE const lib_handle ) : lib_handle_( lib_handle ) {}
    ~win32_lib_handle() {}

    void ensure() const; //...implement later as desired...
    void verify() const { BOOST_ASSERT( lib_handle() && "Library not loaded." ); }

private:
    HMODULE const lib_handle_;
};


class win32_preloaded_reloadable_lib : public win32_lib_handle
{
public:
    win32_preloaded_reloadable_lib( char    const * const lib_name ) : win32_lib_handle( loaded_lib_handle( lib_name ) ) { verify(); }
    win32_preloaded_reloadable_lib( wchar_t const * const lib_name ) : win32_lib_handle( loaded_lib_handle( lib_name ) ) { verify(); }
};


class win32_preloaded_delayloaded_lib
{
public:
    win32_preloaded_delayloaded_lib( char const * const lib_name )
    {
        if ( !delayed_lib_.is_initialized() )
            delayed_lib_ = in_place( lib_name );
        else
            BOOST_ASSERT
            (
                ( delayed_lib_->lib_handle() == win32_preloaded_reloadable_lib( lib_name ).lib_handle() ) &&
                "Library reloaded/relocated!"
            );
    }

    win32_preloaded_delayloaded_lib( wchar_t const * const lib_name )
    {
        if ( !delayed_lib_.is_initialized() )
            delayed_lib_ = in_place( lib_name );
        else
            BOOST_ASSERT
            (
                ( delayed_lib_->lib_handle() == win32_preloaded_reloadable_lib( lib_name ).lib_handle() ) &&
                "Library reloaded/relocated!"
            );
    }

    static HMODULE lib_handle() { return delayed_lib_->lib_handle(); }

    static HMODULE loaded_lib_handle( char    const * const lib_name )
    {
        BOOST_ASSERT( win32_lib_handle::loaded_lib_handle( lib_name ) == lib_handle() );
        ignore_unused_variable_warning( lib_name );
        return lib_handle();
    }
    static HMODULE loaded_lib_handle( wchar_t const * const lib_name )
    {
        BOOST_ASSERT( win32_lib_handle::loaded_lib_handle( lib_name ) == lib_handle() );
        ignore_unused_variable_warning( lib_name );
        return lib_handle();
    }

private:
    static optional<win32_preloaded_reloadable_lib> delayed_lib_;
};

__declspec( selectany )
optional<win32_preloaded_reloadable_lib> win32_preloaded_delayloaded_lib::delayed_lib_;


class win32_reloadable_lib_guard : public win32_lib_handle
{
public:
    win32_reloadable_lib_guard( char    const * const lib_name ) : win32_lib_handle( ::LoadLibraryA( lib_name ) ) { ensure(); }
    win32_reloadable_lib_guard( wchar_t const * const lib_name ) : win32_lib_handle( ::LoadLibraryW( lib_name ) ) { ensure(); }

    ~win32_reloadable_lib_guard() { BOOST_VERIFY( ::FreeLibrary( lib_handle() ) ); }
};


class win32_delayloaded_lib_guard
{
public:
    win32_delayloaded_lib_guard( char const * const lib_name )
    {
        if ( !delayed_lib_.is_initialized() )
            delayed_lib_ = in_place( lib_name );
        else
            BOOST_ASSERT
            (
                ( delayed_lib_->lib_handle() == win32_reloadable_lib_guard( lib_name ).lib_handle() ) &&
                "Library reloaded/relocated!"
            );
    }

    win32_delayloaded_lib_guard( wchar_t const * const lib_name )
    {
        if ( !delayed_lib_.is_initialized() )
            delayed_lib_ = in_place( lib_name );
        else
            BOOST_ASSERT
            (
                ( delayed_lib_->lib_handle() == win32_reloadable_lib_guard( lib_name ).lib_handle() ) &&
                "Library reloaded/relocated!"
            );
    }

    static HMODULE lib_handle() { return delayed_lib_->lib_handle(); }

    static HMODULE loaded_lib_handle( char    const * const lib_name )
    {
        BOOST_ASSERT( win32_lib_handle::loaded_lib_handle( lib_name ) == lib_handle() );
        ignore_unused_variable_warning( lib_name );
        return lib_handle();
    }
    static HMODULE loaded_lib_handle( wchar_t const * const lib_name )
    {
        BOOST_ASSERT( win32_lib_handle::loaded_lib_handle( lib_name ) == lib_handle() );
        ignore_unused_variable_warning( lib_name );
        return lib_handle();
    }

private:
    static optional<win32_reloadable_lib_guard> delayed_lib_;
};

__declspec( selectany )
optional<win32_reloadable_lib_guard> win32_delayloaded_lib_guard::delayed_lib_;


class dummy_lib_guard
{
public:
    dummy_lib_guard( char    const * /*lib_name*/ ) {}
    dummy_lib_guard( wchar_t const * /*lib_name*/ ) {}

    static HMODULE loaded_lib_handle( char    const * const lib_name ) { return win32_lib_handle::loaded_lib_handle( lib_name ); }
    static HMODULE loaded_lib_handle( wchar_t const * const lib_name ) { return win32_lib_handle::loaded_lib_handle( lib_name ); }
};



////////////////////////////////////////////////////////////////////////////////
//
// Preprocessor section
//
////////////////////////////////////////////////////////////////////////////////

#define BOOST_LIB_LINK(    option ) BOOST_PP_TUPLE_ELEM( 3, 0, option )
#define BOOST_LIB_LOADING( option ) BOOST_PP_TUPLE_ELEM( 3, 1, option )
#define BOOST_LIB_INIT(    option ) BOOST_PP_TUPLE_ELEM( 3, 2, option )

#define BOOST_LIB_GUARD_FOR_LINK_TYPE( type )                                                  \
    BOOST_PP_EXPR_IF                                                                           \
    (                                                                                          \
        BOOST_PP_EQUAL( BOOST_LIB_LINK( type ), BOOST_LIB_LINK_LOADTIME_OR_STATIC ),           \
        no need for a lib guard with load time or static linking                               \
    )                                                                                          \
    BOOST_PP_IF                                                                                \
    (                                                                                          \
        BOOST_PP_EQUAL( BOOST_LIB_INIT( type ), BOOST_LIB_INIT_ASSUME ),                       \
        dummy_lib_guard,                                                                       \
        BOOST_PP_IF                                                                            \
        (                                                                                      \
            BOOST_PP_EQUAL( BOOST_LIB_LINK( type ), BOOST_LIB_LINK_RUNTIME_ASSUME_LOADED ),    \
            BOOST_PP_IF                                                                        \
            (                                                                                  \
                BOOST_PP_EQUAL( BOOST_LIB_LOADING( type ), BOOST_LIB_LOADING_SINGLE ),         \
                win32_preloaded_delayloaded_lib,                                               \
                win32_preloaded_reloadable_lib                                                 \
            ),                                                                                 \
            BOOST_PP_IF                                                                        \
            (                                                                                  \
                BOOST_PP_EQUAL( BOOST_LIB_LOADING( type ), BOOST_LIB_LOADING_SINGLE ),         \
                win32_delayloaded_lib_guard,                                                   \
                win32_reloadable_lib_guard                                                     \
            )                                                                                  \
        )                                                                                      \
    )


#define BOOST_DELAYED_EXTERN_LIB_FUNCTION_TYPEDEF_AND_POINTER_WORKER( condition, return_type, call_convention, function_name, parameter_sequence )   \
    BOOST_PP_EXPR_IF                                                                                                                                 \
    (                                                                                                                                                \
        condition,                                                                                                                                   \
        typedef return_type ( call_convention & BOOST_PP_CAT( P, function_name ) ) ( BOOST_PP_SEQ_ENUM( parameter_sequence ) ) throw();              \
                return_type ( call_convention & BOOST_PP_CAT( p, function_name ) ) ( BOOST_PP_SEQ_ENUM( parameter_sequence ) ) throw();              \
    )

#define BOOST_DELAYED_EXTERN_LIB_FUNCTION_TYPEDEF_AND_POINTER( r, dummy, element )   \
    BOOST_DELAYED_EXTERN_LIB_FUNCTION_TYPEDEF_AND_POINTER_WORKER( BOOST_PP_TUPLE_ELEM( 5, 0, element ), BOOST_PP_TUPLE_ELEM( 5, 1, element ), BOOST_PP_TUPLE_ELEM( 5, 2, element ), BOOST_PP_TUPLE_ELEM( 5, 3, element ), BOOST_PP_TUPLE_ELEM( 5, 4, element ) )


#define BOOST_DELAYED_EXTERN_LIB_FUNCTION_PARAMETERS( r, data, index, parameter )   \
    BOOST_PP_COMMA_IF( index )                                                      \
    parameter BOOST_PP_CAT( prm, index )


#define BOOST_DELAYED_EXTERN_LIB_FUNCTION_ARGUMENTS( r, data, index, parameter )   \
    BOOST_PP_COMMA_IF( index )                                                     \
    BOOST_PP_CAT( prm, index )


#define BOOST_DELAYED_EXTERN_LIB_FUNCTION_IMPLEMENTATION_WORKER( condition, guard_class_name, return_type, call_convention, function_name, parameter_sequence )   \
    BOOST_PP_EXPR_IF                                                                                                                                 \
    (                                                                                                                                                \
        condition,                                                                                                                                   \
        inline __declspec( nothrow ) return_type call_convention function_name( BOOST_PP_SEQ_FOR_EACH_I( BOOST_DELAYED_EXTERN_LIB_FUNCTION_PARAMETERS, 0, parameter_sequence ) ) {          \
            return guard_class_name::lib_functions().BOOST_PP_CAT( p, function_name )( BOOST_PP_SEQ_FOR_EACH_I( BOOST_DELAYED_EXTERN_LIB_FUNCTION_ARGUMENTS, 0, parameter_sequence ) ); }   \
    )

#define BOOST_DELAYED_EXTERN_LIB_FUNCTION_IMPLEMENTATION( r, guard_class_name, element )   \
    BOOST_DELAYED_EXTERN_LIB_FUNCTION_IMPLEMENTATION_WORKER( BOOST_PP_TUPLE_ELEM( 5, 0, element ), guard_class_name, BOOST_PP_TUPLE_ELEM( 5, 1, element ), BOOST_PP_TUPLE_ELEM( 5, 2, element ), BOOST_PP_TUPLE_ELEM( 5, 3, element ), BOOST_PP_TUPLE_ELEM( 5, 4, element ) )


#define BOOST_DELAYED_EXTERN_LIB_FUNCTION_LOAD_WORKER( function_name )                                                                                                     \
    BOOST_PP_CAT( p, function_name )( boost::win32_lib_handle::checked_proc_address<BOOST_PP_CAT( P, function_name )>( lib_handle, BOOST_PP_STRINGIZE( function_name ) ) )

#define BOOST_DELAYED_EXTERN_LIB_FUNCTION_LOAD( r, data, index, function_tuple )        \
    BOOST_PP_EXPR_IF                                                                    \
    (                                                                                   \
        BOOST_PP_TUPLE_ELEM( 5, 0, function_tuple ),                                    \
        BOOST_PP_COMMA_IF( index )                                                      \
        BOOST_DELAYED_EXTERN_LIB_FUNCTION_LOAD_WORKER                                   \
        (                                                                               \
            BOOST_PP_TUPLE_ELEM( 5, 3, function_tuple )                                 \
        )                                                                               \
    )


#define BOOST_DELAYED_EXTERN_LIB_GUARD( lib_name, guard_class_name, guard_option, functionSequence )                \
    class guard_class_name : public boost:: BOOST_LIB_GUARD_FOR_LINK_TYPE( guard_option )                           \
    {                                                                                                               \
    private:                                                                                                        \
        struct functions : ::boost::noncopyable                                                                     \
        {                                                                                                           \
            BOOST_PP_SEQ_FOR_EACH( BOOST_DELAYED_EXTERN_LIB_FUNCTION_TYPEDEF_AND_POINTER, 0 , functionSequence );   \
                                                                                                                    \
            functions( HMODULE const lib_handle )                                                                   \
                :                                                                                                   \
                BOOST_PP_SEQ_FOR_EACH_I( BOOST_DELAYED_EXTERN_LIB_FUNCTION_LOAD, 0 , functionSequence )             \
            {}                                                                                                      \
        };                                                                                                          \
        template <int dummy = 0>                                                                                    \
        struct functions_singleton : ::boost::noncopyable                                                           \
        {                                                                                                           \
            static void create( HMODULE const lib_handle )                                                          \
            {                                                                                                       \
                if ( !functions_.is_initialized() )                                                                 \
                    functions_ = boost::in_place( lib_handle );                                                     \
            }                                                                                                       \
            static functions const & lib_functions() { return *functions_; }                                        \
        private:                                                                                                    \
            static boost::optional<functions> functions_;                                                           \
        };                                                                                                          \
    public:                                                                                                         \
        guard_class_name() : boost:: BOOST_LIB_GUARD_FOR_LINK_TYPE( guard_option ) ( L#lib_name )                   \
        {                                                                                                           \
            BOOST_PP_EXPR_IF                                                                                        \
            (                                                                                                       \
                BOOST_PP_EQUAL( BOOST_LIB_INIT( guard_option ), BOOST_LIB_INIT_AUTO ),                              \
                functions_singleton<>::create( lib_handle() );                                                      \
            )                                                                                                       \
        }                                                                                                           \
        static void init_functions( HMODULE const lib_handle ) { functions_singleton<>::create( lib_handle ); }     \
        static void init_functions() { functions_singleton<>::create( loaded_lib_handle( L#lib_name ) ); }          \
        static functions const & lib_functions() { return functions_singleton<>::lib_functions(); }                 \
    };                                                                                                              \
                                                                                                                    \
template <int dummy>                                                                                                \
__declspec( selectany )                                                                                             \
boost::optional<guard_class_name::functions> guard_class_name::functions_singleton<dummy>::functions_;              \
                                                                                                                    \
BOOST_PP_SEQ_FOR_EACH( BOOST_DELAYED_EXTERN_LIB_FUNCTION_IMPLEMENTATION, guard_class_name, functionSequence );      \
/**/


//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // extern_lib_hpp
