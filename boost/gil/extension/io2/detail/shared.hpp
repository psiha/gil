////////////////////////////////////////////////////////////////////////////////
///
/// \file shared.hpp
/// ----------------
///
/// Common functionality for all GIL::IO backends.
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
#ifndef shared_hpp__DA4F9174_EBAA_43E8_BEDD_A273BBA88CE7
#define shared_hpp__DA4F9174_EBAA_43E8_BEDD_A273BBA88CE7
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------
namespace detail
{
//------------------------------------------------------------------------------

#ifndef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    #ifdef _MSC_VER
        #define BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    #endif
#endif

#ifdef BOOST_GIL_THROW_THROUGH_C_SUPPORTED
    #define BOOST_GIL_CAN_THROW throw(...)
#else
    #define BOOST_GIL_CAN_THROW
#endif


////////////////////////////////////////////////////////////////////////////////
///
/// \class cumulative_result
///
////////////////////////////////////////////////////////////////////////////////

class cumulative_result
{
public:
    cumulative_result() : result_( true ) { mark_as_inspected(); }

    void accumulate( bool const new_result )
    {
        // Catch the error early when debugging...
        BOOST_ASSERT( new_result );
        result_ &= new_result;
        mark_as_dirty();
    }

    template <typename T1, typename T2>
    void accumulate_equal    ( T1 const new_result, T2 const desired_result   ) { accumulate( new_result == desired_result   ); }
    template <typename T>
    void accumulate_different( T  const new_result, T  const undesired_result ) { accumulate( new_result != undesired_result ); }
    template <typename T1, typename T2>
    void accumulate_greater  ( T1 const new_result, T2 const threshold        ) { accumulate( new_result > threshold         ); }

    bool failed() const
    {
        mark_as_inspected();
        return !result_;
    }

    void throw_if_error( char const * const description ) const
    {
        mark_as_inspected();
        io_error_if_not( result_, description );
    }

private:
    bool result_;

    // correct usage debugging section
#ifdef _DEBUG
public:
    ~cumulative_result() { BOOST_ASSERT( inspected_ ); }
private:
    void mark_as_dirty    () const { inspected_ = false; }
    void mark_as_inspected() const { inspected_ = true ; }
    mutable bool inspected_;
#else
    void mark_as_dirty    () const {}
    void mark_as_inspected() const {}
#endif // _DEBUG
};

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // shared_hpp
