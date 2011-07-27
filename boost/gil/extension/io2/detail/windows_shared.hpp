////////////////////////////////////////////////////////////////////////////////
///
/// \file windows_shared.hpp
/// ------------------------
///
///   Common functionality for MS Windows based backends.
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
#ifndef windows_shared_hpp__9337A434_D2F6_43F2_93C8_4CE66C07B74D
#define windows_shared_hpp__9337A434_D2F6_43F2_93C8_4CE66C07B74D
//------------------------------------------------------------------------------
#include "windows_shared_istreams.hpp"

#include "boost/array.hpp"
#include "boost/assert.hpp"

#include <cstring>
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
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
///
/// \class wide_path
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
// - GP and WIC want wide-char paths
// - we received a narrow-char path
// - we are using GP or WIC which means we are also using Windows
// - on Windows a narrow-char path can only be up to MAX_PATH in length:
//  http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#maxpath
// - it is therefore safe to use a fixed sized/stack buffer....
//                                            (24.07.2010.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

class wide_path
{
public:
    explicit wide_path( char const * const p_file_name )
    {
        BOOST_ASSERT( p_file_name );
        BOOST_ASSERT( std::strlen( p_file_name ) < wide_file_name_.size() );
        char    const * pSource     ( p_file_name             );
        wchar_t       * pDestination( wide_file_name_.begin() );
        do
        {
            *pDestination++ = *pSource;
        } while ( *pSource++ );
        BOOST_ASSERT( pDestination < wide_file_name_.end() );
    }

    operator wchar_t const * () const { return wide_file_name_.begin(); }

private:
    boost::array<wchar_t, MAX_PATH> wide_file_name_;
};


#pragma warning( push )
#pragma warning( disable : 4355 ) // 'this' used in base member initializer list.

#pragma warning( pop )

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace io
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // windows_shared_hpp
