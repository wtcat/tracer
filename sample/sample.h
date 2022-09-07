/*
 * Copyright 2022 wtcat 
 */
#ifdef MTRACE_H_
#define MTRACE_H_

#ifdef __cplusplus
extern "C" {
#endif

int _lvgl_mem_init(void);
void* _lvgl_malloc(size_t size);
void* _lvgl_realloc(void* ptr, size_t size);
void _lvgl_free(void* ptr);

#ifdef __cplusplus
}
#endif
#endif /* MTRACE_H_ */
