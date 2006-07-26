
#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#define BUFFER_LENGTH    8192
#define FILENAME_LENGTH  4096

#define MALLOC(size) ({ \
    void *__var; \
    if (!(__var = malloc(size))) { \
        PFERROR("malloc(%d)", size); \
        exit(2); \
    } \
    __var; \
})

#define CALLOC(nb, size) ({ \
    void *__var; \
    if (!(__var = calloc(nb, size))) { \
        PFERROR("calloc(%d, %d)", nb, size); \
        exit(2); \
    } \
    __var; \
})

#define FREE(ptr)    free(ptr)

#define STRDUP(str)  ({ \
    char *__var; \
    if (!(__var = strdup(str))) { \
        PFERROR("strdup(%s)", str); \
        exit(2); \
    } \
    __var; \
})

#define GET_UMASK()    ({mode_t __mask = umask(0); umask(__mask); __mask; })


#define IS(v, f)     (((v) & (f)) == (f))
#define SET(v, f)    ((v) |= (f))
#define UNSET(v, f)  ((v) &= ~(f))


#ifndef __USE_BSD
typedef unsigned int    u_int;
typedef unsigned char   u_char;
#endif


#define BUFFLEN BUFFER_LENGTH

#define PFERROR(str...)    PFERROR2(##str, 0)
#define PFERROR2(str, p...)  pferror("%s:%d: " str, __FILE__, __LINE__, ##p)
#define HEXTODEC(a)  (('0'<=(a) && (a)<='9')?(a)-'0':(a)-'a'+10)


#ifdef  MEM_OPTIM
/*
 * Returns a pointer to type <type> taken from the
 * pool <pool_type> or dynamically allocated. In the
 * first case, <pool_type> is updated to point to the
 * next element in the list.
 */
#define POOL_ALLOC(type) ({                     \
    void *p;                                    \
    if ((p = pool_##type) == NULL)              \
        p = malloc(sizeof(##type));              \
    else {                                      \
        pool_##type = *(void **)pool_##type;    \
    }                                           \
    p;                                          \
})

/*
 * Puts a memory area back to the corresponding pool.
 * Items are chained directly through a pointer that
 * is written in the beginning of the memory area, so
 * there's no need for any carrier cell. This implies
 * that each memory area is at least as big as one
 * pointer.
 */
#define POOL_FREE(type, ptr) ({                         \
    *(void **)ptr = (void *)pool_##type;                \
    pool_##type = (void *)ptr;                          \
})
#define POOL_INIT(type)       type  *pool_##type = NULL
#define POOL_INIT_PROTO(type) extern type *pool_##type

#else
#define POOL_ALLOC(type) (calloc(1,sizeof(##type)));
#define POOL_FREE(type, ptr) (free(ptr));
#define POOL_INIT        
#endif  /* MEM_OPTIM */


typedef void    *p2void[2];

/* initialise memory pool for list managing */

#define PUSH_STR_SORTED(ptr, str) (ptr = push_str_sorted(ptr, str))

/* unusable!!!
 * #define SIMPLE_LIST_FOREACH(list, data, current, next) \
 *    for(current = list; \
 *        (next = SIMPLE_LIST_NEXT(current), data = SIMPLE_LIST_PTR(current), current); \
 *	current = next)
 */

#define SIMPLE_LIST_FLUSH(list)  ( { \
  while (list) SIMPLE_LIST_POP(list); \
})

#define SIMPLE_LIST_NEXT_PTR(list)  (SIMPLE_LIST_PTR(SIMPLE_LIST_NEXT(list)))
#define SIMPLE_LIST_NEXT(list)      (*(((void**)(list))))
#define SIMPLE_LIST_PTR(list)       (*(((void**)(list)) + 1))
#define SIMPLE_LIST_INIT() POOL_INIT(p2void)

#define SIMPLE_LIST_PUSH(list, elem) ( { \
  void **__new; \
  __new = POOL_ALLOC(p2void); \
  __new[0] = (void*)(list); \
  __new[1] = (void*)(elem); \
  (list) = (void *)__new; \
})

#define SIMPLE_LIST_FILE(list, elem) ( { \
  void **__next = list; \
  if (!list) { \
    __next = POOL_ALLOC(p2void); \
    (list) = (void*)__next; \
  } else { \
    while (SIMPLE_LIST_NEXT(__next)) __next = SIMPLE_LIST_NEXT(__next); \
    SIMPLE_LIST_NEXT(__next) = POOL_ALLOC(p2void); \
    __next = SIMPLE_LIST_NEXT(__next); \
  } \
  __next[0] = NULL; \
  __next[1] = (void*)(elem); \
} )


#define SIMPLE_LIST_DEFILE(list) SIMPLE_LIST_POP(list)
#define SIMPLE_LIST_POP(list) ( { \
  void **__old = (void **)list, *__elem = NULL; \
  if (list) { \
    list = __old[0]; \
    __elem = __old[1]; \
    POOL_FREE(p2void, __old); \
  } \
  __elem ; \
})


#define LIST_CHAIN(pprev, pnew, pnext)  ({ \
    (pnew)->prev = pprev; \
    (pnew)->next = pnext; \
    (pnew)->prev->next = pnew; \
    (pnew)->next->prev = pnew; \
})

#define LIST_UNCHAIN(pold) ({ \
    (pold)->prev->next = (pold)->next; \
    (pold)->next->prev = (pold)->prev; \
})


POOL_INIT_PROTO(p2void);

char *right(mode_t mode);
char *dirname(char *);
char *basename(char *);
int  error(char *,...);
void warning(char *,...);
int  fatal_error(char *,...);
int  pferror(char *,...);
char *backslashed_strchr(char *s, char c);
char *backslashed_strmchr(char *s, char *mc);
char *backslashed_str(char *, char *toback);
void *push_str_sorted(void *ptr, char *str);

#endif /* __UTILS_H__ */
