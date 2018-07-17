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
static inline const char* type##ToStr(int x) {switch(x){MACRO_FOR_EACH(_CASE_LINE,args)default: return "invalid";}}\
_ENUM_CAST(type)

#else

/* Using ISO C variadic macros */

#define _NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, N, ...) N

#define _fe_0(_call, ...)
#define _fe_1(_call, x) _call(x)
#define _fe_2(_call, x, ...) _call(x) _fe_1(_call, __VA_ARGS__)
#define _fe_3(_call, x, ...) _call(x) _fe_2(_call, __VA_ARGS__)
#define _fe_4(_call, x, ...) _call(x) _fe_3(_call, __VA_ARGS__)
#define _fe_5(_call, x, ...) _call(x) _fe_4(_call, __VA_ARGS__)
#define _fe_6(_call, x, ...) _call(x) _fe_5(_call, __VA_ARGS__)
#define _fe_7(_call, x, ...) _call(x) _fe_6(_call, __VA_ARGS__)
#define _fe_8(_call, x, ...) _call(x) _fe_7(_call, __VA_ARGS__)
#define _fe_9(_call, x, ...) _call(x) _fe_8(_call, __VA_ARGS__)
#define _fe_10(_call, x, ...) _call(x) _fe_9(_call, __VA_ARGS__)

#define MACRO_FOR_EACH(x, ...) \
    _NTH_ARG(_, ##__VA_ARGS__, \
        _fe_10, _fe_9, _fe_8, _fe_7, _fe_6, _fe_5, _fe_4, _fe_3, _fe_2, _fe_1, _fe_0) \
        (x, ##__VA_ARGS__)

/* Enum to string magic */

#define ENUM(type,...) \
enum type { __VA_ARGS__ }; \
static inline const char* type##ToStr(int x) {switch(x){MACRO_FOR_EACH(_CASE_LINE,__VA_ARGS__) default: return "invalid";}} \
_ENUM_CAST(type)
#endif

#define _CASE_LINE(x) case x: return #x;
#ifdef __cplusplus
#define _ENUM_CAST(type) ; static inline const char* toStr(type x) {return type##ToStr(x);}
#else
#define _ENUM_CAST(type)
#endif

#endif
