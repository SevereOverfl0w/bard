#include "unit.h"
#include <stdlib.h>
#include <string.h>
#include "logger.h"

bool fontCmp(const void* straw, const void* needle, size_t eSize) {
	char* e1 = *(char**)straw;
	return strcmp(e1, needle) == 0;
}

void unit_init(struct Unit* unit) {
	unit->name = NULL;
	unit->type = UNIT_POLL;
	unit->command = NULL;

	unit->hasRegex = false;
	unit->regex = NULL;

	unit->advancedFormat = false;
	unit->format = NULL;

	unit->interval = 0;

	map_init(&unit->fontMap, sizeof(char*), sizeof(struct FontContainer), fontCmp);
}

void unit_free(struct Unit* unit) {
	free(unit->name);
	free(unit->command);
	free(unit->regex);
	free(unit->format);

	map_free(&unit->fontMap);
}

	//Setters
	bool unit_setName(struct Unit* unit, const char* name) {
		free(unit->name);
		if(name == NULL) {
			unit->name = NULL;
			return true;
		}

		size_t nameLen = strlen(name) + 1;
		unit->name = malloc(sizeof(char) * nameLen);
		if(unit->name == NULL) return false;
		strcpy(unit->name, name);
		return true;
	}
	bool unit_setType(struct Unit* unit, const enum UnitType type) {
		unit->type = type;
		return true;
	}
	bool unit_setCommand(struct Unit* unit, const char* command) {
		free(unit->command);
		if(command == NULL) {
			unit->command = NULL;
			return true;
		}

		size_t commandLen = strlen(command) + 1;
		unit->command = malloc(sizeof(char) * commandLen);
		if(unit->command == NULL) return false;
		strcpy(unit->command, command);
		return true;
	}
	bool unit_setRegex(struct Unit* unit, const char* regex) {
		free(unit->regex);
		if(regex == NULL) {
			unit->regex = NULL;
			return true;
		}

		size_t regexLen = strlen(regex) + 1;
		unit->regex = malloc(sizeof(char) * regexLen);
		if(unit->regex == NULL) return false;
		strcpy(unit->regex, regex);
		return true;
	}
	bool unit_setFormat(struct Unit* unit, const char* format){
		free(unit->format);
		if(format == NULL) {
			unit->format = NULL;
			return true;
		}

		size_t formatLen = strlen(format) + 1;
		unit->format = malloc(sizeof(char) * formatLen);
		if(unit->format == NULL) return false;
		strcpy(unit->format, format);
		return true;
	}
	bool unit_setInterval(struct Unit* unit, const int interval) {
		unit->interval = interval;
		return true;
	}
	//Add font to fontmap is called once per font
	bool unit_setFonts(struct Unit* unit, const char* key, const char* value) {
		if(key == NULL || value == NULL)
			return true;

		size_t keyLen = strlen(key) + 1;
		size_t valueLen = strlen(value) + 1;

		char* newKey = malloc(sizeof(char) * keyLen);
		if(newKey == NULL) return false;

		struct FontContainer* container = malloc(sizeof(struct FontContainer));
		if(container == NULL) return false;

		container->font = malloc(sizeof(char) * valueLen);
		if(container->font == NULL) return false;

		strcpy(newKey, key);
		strcpy(container->font, value);

		map_put(&unit->fontMap, &newKey, &container);
		return true;
	}
