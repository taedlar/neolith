#pragma once

typedef struct {
    size_t real_size;
    char *buffer;
} outbuffer_t;

void outbuf_zero(outbuffer_t *);
void outbuf_add(outbuffer_t *, const char *);
void outbuf_addchar(outbuffer_t *, char);
void outbuf_addv(outbuffer_t *, const char *, ...);
void outbuf_fix(outbuffer_t *);
void outbuf_push(outbuffer_t *);
size_t outbuf_extend(outbuffer_t *, size_t);
