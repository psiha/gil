////////////////////////////////////////////////////////////////////////////////
///
/// \file memory_mapping.hpp
/// ------------------------
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
#ifndef memory_mapping_hpp__D9C84FF5_E506_4ECB_9778_61E036048D28
#define memory_mapping_hpp__D9C84FF5_E506_4ECB_9778_61E036048D28
//------------------------------------------------------------------------------
#include "boost/assert.hpp"
#include "boost/noncopyable.hpp"
#include "boost/range/iterator_range.hpp"

#ifndef _WIN32
    #include "fcntl.h"
#endif // _WIN32

#ifdef _WIN32
    #define BOOST_AUX_IO_WIN32_OR_POSIX( win32, posix ) win32
#else
    #define BOOST_AUX_IO_WIN32_OR_POSIX( win32, posix ) posix
#endif
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------

// Implementation note:
//   Using structs with public members and factory functions to enable (almost)
// zero-overhead 'link-time' conversion to native flag formats and to allow the
// user to modify the created flags or create fully custom ones so that specific
// platform-dependent use-cases, not otherwise covered through the generic
// interface, can also be covered.
//                                            (10.10.2010.) (Domagoj Saric)

struct file_flags
{
    struct handle_access_rights
    {
        static unsigned int const read   ;
        static unsigned int const write  ;
        static unsigned int const execute;
    };

    struct share_mode
    {
        static unsigned int const none  ;
        static unsigned int const read  ;
        static unsigned int const write ;
        static unsigned int const remove;
    };

    enum open_policy
    {
        create_new                      = BOOST_AUX_IO_WIN32_OR_POSIX( 1, O_CREAT | O_EXCL  ),
        create_new_or_truncate_existing = BOOST_AUX_IO_WIN32_OR_POSIX( 2, O_CREAT | O_TRUNC ),
        open_existing                   = BOOST_AUX_IO_WIN32_OR_POSIX( 3, 0                 ),
        open_or_create                  = BOOST_AUX_IO_WIN32_OR_POSIX( 4, O_CREAT           ),
        open_and_truncate_existing      = BOOST_AUX_IO_WIN32_OR_POSIX( 5, O_TRUNC           )
    };

    struct system_hints
    {
        static unsigned int const random_access    ;
        static unsigned int const sequential_access;
        static unsigned int const non_cached       ;
        static unsigned int const delete_on_close  ;
        static unsigned int const temporary        ;
    };

    struct on_construction_rights
    {
        static unsigned int const read   ;
        static unsigned int const write  ;
        static unsigned int const execute;
    };

    static file_flags create
    (
        unsigned int handle_access_flags   ,
        unsigned int share_mode            ,
        open_policy                        ,
        unsigned int system_hints          ,
        unsigned int on_construction_rights
    );

    static file_flags create_for_opening_existing_files
    (
        unsigned int handle_access_flags,
        unsigned int share_mode         ,
        bool         truncate           ,
        unsigned int system_hints
    );

#ifdef _WIN32
    unsigned long desired_access      ;
    unsigned long share_mode          ;
    unsigned long creation_disposition;
    unsigned long flags_and_attributes;
#else
    int oflag;
    int pmode;
#endif // _WIN32
};

struct mapping_flags
{
    struct handle_access_rights
    {
        static unsigned int const read   ;
        static unsigned int const write  ;
        static unsigned int const execute;
    };

    struct share_mode
    {
        static unsigned int const shared;
        static unsigned int const hidden;
    };

    struct system_hint
    {
        static unsigned int const strict_target_address  ;
        static unsigned int const lock_to_ram            ;
        static unsigned int const reserve_page_file_space;
        static unsigned int const precommit              ;
        static unsigned int const uninitialized          ;
    };

    static mapping_flags create
    (
        unsigned int handle_access_rights,
        unsigned int share_mode          ,
        unsigned int system_hints
    );

#ifdef _WIN32
    unsigned int create_mapping_flags;
    unsigned int map_view_flags;
#else
    int protection;
    int flags     ;
#endif // _WIN32
};

namespace guard
{
//------------------------------------------------------------------------------

#ifdef _WIN32
class windows_handle : noncopyable
{
public:
    typedef void * handle_t;

    explicit windows_handle( handle_t );
    ~windows_handle();

    handle_t const & handle() const;

private:
    handle_t const handle_;
};
#endif // _WIN32

class posix_handle : noncopyable
{
public:
    typedef int handle_t;

    explicit posix_handle( handle_t );

    #ifdef _WIN32
        explicit posix_handle( windows_handle::handle_t );
    #endif // _WIN32

    ~posix_handle();

    handle_t const & handle() const;

private:
    handle_t const handle_;
};


#ifdef _WIN32
    typedef windows_handle native_handle_guard;
#else
    typedef posix_handle   native_handle_guard;
#endif // _WIN32
typedef native_handle_guard::handle_t native_handle_t;


native_handle_guard create_file( char const * file_name, file_flags const &                            );
native_handle_guard create_file( char const * file_name, file_flags const &, unsigned int desired_size );

//------------------------------------------------------------------------------
} // namespace guard

bool        set_file_size( guard::native_handle_t, std::size_t desired_size );
std::size_t get_file_size( guard::native_handle_t                           );


typedef iterator_range<unsigned char const *> memory_chunk_t;//...zzz...this implicitly constructs from a string literal...
typedef iterator_range<unsigned char       *> writable_memory_chunk_t;


////////////////////////////////////////////////////////////////////////////////
///
/// \class memory_mapping
///
////////////////////////////////////////////////////////////////////////////////

class memory_mapping
    :
    private writable_memory_chunk_t
    #ifndef __clang__
        ,private boost::noncopyable
    #endif // __clang__
{
public:
    memory_mapping
    (
        guard::native_handle_t,
        mapping_flags const &,
        unsigned int desired_size
    );
    ~memory_mapping();

    operator writable_memory_chunk_t const & () const { return memory_range(); }

protected:
    writable_memory_chunk_t const & memory_range() const { return *this; }
};


////////////////////////////////////////////////////////////////////////////////
///
/// \class memory_mapped_source
///
////////////////////////////////////////////////////////////////////////////////

class memory_mapped_source : private memory_mapping
{
public:
    memory_mapped_source( guard::native_handle_t, unsigned int desired_size );

    operator memory_chunk_t const & () const
    {
        //writable_memory_chunk_t const & writable( *this );
        writable_memory_chunk_t const & writable( memory_mapping::memory_range()                       ); 
        memory_chunk_t          const & readonly( reinterpret_cast<memory_chunk_t const &>( writable ) );
        BOOST_ASSERT( readonly == writable );
        return readonly;
    }

protected:
    memory_chunk_t const & memory_range() const { return *this; }
};


memory_mapped_source map_read_only_file( char const * file_name );

//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // memory_mapping_hpp
