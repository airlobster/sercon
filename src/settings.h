#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* settings_t;

settings_t settings_init(const char* appname);
void settings_free(settings_t settings);
const char* settings_get(settings_t settings, const char* key);
bool settings_set(settings_t settings, const char* key, const char* value);
bool settings_save(settings_t settings);
bool settings_load(settings_t settings);
bool settings_delete(settings_t settings, const char* key);
void settings_clear(settings_t settings);
size_t settings_size(settings_t settings);
void settings_print(settings_t settings);

#ifdef __cplusplus
}
#endif

#endif // _SETTINGS_H_
