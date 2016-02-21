
#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdlib.h>

typedef struct ring_buffer_s
{
    char *buffer;
    char *end;

    size_t capacity;
    size_t count;

    char *head;
    char *tail;

} ring_buffer_t;

ring_buffer_t *create_ring_buffer(size_t capacity);
void push_data_in_ring_buffer(ring_buffer_t *ring_buffer, char *data, size_t len);
char *pop_data_from_ring_buffer(ring_buffer_t *ring_buffer, size_t len);
void delete_ring_buffer(ring_buffer_t *ring_buffer);

#endif
