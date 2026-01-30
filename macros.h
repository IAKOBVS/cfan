#ifndef MACROS_H
#	define MACROS_H 1

#	include <stdio.h>
#	include <errno.h>

#	ifdef DEBUG
#		define DBG(x) (x)
#	else
#		define DBG(x)
#	endif

#	ifdef __ASSERT_FUNCTION
#		define ASSERT_FUNC __ASSERT_FUNCTION
#	else
#		define ASSERT_FUNC __func__
#	endif

#	define MAX(x, y)    (((x) > (y)) ? (x) : (y))
#	define MIN(x, y)    (((x) < (y)) ? (x) : (y))
#	define LEN(X)       (sizeof(X) / sizeof(X[0]))
#	define S_LITERAL(s) s, S_LEN(s)
#	define S_LEN(s)     (sizeof(s) - 1)

#	ifdef __ASSERT_FUNCTION
#		define ASSERT_FUNC __ASSERT_FUNCTION
#	else
#		define ASSERT_FUNC __func__
#	endif

#	define DIE(x)                                                                  \
		do {                                                                    \
			if (errno)                                                      \
				perror("");                                             \
			fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, ASSERT_FUNC); \
			abort();                                                        \
			x;                                                              \
		} while (0)

#	define DIE_GRACEFUL(x)                                                         \
		do {                                                                    \
			if (errno)                                                      \
				perror("");                                             \
			fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, ASSERT_FUNC); \
			c_exit(EXIT_FAILURE);                                           \
			x;                                                              \
		} while (0)

#	ifdef __glibc_has_builtin
#		define HAS_BUILTIN(name) __glibc_has_builtin(name)
#	elif defined __has_builtin
#		define HAS_BUILTIN(name) __has_builtin(name)
#	else
#		define HAS_BUILTIN(name) 0
#	endif /* has_builtin */

#	if defined __glibc_unlikely && defined __glibc_likely
#		define likely(x)   __glibc_likely(x)
#		define unlikely(x) __glibc_unlikely(x)
#	elif ((defined __GNUC__ && (__GNUC__ >= 3)) || defined __clang__) && HAS_BUILTIN(__builtin_expect)
#		define likely(x)   __builtin_expect((x), 1)
#		define unlikely(x) __builtin_expect((x), 0)
#	else
#		define likely(x)   (x)
#		define unlikely(x) (x)
#	endif /* unlikely */

#	if defined _POSIX_C_SOURCE && _POSIX_C_SOURCE >= 199309L
#		define HAVE_NANOSLEEP 1
#	else
#		define HAVE_NANOSLEEP 0
#	endif /* Posix 199309L */

#	ifdef __glibc_has_attribute
#		define HAS_ATTRIBUTE(attr) __glibc_has_attribute(attr)
#	elif defined __has_attribute
#		define HAS_ATTRIBUTE(attr) __has_attribute(attr)
#	else
#		define HAS_ATTRIBUTE(attr) 0
#	endif /* has_attribute */

#	ifndef ATTR_INLINE_
#		ifdef __inline
#			define ATTR_INLINE_ __inline
#		elif (defined __cplusplus || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L))
#			define ATTR_INLINE_ inline
#		else
#			define ATTR_INLINE_
#		endif
#	endif

#	ifdef __always_inline
#		define ATTR_INLINE __always_inline
#	elif HAS_ATTRIBUTE(__always_inline__)
#		define ATTR_INLINE __attribute__((__always_inline__)) ATTR_INLINE_
#	else
#		define ATTR_INLINE
#	endif

#endif /* MACROS_H */
