/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

 *********************************************************************/
#define MAX_BLOCK_SIZE 1600
#define MAX_BLOCK_COUNT 16

#define DECLARE_HEAP(type, orderby) \
    struct heap_ ## type {   \
        uint32_t size;    \
        uint32_t n;       \
        type *top[MAX_BLOCK_COUNT];        \
    }; \
    typedef struct heap_ ## type heap_ ## type; \
    static inline type* heap_get_element(struct heap_ ## type *heap, uint32_t idx) \
    { \
        uint32_t elements_per_block = MAX_BLOCK_SIZE/sizeof(type); \
        return &heap->top[idx/elements_per_block][idx%elements_per_block];\
    } \
    static inline int8_t heap_increase_size(struct heap_ ## type *heap) \
    {\
        type *newTop; \
        uint32_t elements_per_block = MAX_BLOCK_SIZE/sizeof(type); \
        uint32_t elements = (heap->n + 1)%elements_per_block;\
        elements = elements?elements:elements_per_block;\
        if (heap->n+1 > elements_per_block * MAX_BLOCK_COUNT){\
            return -1;\
        }\
        newTop = PICO_ZALLOC(elements*sizeof(type)); \
        if(!newTop) { \
            return -1; \
        } \
        if (heap->top[heap->n/elements_per_block])  { \
            memcpy(newTop, heap->top[heap->n/elements_per_block], (elements - 1) * sizeof(type)); \
            PICO_FREE(heap->top[heap->n/elements_per_block]); \
        } \
        heap->top[heap->n/elements_per_block] = newTop;             \
        heap->size++;                                                               \
        return 0;                                                               \
    }\
    static inline int heap_insert(struct heap_ ## type *heap, type * el) \
    { \
        type *half;                                                                 \
        uint32_t i; \
        if (++heap->n >= heap->size) {                                                \
            if (heap_increase_size(heap)){                                                    \
                heap->n--;                                                           \
                return -1;                                                           \
            }                                                                       \
        }                                                                             \
        if (heap->n == 1) {                                                       \
            memcpy(heap_get_element(heap, 1), el, sizeof(type));                                    \
            return 0;                                                                   \
        }                                                                             \
        i = heap->n;                                                                    \
        half = heap_get_element(heap, i/2);                                                   \
        while ( (i > 1) && (half->orderby > el->orderby) ) {        \
            memcpy(heap_get_element(heap, i), heap_get_element(heap, i / 2), sizeof(type));                     \
            i /= 2;                                                                     \
            half = heap_get_element(heap, i/2);                                                   \
        }             \
        memcpy(heap_get_element(heap, i), el, sizeof(type));                                      \
        return 0;                                                                     \
    } \
    static inline int heap_peek(struct heap_ ## type *heap, type * first) \
    { \
        type *last;           \
        type *left_child;           \
        type *right_child;           \
        uint32_t i, child;        \
        if(heap->n == 0) {    \
            return -1;          \
        }                     \
        memcpy(first, heap_get_element(heap, 1), sizeof(type));   \
        last = heap_get_element(heap, heap->n--);                 \
        for(i = 1; (i * 2u) <= heap->n; i = child) {   \
            child = 2u * i;                              \
            right_child = heap_get_element(heap, child+1);     \
            left_child = heap_get_element(heap, child);      \
            if ((child != heap->n) &&                   \
                (right_child->orderby          \
                < left_child->orderby))           \
                child++;                                \
            left_child = heap_get_element(heap, child);      \
            if (last->orderby >                         \
                left_child->orderby)               \
                memcpy(heap_get_element(heap,i), heap_get_element(heap,child), \
                       sizeof(type));                  \
            else                                        \
                break;                                  \
        }                                             \
        memcpy(heap_get_element(heap, i), last, sizeof(type));    \
        return 0;                                     \
    } \
    static inline type *heap_first(heap_ ## type * heap)  \
    { \
        if (heap->n == 0)     \
            return NULL;        \
        return heap_get_element(heap, 1);  \
    } \
    static inline heap_ ## type *heap_init(void) \
    { \
        heap_ ## type * p = (heap_ ## type *)PICO_ZALLOC(sizeof(heap_ ## type));  \
        return p;     \
    } \

