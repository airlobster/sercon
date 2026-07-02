#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "settings.h"
#include "utils.h"

typedef struct _settings_node_t {
	char* key; /**< The key of the setting. */
	char* value; /**< The value of the setting. */
	struct _settings_node_t *next, *prev; /**< Pointers to the next and previous nodes in the linked list. */
} settings_node_t;

typedef struct {
		char* appname; /**< The name of the application. */
		char* filename; /**< The path to the settings file. */
		settings_node_t *head, *tail; /**< Pointers to the head and tail of the linked list of settings. */
		size_t size; /**< The number of settings. */
} settings_impl_t;

/**
 * @brief Find a settings node by key.
 * @param s The settings implementation instance.
 * @param key The key to search for.
 * @return The settings node if found, NULL otherwise.
 */
static settings_node_t* find_node(settings_impl_t* s, const char* key) {
	ASSERT(s);
	ASSERT(key);
	settings_node_t* node = s->head;
	while( node ) {
		if( strcmp(node->key, key) == 0 ) {
			return node;
		}
		node = node->next;
	}
	return NULL;
}

/**
 * @brief Get the home directory of the current user.
 * @return The home directory path as a string.
 */
settings_t settings_init(const char* appname) {
	settings_impl_t* s = (settings_impl_t*)malloc(sizeof(settings_impl_t));
	if( ! s ) {
		return NULL;
	}

	s->appname = strdup(appname);
	s->filename = NULL;
	s->head = NULL;
	s->tail = NULL;
	s->size = 0;

	asprintf(&s->filename, "%s/.%s.conf", getHomeDir(), appname);

	settings_load((settings_t)s);

	return (settings_t)s;
}

/**
 * @brief Free a settings instance.
 * @param settings The settings instance to free.
 */
void settings_free(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	settings_save(settings);
	free(s->filename);
	free(s->appname);
	settings_clear(settings);
	free(s);
}

/**
 * @brief Get the value of a setting by key.
 * @param settings The settings instance.
 * @param key The key of the setting.
 * @return The value of the setting if found, NULL otherwise.
 */
const char* settings_get(settings_t settings, const char* key) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	settings_node_t* node = find_node(s, key);
	if( node ) {
		return node->value;
	}
	return NULL;
}

/**
 * @brief Set the value of a setting by key.
 * @param settings The settings instance.
 * @param key The key of the setting.
 * @param value The value to set.
 * @return true if the setting was set successfully, false otherwise.
 */
bool settings_set(settings_t settings, const char* key, const char* value) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	settings_node_t* node = find_node(s, key);
	if( ! node ) {
		node = (settings_node_t*)malloc(sizeof(settings_node_t));
		if( ! node ) {
			DEBUG_MSG("Failed to allocate memory for settings node");
			return false;
		}
		node->key = strdup(key);
		node->next = NULL;
		node->prev = s->tail;
		if( s->tail ) {
			s->tail->next = node;
		} else {
			s->head = node;
		}
		s->tail = node;
		s->size++;
	}
	node->value = strdup(value ? value : "");
	return true;
}

/**
 * @brief Save the settings to the settings file.
 * @param settings The settings instance.
 * @return true if the settings were saved successfully, false otherwise.
 * @details This function saves the settings to a temporary file first,
 * and then renames it to the actual settings file. This ensures that the
 * settings file is not corrupted if the program crashes during the save operation.
 */
bool settings_save(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	ASSERT(s->filename);
	ASSERT(s->appname);

	char* tempname = 0;
	asprintf(&tempname, "%s.tmp", s->filename);

	FILE* file = fopen(tempname, "w");
	if( ! file ) {
		DEBUG_MSG("Failed to create settings temporary file: %s", tempname);
		free(tempname);
		return false;
	}
	fprintf(file, "# Settings file for %s\n\n", s->appname);
	for(settings_node_t* node = s->head; node; node = node->next) {
		fprintf(file, "%s=%s\n", node->key, node->value);
	}
	fclose(file);

	if( rename(tempname, s->filename) != 0 ) {
		DEBUG_MSG("Failed to rename temporary settings file: %s", tempname);
		free(tempname);
		return false;
	}

	free(tempname);
	return true;
}

/**
 * @brief Load the settings from the settings file.
 * @param settings The settings instance.
 * @return true if the settings were loaded successfully, false otherwise.
 */
bool settings_load(settings_t settings) {
	char line[1024];
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	ASSERT(s->filename);
	FILE* file = fopen(s->filename, "r");
	if( ! file ) {
		return false;
	}
	settings_clear(settings);
	while( fgets(line, sizeof(line), file) ) {
		char *start, *end;
		char *key = NULL, *value = NULL;
		for(start = line; isspace(*start); start++) {}
		for(end = start; *end && ! isspace(*end) && *end != '=' && *end != '#'; end++) {}
		if( start == end ) continue;
		key = strndup(start, end - start);
		if( ! key ) {
			DEBUG_MSG("Failed to allocate memory for key");
			continue;
		}
		for(start = end; isspace(*start) || *start == '=' || *start == '#'; start++) {}
		for(end = start; *end && *end != '\n' && *end != '#'; end++) {}
		value = strndup(start, end - start);
		if( ! value ) {
			DEBUG_MSG("Failed to allocate memory for value");
			free(key);
			continue;
		}
		settings_set(settings, key, value);
		free(key);
		free(value);
	}
	fclose(file);
	return true;
}

/**
 * @brief Delete a setting by key.
 * @param settings The settings instance.
 * @param key The key of the setting to delete.
 * @return true if the setting was deleted successfully, false otherwise.
 */
bool settings_delete(settings_t settings, const char* key) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	settings_node_t* node = find_node(s, key);
	if( node ) {
		if( node->prev ) {
			node->prev->next = node->next;
		} else {
			s->head = node->next;
		}
		if( node->next ) {
			node->next->prev = node->prev;
		} else {
			s->tail = node->prev;
		}
		free(node->key);
		free(node->value);
		free(node);
		s->size--;
		return true;
	}
	return false;
}

/**
 * @brief Clear all settings.
 * @param settings The settings instance.
 */
void settings_clear(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	for(settings_node_t* node = s->head; node; ) {
		settings_node_t* next = node->next;
		free(node->key);
		free(node->value);
		free(node);
		node = next;
	}
	s->head = NULL;
	s->tail = NULL;
	s->size = 0;
}

/**
 * @brief Print all settings to stdout.
 * @param settings The settings instance.
 */
void settings_print(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	for(settings_node_t* node = s->head; node; node = node->next) {
		printf("%s = %s\n", node->key, node->value);
	}
}

/**
 * @brief Get the number of settings.
 * @param settings The settings instance.
 * @return The number of settings.
 */
size_t settings_size(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	return s->size;
}
