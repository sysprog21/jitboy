#ifndef __G_LIST_H__
#define __G_LIST_H__

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef void *gpointer;
typedef void (*GFunc)(gpointer data, gpointer user_data);

typedef struct _GList {
    gpointer data;
    struct _GList *next, *prev;
} GList;

#if defined(__GNUC__) || defined(__clang__)
#define _G_NEW(struct_type, n_structs, func)          \
    (struct_type *) (__extension__({                  \
        size_t __n = (size_t)(n_structs);             \
        size_t __s = sizeof(struct_type);             \
        gpointer __p;                                 \
        if (__s == 1)                                 \
            __p = g_##func(__n);                      \
        else if (__builtin_constant_p(__n) &&         \
                 (__s == 0 || __n <= SIZE_MAX / __s)) \
            __p = g_##func(__n * __s);                \
        else                                          \
            __p = g_##func##_n(__n, __s);             \
        __p;                                          \
    }))
#else
#define _G_NEW(struct_type, n_structs, func) \
    ((struct_type *) g_##func##_n((n_structs), sizeof(struct_type)))
#endif
#define g_new(struct_type, n_structs) _G_NEW(struct_type, n_structs, malloc)

#if defined(__GNUC__) || defined(__clang__)
#define _G_BOOLEAN_EXPR(expr)    \
    __extension__({              \
        int _g_boolean_var_;     \
        if (expr)                \
            _g_boolean_var_ = 1; \
        else                     \
            _g_boolean_var_ = 0; \
        _g_boolean_var_;         \
    })
#define G_LIKELY(expr) (__builtin_expect(_G_BOOLEAN_EXPR(expr), 1))
#define G_UNLIKELY(expr) (__builtin_expect(_G_BOOLEAN_EXPR(expr), 0))
#else
#define G_LIKELY(expr) (expr)
#define G_UNLIKELY(expr) (expr)
#endif

#define SIZE_OVERFLOWS(a, b) (G_UNLIKELY((b) > 0 && (a) > SIZE_MAX / (b)))

static inline gpointer g_malloc(size_t n_bytes)
{
    if (G_LIKELY(n_bytes)) {
        gpointer mem = malloc(n_bytes);
        if (mem)
            return mem;
    }
    return NULL;
}

static inline gpointer g_malloc_n(size_t n_blocks, size_t n_block_bytes)
{
    assert(!SIZE_OVERFLOWS(n_blocks, n_block_bytes));
    return g_malloc(n_blocks * n_block_bytes);
}

static inline void g_free(gpointer mem)
{
    free(mem);
}

#define g_slice_new(type) ((type *) g_malloc(sizeof(type)))
#define _g_list_alloc() g_slice_new(GList)

static inline GList *g_list_reverse(GList *list)
{
    GList *last = NULL;
    while (list) {
        last = list;
        list = last->next;
        last->next = last->prev;
        last->prev = list;
    }

    return last;
}

static inline GList *g_list_prepend(GList *list, gpointer data)
{
    GList *new_list = malloc(sizeof(GList));
    new_list->data = data;
    new_list->next = list;

    if (list) {
        new_list->prev = list->prev;
        if (list->prev)
            list->prev->next = new_list;
        list->prev = new_list;
    } else
        new_list->prev = NULL;

    return new_list;
}

static inline void g_list_foreach(GList *list, GFunc func, gpointer user_data)
{
    while (list) {
        GList *next = list->next;
        (*func)(list->data, user_data);
        list = next;
    }
}

#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#define G_STRUCT_OFFSET(struct_type, member) \
    ((long) offsetof(struct_type, member))
#else
#define G_STRUCT_OFFSET(struct_type, member) \
    ((long) ((uint8_t *) &((struct_type *) 0)->member))
#endif

#define g_slice_free_chain(type, mem_chain, next)                    \
    do {                                                             \
        g_slice_free_chain_with_offset(sizeof(type), (mem_chain),    \
                                       G_STRUCT_OFFSET(type, next)); \
    } while (0)

static inline void g_slice_free_chain_with_offset(size_t mem_size,
                                                  gpointer mem_chain,
                                                  size_t next_offset)
{
    gpointer slice = mem_chain;
    while (slice) {
        uint8_t *current = slice;
        slice = *(gpointer *) (current + next_offset);
        free(current);
    }
}

static inline void g_list_free(GList *list)
{
    g_slice_free_chain(GList, list, next);
}

static inline void g_list_free_full(GList *list, void(*free_func))
{
    g_list_foreach(list, free_func, NULL);
    g_list_free(list);
}

static inline GList *_g_list_remove_link(GList *list, GList *link)
{
    if (!link)
        return list;

    if ((link->prev) && (link->prev->next == link))
        link->prev->next = link->next;
    if ((link->next) && (link->next->prev == link))
        link->next->prev = link->prev;

    if (link == list)
        list = list->next;

    link->next = link->prev = NULL;

    return list;
}

static inline GList *g_list_delete_link(GList *list, GList *link_)
{
    list = _g_list_remove_link(list, link_);
    free(link_);
    return list;
}

#endif
