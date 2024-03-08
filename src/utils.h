#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "stb_ds.h"

// taken from [https://github.com/gingerBill/gb]

typedef uint8_t   u8;
typedef  int8_t   i8;
typedef uint16_t u16;
typedef  int16_t i16;
typedef uint32_t u32;
typedef  int32_t i32;
typedef uint64_t u64;
typedef  int64_t i64;

typedef float  f32;
typedef double f64;

typedef size_t    usize;
typedef ptrdiff_t isize;

static_assert(sizeof(u8) == sizeof(i8), "type check");
static_assert(sizeof(u16) == sizeof(i16), "type check");
static_assert(sizeof(u32) == sizeof(i32), "type check");
static_assert(sizeof(u64) == sizeof(i64), "type check");

static_assert(sizeof(u8) == 1, "type check");
static_assert(sizeof(u16) == 2, "type check");
static_assert(sizeof(u32) == 4, "type check");
static_assert(sizeof(u64) == 8, "type check");

static_assert(sizeof(f32) == 4, "type check");
static_assert(sizeof(f64) == 8, "type check");

static_assert(sizeof(usize) == sizeof(isize), "type check");

#define U8_MIN 0u
#define U8_MAX 0xffu
#define I8_MIN (-0x7f - 1)
#define I8_MAX 0x7f

#define U16_MIN 0u
#define U16_MAX 0xffffu
#define I16_MIN (-0x7fff - 1)
#define I16_MAX 0x7fff

#define U32_MIN 0u
#define U32_MAX 0xffffffffu
#define I32_MIN (-0x7fffffff - 1)
#define I32_MAX 0x7fffffff

#define U64_MIN 0ull
#define U64_MAX 0xffffffffffffffffull
#define I64_MIN (-0x7fffffffffffffffll - 1)
#define I64_MAX 0x7fffffffffffffffll

#define F32_MIN 1.17549435e-38f
#define F32_MAX 3.40282347e+38f

#define F64_MIN 2.2250738585072014e-308
#define F64_MAX 1.7976931348623157e+308

#ifndef NULL
#if defined(__cplusplus)
#if __cplusplus >= 201103L
#define NULL nullptr
#else
#define NULL 0
#endif
#else
#define NULL ((void *)0)
#endif
#endif

#define println(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define local static


////////////////////////////////////////////////////////////////
//
// Defer statement
// Akin to D's SCOPE_EXIT or
// similar to Go's defer but scope-based
//
// NOTE: C++11 (and above) only!
//
//extern "C++" {
//	template <typename T> struct gbRemoveReference       { typedef T Type; };
//	template <typename T> struct gbRemoveReference<T &>  { typedef T Type; };
//	template <typename T> struct gbRemoveReference<T &&> { typedef T Type; };
//
//	template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &t)  { return static_cast<T &&>(t); }
//	template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &&t) { return static_cast<T &&>(t); }
//	template <typename T> inline T &&gb_move   (T &&t)                                   { return static_cast<typename gbRemoveReference<T>::Type &&>(t); }
//	template <typename F>
//	struct gbprivDefer {
//		F f;
//		gbprivDefer(F &&f) : f(gb_forward<F>(f)) {}
//		~gbprivDefer() { f(); }
//	};
//	template <typename F> gbprivDefer<F> gb__defer_func(F &&f) { return gbprivDefer<F>(gb_forward<F>(f)); }
//
//	#define DEFER_1(x, y) x##y
//	#define DEFER_2(x, y) DEFER_1(x, y)
//	#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
//	#define defer(code)      auto DEFER_3(_defer_) = gb__defer_func([&]()->void{code;})
//}


////////////////////////////////////////////////////////////////
//
// Macro Fun!
//
//

#ifndef JOIN_MACROS
#define JOIN_MACROS
#define JOIN2_IND(a, b) a##b

#define JOIN2(a, b)       JOIN2_IND(a, b)
#define JOIN3(a, b, c)    JOIN2(JOIN2(a, b), c)
#define JOIN4(a, b, c, d) JOIN2(JOIN2(JOIN2(a, b), c), d)
#endif

// from [boost/current_function.hpp](https://www.boost.org/doc/libs/1_62_0/boost/current_function.hpp)

#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)

# define BOOST_CURRENT_FUNCTION __PRETTY_FUNCTION__

#elif defined(__DMC__) && (__DMC__ >= 0x810)

# define BOOST_CURRENT_FUNCTION __PRETTY_FUNCTION__

#elif defined(__FUNCSIG__)

# define BOOST_CURRENT_FUNCTION __FUNCSIG__

#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))

# define BOOST_CURRENT_FUNCTION __FUNCTION__

#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)

# define BOOST_CURRENT_FUNCTION __FUNC__

#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)

# define BOOST_CURRENT_FUNCTION __func__

#elif defined(__cplusplus) && (__cplusplus >= 201103)

# define BOOST_CURRENT_FUNCTION __func__

#else

# define BOOST_CURRENT_FUNCTION "(unknown)"

#endif


////////////////////////////////////////////////////////////////
//
// Debug
//
//


#ifndef DEBUG_TRAP
#if defined(_MSC_VER)
#if _MSC_VER < 1300
#define DEBUG_TRAP() __asm int 3 /* Trap to debugger! */
#else
#define DEBUG_TRAP() __debugbreak()
#endif
#else
#define DEBUG_TRAP() abort()
#endif
#endif

#ifndef ASSERT_MSG
#define ASSERT_MSG(cond, msg, ...) do { \
	if (!(cond)) { \
		gb_assert_handler("Assertion Failure", #cond, __FILE__, BOOST_CURRENT_FUNCTION, (i64)__LINE__, msg, ##__VA_ARGS__); \
		DEBUG_TRAP(); \
	} \
} while (0)
#endif

#ifndef ASSERT
#define ASSERT(cond) ASSERT_MSG(cond, NULL)
#endif

#ifndef ASSERT_NOT_NULL
#define ASSERT_NOT_NULL(ptr) ASSERT_MSG((ptr) != NULL, #ptr " must not be NULL")
#endif

#ifndef PANIC
#define PANIC(msg, ...) do { \
	gb_assert_handler("Panic", NULL, __FILE__, BOOST_CURRENT_FUNCTION, (i64)__LINE__, msg, ##__VA_ARGS__); \
	DEBUG_TRAP(); \
} while (0)
#endif

#ifndef TODO
#define TODO do { \
	gb_assert_handler("Panic", NULL, __FILE__, BOOST_CURRENT_FUNCTION, (i64)__LINE__, "not yet implemented"); \
	DEBUG_TRAP(); \
} while (0)
#endif

static void gb_assert_handler(char const *prefix, char const *condition, char const *file, char const *function, i32 line, char const *msg, ...) {
	fprintf(stderr, "%s::%s::(%d)::\n%s:", file, function, line, prefix);
	if (condition)
		fprintf(stderr, "`%s` ", condition);
	if (msg) {
		va_list va;
		va_start(va, msg);
		vfprintf(stderr, msg, va);
		va_end(va);
	}
	fprintf(stderr, "\n");
}

