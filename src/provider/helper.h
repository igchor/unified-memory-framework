#ifdef __cplusplus
extern "C" {
#endif

void register_alloc(void *ptr);
void register_free(void *ptr);
void print_map();

#ifdef __cplusplus
}
#endif