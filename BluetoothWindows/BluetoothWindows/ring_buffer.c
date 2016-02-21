
#include <stdlib.h>
#include <stdint.h>

#include "ring_buffer.h"

ring_buffer_t *create_ring_buffer(size_t capacity)
{
    ring_buffer_t *ring_buffer = malloc(sizeof(ring_buffer_t));

    if (ring_buffer == NULL) {
        return NULL;
    }

    ring_buffer->capacity = capacity;
    ring_buffer->buffer = malloc(sizeof(char *) * ring_buffer->capacity);
    if (ring_buffer->buffer == NULL) {
        free(ring_buffer);
        return NULL;
    }
    ring_buffer->head = ring_buffer->buffer;
    ring_buffer->tail = ring_buffer->buffer;
    ring_buffer->end = ring_buffer->buffer + ring_buffer->capacity;
    ring_buffer->count = 0;
    return ring_buffer;
}

void delete_ring_buffer(ring_buffer_t *ring_buffer)
{
    free(ring_buffer->buffer);
    free(ring_buffer);
}

/*
TODO: would be much better with memmove()...
*/

void push_data_in_ring_buffer(ring_buffer_t *ring_buffer, char *data, size_t len)
{
    size_t counter = 0;

    while (counter < len) {
        *ring_buffer->head = data[counter];
        ring_buffer->head = ring_buffer->head + 1;
        ++counter;
        if (ring_buffer->head == ring_buffer->end) {
            ring_buffer->head = ring_buffer->buffer;
        }
        ++ring_buffer->count;
    }
}

char *pop_data_from_ring_buffer(ring_buffer_t *ring_buffer, size_t len)
{
    size_t counter = 0;
    char *data = malloc(sizeof(char *) * len + 1);

    if (data == NULL) {
        return NULL;
    }
    while (counter < len) {
        data[counter] = *ring_buffer->tail;
        ring_buffer->tail = ring_buffer->tail + 1;
        if (ring_buffer->tail == ring_buffer->end) {
            ring_buffer->tail = ring_buffer->buffer;
        }
        ++counter;
        --ring_buffer->count;
    }
    return data;
}
