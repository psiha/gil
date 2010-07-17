#ifndef libx_shared_h__
#define libx_shared_h__
//------------------------------------------------------------------------------
#include "../../../utilities.hpp"

#include "crtdefs.h"
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

class generic_vertical_roi
{
public:
    typedef std::ptrdiff_t     value_type;
    typedef point2<value_type> point_t   ;

    typedef value_type         offset_t  ;

public:
    generic_vertical_roi( value_type const start_row, value_type const end_row )
        :
        start_row_( start_row ),
        end_row_  ( end_row   )
    {}

    value_type start_row() const { return start_row_; }
    value_type end_row  () const { return end_row_  ; }

private:
    void operator=( generic_vertical_roi const & );

private:
    value_type const start_row_;
    value_type const   end_row_;
};

//------------------------------------------------------------------------------
} // namespace detail
//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------

#endif // libx_shared_h__