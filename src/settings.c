#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "settings.h"
#include "utils.h"

typedef struct _settings_node_t {
	char* key;
	char* value;
	struct _settings_node_t *next, *prev;
} settings_node_t;

typedef struct {
		char* appname;
		char* filename;
		settings_node_t *head, *tail;
		size_t size;
} settings_impl_t;

settings_node_t* find_node(settings_impl_t* s, const char* key) {
	settings_node_t* node = s->head;
	while( node ) {
		if( strcmp(node->key, key) == 0 ) {
			return node;
		}
		node = node->next;
	}
	return NULL;
}

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

void settings_free(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	settings_save(settings);
	free(s->filename);
	free(s->appname);
	settings_clear(settings);
	free(s);
}

const char* settings_get(settings_t settings, const char* key) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	settings_node_t* node = find_node(s, key);
	if( node ) {
		return node->value;
	}
	return NULL;
}

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

bool settings_save(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	ASSERT(s->filename);
	ASSERT(s->appname);
	FILE* file = fopen(s->filename, "w");
	if( ! file ) {
		DEBUG_MSG("Failed to open settings file for writing: %s", s->filename);
		return false;
	}
	fprintf(file, "# Settings file for %s\n\n", s->appname);
	for(settings_node_t* node = s->head; node; node = node->next) {
		fprintf(file, "%s=%s\n", node->key, node->value);
	}
	fclose(file);
	return true;
}

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
		for(start = end; isspace(*start) || *start == '=' || *start == '#'; start++) {}
		for(end = start; *end && *end != '\n' && *end != '#'; end++) {}
		value = strndup(start, end - start);
		settings_set(settings, key, value);
		free(key);
		free(value);
	}
	fclose(file);
	return true;
}

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

void settings_print(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	for(settings_node_t* node = s->head; node; node = node->next) {
		printf("%s = %s\n", node->key, node->value);
	}
}

size_t settings_size(settings_t settings) {
	ASSERT(settings);
	settings_impl_t* s = (settings_impl_t*)settings;
	return s->size;
}
