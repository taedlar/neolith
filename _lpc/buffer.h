/* buffer.h by John Garnett, 1993/11/07 */

#ifndef _BUFFER_H_
#define _BUFFER_H_

struct buffer_s {
    /* first two elements of struct must be 'ref' followed by 'size' */
    unsigned short ref;
    unsigned int size;
#ifdef DEBUG
    unsigned short extra_ref;
#endif
    unsigned char item[1];
};

/*
 * buffer.c
 */
extern buffer_t null_buf;

buffer_t *null_buffer(void);
void free_buffer(buffer_t *);
buffer_t *allocate_buffer(int);
int write_buffer(buffer_t *, int, char *, int);
char *read_buffer(buffer_t *, int, int, int *);

#endif
