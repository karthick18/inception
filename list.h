#ifndef _LIST_H_
#define _LIST_H_

#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

struct list
{
    struct list *next;
    struct list **pprev;
};

struct list_head
{
    struct list *head;
    struct list **tail;
    int nodes;
};

#define LIST_DECLARE(l) \
    struct list_head l = { .head = NULL, .tail = &(l).head, .nodes = 0 }

static __inline__ void list_add(struct list *element, struct list_head *list)
{
    assert(element);
    assert(list);
    assert(list->tail);
    if( (element->next = list->head) )
        list->head->pprev = &element->next;
    else
        list->tail = &element->next;
    
    *(element->pprev = &list->head) = element;
    ++list->nodes;
}

static __inline__ void list_add_tail(struct list *element, struct list_head *list)
{
    assert(element);
    assert(list);
    assert(list->tail);
    element->next = NULL;
    *(element->pprev = list->tail) = element;
    list->tail =  &element->next;
    ++list->nodes;
}

static __inline__ void list_del(struct list *element, struct list_head *list)
{
    assert(element);
    assert(list);
    assert(list->tail);
    if(element->pprev)
    {
        if(element->next)
        {
            element->next->pprev = element->pprev;
        }
        else
        {
            list->tail = element->pprev;
        }
        *(element->pprev) = element->next;
        --list->nodes;
        assert(list->nodes >= 0);
        element->pprev = NULL;
        element->next =  NULL;
    }
}

static __inline__ void list_init(struct list_head *list)
{
    assert(list);
    list->head = NULL;
    list->tail = &list->head;
    list->nodes = 0;
}

#define LIST_ENTRY(element, cast, field)                                \
    (cast*) ( (unsigned char*)element - (unsigned long)&(((cast*)0)->field) )

#define list_destroy(list, cast, field, destroy_cb)         \
do                                                          \
{                                                           \
    __typeof__ ((destroy_cb)) *fun_ptr = &(destroy_cb);     \
    while((list)->head)                                     \
    {                                                       \
        cast *wl = LIST_ENTRY((list)->head, cast, field);   \
        list_del((list)->head, list);                       \
        if(fun_ptr) (*fun_ptr)(wl);                         \
    }                                                       \
    assert((list)->nodes == 0);                             \
} while(0)

#define list_dump(list, cast, field, display_cb)                        \
do                                                                      \
{                                                                       \
    if((list)->nodes)                                                   \
    {                                                                   \
        int __index = 0;                                                \
        __typeof__((list)->head) iter =  NULL;                          \
        __typeof__((display_cb)) *fun_ptr = &(display_cb);              \
        do_log("\nList dump for [%d] words\n", (list)->nodes);          \
        do_log("---------------------------\n");                        \
        for(iter = (list)->head; iter; iter = iter->next, ++__index)    \
        {                                                               \
            cast *wl = LIST_ENTRY(iter, cast, field);                   \
            if(fun_ptr) (*fun_ptr)(wl, __index);                        \
        }                                                               \
        do_log("\n---------------------------\n");                      \
    }                                                                   \
}while(0)

#define DUP_LIST_CB(element, list, cast, field) ({  \
    list_add_tail(&element->field, list);           \
})

#define dup_list(src, dst, cast, field)             \
do                                                  \
{                                                   \
    register __typeof__(src) iter;                  \
    LIST_DECLARE(temp_list);                        \
    for(iter = src; iter; iter = iter->next)        \
    {                                               \
        cast *wl = LIST_ENTRY(iter, cast, field);   \
        DUP_LIST_CB(wl, &temp_list, cast, field);   \
    }                                               \
    (dst) = temp_list.head;                         \
}while(0)

#ifdef __cplusplus
}
#endif

#endif
