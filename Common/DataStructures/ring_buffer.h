#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t         *ring_buf;
    uint32_t        size;
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} RingBuffer_t;

void     RingBuffer_Init(RingBuffer_t *rb, uint8_t *buf, uint32_t size);
int      RingBuffer_IsEmpty(const RingBuffer_t *rb);
int      RingBuffer_IsFull(const RingBuffer_t *rb);
uint32_t RingBuffer_Count(const RingBuffer_t *rb);

/* Push one byte (called from ISR context) */
void     RingBuffer_Push(RingBuffer_t *rb, uint8_t byte);

/* Pop one byte (called from main-loop context) */
uint8_t  RingBuffer_Pop(RingBuffer_t *rb);

/* Peek at byte at logical position pos without removing (pos=0 is oldest) */
/* Returns byte value (0-255), or -1 if pos is out of range */
int      RingBuffer_Peek(const RingBuffer_t *rb, uint32_t pos);

/* Discard all buffered data */
void     RingBuffer_Flush(RingBuffer_t *rb);

#endif
