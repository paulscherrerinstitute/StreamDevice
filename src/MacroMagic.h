/*************************************************************************
* This header provides macros for enum to string conversions.
* Please see ../docs/ for detailed documentation.
*
* (C) 2018 Dirk Zimoch (dirk.zimoch@psi.ch)
*
* This file is part of StreamDevice.
*
* StreamDevice is free software: You can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* StreamDevice is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with StreamDevice. If not, see https://www.gnu.org/licenses/.
*************************************************************************/

#ifndef _MacroMagic_h
#define _MacroMagic_h

#if defined __GNUC__ && __GNUC__ < 3

/* Using old GCC variadic macros */

#define _NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, N, args...) N

#define _fe_0(_call, args...)
#define _fe_1(_call, x) _call(x)
#define _fe_2(_call, x, args...) _call(x) _fe_1(_call, args)
#define _fe_3(_call, x, args...) _call(x) _fe_2(_call, args)
#define _fe_4(_call, x, args...) _call(x) _fe_3(_call, args)
#define _fe_5(_call, x, args...) _call(x) _fe_4(_call, args)
#define _fe_6(_call, x, args...) _call(x) _fe_5(_call, args)
#define _fe_7(_call, x, args...) _call(x) _fe_6(_call, args)
#define _fe_8(_call, x, args...) _call(x) _fe_7(_call, args)
#define _fe_9(_call, x, args...) _call(x) _fe_8(_call, args)
#define _fe_10(_call, x, args...) _call(x) _fe_9(_call, args)

#define MACRO_FOR_EACH(x, args...) \
    _NTH_ARG(_, ##args, \
        _fe_10, _fe_9, _fe_8, _fe_7, _fe_6, _fe_5, _fe_4, _fe_3, _fe_2, _fe_1, _fe_0) \
        (x, ##args)

/* Enum to string magic */

#define ENUM(type, args...) \
enum type { args }; \
static inline const char* type##ToStr(int x) {switch(x) {MACRO_FOR_EACH(_CASE_LINE,args)default: return "invalid";}}\
_ENUM_CAST(type)

#else

/* Using ISO C variadic macros */

#define _NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, N, ...) N

/*
 * we need to use _EXPAND below to work around a problem in Visual Studio 2010
 * where __VA_ARGS__ is treated as a single argument
 * See https://stackoverflow.com/questions/5134523/msvc-doesnt-expand-va-args-correctly
 */
#define _EXPAND( x ) x

#define _fe_0(_call, ...)
#define _fe_1(_call, x) _call(x)
#define _fe_2(_call, x, ...) _call(x) _EXPAND(_fe_1(_call, __VA_ARGS__))
#define _fe_3(_call, x, ...) _call(x) _EXPAND(_fe_2(_call, __VA_ARGS__))
#define _fe_4(_call, x, ...) _call(x) _EXPAND(_fe_3(_call, __VA_ARGS__))
#define _fe_5(_call, x, ...) _call(x) _EXPAND(_fe_4(_call, __VA_ARGS__))
#define _fe_6(_call, x, ...) _call(x) _EXPAND(_fe_5(_call, __VA_ARGS__))
#define _fe_7(_call, x, ...) _call(x) _EXPAND(_fe_6(_call, __VA_ARGS__))
#define _fe_8(_call, x, ...) _call(x) _EXPAND(_fe_7(_call, __VA_ARGS__))
#define _fe_9(_call, x, ...) _call(x) _EXPAND(_fe_8(_call, __VA_ARGS__))
#define _fe_10(_call, x, ...) _call(x) _EXPAND(_fe_9(_call, __VA_ARGS__))

#define MACRO_FOR_EACH(x, ...) \
    _EXPAND(_NTH_ARG(_, ##__VA_ARGS__, \
        _fe_10, _fe_9, _fe_8, _fe_7, _fe_6, _fe_5, _fe_4, _fe_3, _fe_2, _fe_1, _fe_0) \
        (x, ##__VA_ARGS__))

/* Enum to string magic */

#define ENUM(type,...) \
enum type { __VA_ARGS__ }; \
static inline const char* type##ToStr(int x) {switch(x) {_EXPAND(MACRO_FOR_EACH(_CASE_LINE,__VA_ARGS__)) default: return "invalid";}} \
_ENUM_CAST(type)
#endif

#define _CASE_LINE(x) case x: return #x;
#ifdef __cplusplus
#define _ENUM_CAST(type) ; static inline const char* toStr(type x) {return type##ToStr(x);}
#else
#define _ENUM_CAST(type)
#endif

#endif
