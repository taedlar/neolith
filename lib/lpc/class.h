#pragma once
#include "program.h"

void dealloc_class(array_t *);
void free_class(array_t *);
array_t *allocate_class(class_def_t *, int);
array_t *allocate_class_by_size(int);
