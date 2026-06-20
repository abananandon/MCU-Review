#include "ring_buffer.h"

void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buf, uint32_t size)
{
    if (rb == NULL || buf == NULL) {
        return;
    }

    rb->ring_buf = buf;
    rb->size     = size;
    rb->head     = 0;
    rb->tail     = 0;
    rb->count    = 0;
}

int RingBuffer_IsEmpty(const RingBuffer_t *rb)
{
    if (rb == NULL) {
        return -1;
    }

    return rb->count == 0;
}

int RingBuffer_IsFull(const RingBuffer_t *rb)
{
    if (rb == NULL) {
        return -1;
    }

    return rb->count == rb->size;
}

uint32_t RingBuffer_Count(const RingBuffer_t *rb)
{
    if (rb == NULL) {
        return 0;
    }

    return rb->count;
}

void RingBuffer_Push(RingBuffer_t *rb, uint8_t byte)
{
    if (rb == NULL) {
        return;
    }

    if (RingBuffer_IsFull(rb)) {
        /* ISR context — discard oldest to make room */
        rb->tail = (rb->tail + 1) & (rb->size - 1);
        rb->count--;
    }

    rb->ring_buf[rb->head] = byte;
    rb->head = (rb->head + 1) & (rb->size - 1);
    rb->count++;
}

uint8_t RingBuffer_Pop(RingBuffer_t *rb)
{
    uint8_t data;

    if (rb == NULL || RingBuffer_IsEmpty(rb)) {
        return 0;
    }

    data     = rb->ring_buf[rb->tail];
    rb->tail = (rb->tail + 1) & (rb->size - 1);
    rb->count--;

    return data;
}

int RingBuffer_Peek(const RingBuffer_t *rb, uint32_t pos)
{
    uint32_t idx;

    if (rb == NULL || pos >= rb->count) {
        return -1;
    }

    idx = (rb->tail + pos) & (rb->size - 1);
    return rb->ring_buf[idx];
}

void RingBuffer_Flush(RingBuffer_t *rb)
{
    if (rb == NULL) {
        return;
    }

    rb->head  = 0;
    rb->tail  = 0;
    rb->count = 0;
}
