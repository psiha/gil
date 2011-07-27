////////////////////////////////////////////////////////////////////////////////
///
/// \file format_tags.hpp
/// ---------------------
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
#pragma once
#ifndef format_tags_hpp__0635BCB5_83C1_493B_A29C_944BA9F4EF26
#define format_tags_hpp__0635BCB5_83C1_493B_A29C_944BA9F4EF26
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
namespace boost
{
//------------------------------------------------------------------------------
namespace gil
{
//------------------------------------------------------------------------------

//...zzz...synchronize changes with writer implementations...
enum format_tag
{
    bmp,
    gif,
    jpeg,
    png,
    tiff,
    tga,

    number_of_known_formats
};

//struct bmp  {};
//struct gif  {};
//struct jpeg {};
//struct png  {};
//struct tga  {};
//struct tiff {};

//------------------------------------------------------------------------------
} // namespace gil
//------------------------------------------------------------------------------
} // namespace boost
//------------------------------------------------------------------------------
#endif // format_tags_hpp
