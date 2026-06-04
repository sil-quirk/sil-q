#ifndef INCLUDED_SUPPORT_EDITING_BUFFER_H
#define INCLUDED_SUPPORT_EDITING_BUFFER_H

#include "h-basic.h"

struct editing_buffer;

void editing_buffer_init(
    struct editing_buffer* eb_ptr, const char* buf, size_t max_size);
void editing_buffer_destroy(struct editing_buffer* eb_ptr);
int editing_buffer_put_chr(struct editing_buffer* eb_ptr, char ch);
int editing_buffer_set_position(struct editing_buffer* eb_ptr, size_t new_pos);
void editing_buffer_display(struct editing_buffer* eb_ptr, int x, int y,
    byte col);
int editing_buffer_delete(struct editing_buffer* eb_ptr);
void editing_buffer_clear(struct editing_buffer* eb_ptr);
void editing_buffer_get_all(
    struct editing_buffer* eb_ptr, char buf[], size_t max_size);
int editing_buffer_put_str(
    struct editing_buffer* eb_ptr, const char* str, int n);

#endif /* INCLUDED_SUPPORT_EDITING_BUFFER_H */
