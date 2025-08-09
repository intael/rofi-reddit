#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <stddef.h>

#define LOG_ERR_MALLOC(type, size) (log_err_malloc(sizeof(type) * (size)))

void* log_err_malloc(size_t size);

#endif
