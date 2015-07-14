#include "font.h"
#include "errno.h"
#include <string.h>
#include <stdio.h>
#include "logger.h"

#define MAXFONTS 10

struct PipeStage font_getStage(){
	struct PipeStage stage;
	stage.obj = calloc(1, sizeof(struct Font));
	if(stage.obj == NULL)
		stage.error = ENOMEM;
	stage.create = (int (*)(void*, char*))font_init;
	stage.addUnits = (int (*)(void*, struct Units*))font_addUnits;
	stage.getArgs = (int (*)(void*, char*, size_t))font_getArgs;
	stage.process = (int (*)(void*, struct Unit*))font_format;
	stage.destroy = (int (*)(void*))font_kill;
	return stage;
}

int font_init(struct Font* font, char* configDir) {
	vector_init(&font->fonts, sizeof(char*), 8);
	return 0;
}

int font_kill(struct Font* font) {
	vector_kill(&font->fonts);
	return 0;
}

struct addFontsData {
	Vector* fonts;
};
static int addFonts(void* key, void* value, void* userdata) {
	char* k = *(char**)key;
	struct FontContainer* v = *(struct FontContainer**)value;
	struct addFontsData* data = (struct addFontsData*)userdata;

	int nextIndex = vector_size(data->fonts);
	v->fontID = nextIndex+1;

	log_write(LEVEL_INFO, "Font %d with selector: %s\n", v->fontID, v->font);
	return vector_putBack(data->fonts, &v->font);
}

struct addUnitFontsData{
	Vector* fonts;
};
static int addUnitFonts(void* elem, void* userdata) {
	struct Unit* unit = (struct Unit*)elem;
	struct addUnitFontsData* data = (struct addUnitFontsData*)userdata;
	struct addFontsData newData = {
		.fonts = data->fonts,
	};
	return map_foreach(&unit->fontMap, addFonts, &newData);
}

int font_addUnits(struct Font* font, struct Units* units){
	struct addUnitFontsData fontData = {
		.fonts = &font->fonts,
	};
	vector_foreach(&units->left, addUnitFonts, &fontData);
	vector_foreach(&units->center, addUnitFonts, &fontData);
	vector_foreach(&units->right, addUnitFonts, &fontData);
}

struct fontSelectorData{
	Vector* fontSelector;
	bool first;
};
static int fontSelector(void* elem, void* userdata) {	
	char* font = *(char**)elem;
	struct fontSelectorData* data = (struct fontSelectorData*)userdata;

	if(!data->first)
		vector_putListBack(data->fontSelector, ",", 1);
	data->first = false;
	int fontLen = strlen(font);
	vector_putListBack(data->fontSelector, font, fontLen); 
	return 0;
}

int font_getArgs(struct Font* font, char* argString, size_t maxLen) {
	Vector fontSel;
	vector_init(&fontSel, sizeof(char), 256);
	struct fontSelectorData data = {
		.fontSelector = &fontSel,
		.first = true,
	};
	vector_putListBack(&fontSel, " -f \"", 5);
	vector_foreach(&font->fonts, fontSelector, &data);
	vector_putListBack(&fontSel, "\"", 1);
	vector_putListBack(&fontSel, "\0", 1);
	size_t argLen = strlen(argString);
	if(argLen + vector_size(&fontSel) >= maxLen)
		return ENOMEM;
	strcpy(argString + argLen, fontSel.data);
	vector_kill(&fontSel);
	return 0;
}

#define LOOKUP_MAX 20
static char* getNext(const char* curPos, int* index, char (*lookups)[LOOKUP_MAX], size_t lookupsLen)
{
	if(lookupsLen == 0)
		return NULL;
	char* curMin = strstr(curPos, lookups[0]);
	*index = 0;
	char* thisPos = NULL;
	for(size_t i = 1; i < lookupsLen; i++)
	{
		//log_write(LEVEL_INFO, "Finding: %s\n", lookups[i]);
		thisPos = strstr(curPos, lookups[i]);
		if(thisPos == NULL)
			continue;
		if(curMin == NULL || thisPos < curMin)
		{
			curMin = thisPos;
			*index = i;
		}
	}
	return curMin;
}

struct createLookupData {
	char (*lookup)[LOOKUP_MAX];
	int (*ids)[MAXFONTS];
	int index;
};
static int createLookup(void* key, void* data, void* userdata) {
	char* fontName = *(char**)key;
	struct FontContainer* container = *(struct FontContainer**)data;
	struct createLookupData* udata = (struct createLookupData*)userdata;
	strcpy(udata->lookup[udata->index], "$font[");
	strcpy(udata->lookup[udata->index]+6, fontName);
	int nameLen = strlen(fontName);
	strcpy(udata->lookup[udata->index]+6+nameLen, "]");
	(*udata->ids)[udata->index] = container->fontID;
	fflush(stderr);
	udata->index++;
	return 0;
}

int font_format(struct Font* font, struct Unit* unit) {
	log_write(LEVEL_INFO, "%s\n", unit->name);
	Vector newOut;
	vector_init(&newOut, sizeof(char), UNIT_BUFFLEN); 
	
	char lookupmem[MAXFONTS*LOOKUP_MAX] = {0}; //the string we are looking for. Depending on the MAX_MATCH this might have to be longer
	char (*lookup)[LOOKUP_MAX] = (char (*)[LOOKUP_MAX])lookupmem;
	int ids[MAXFONTS]={0};
	struct createLookupData data = {
		.lookup = lookup,
		.ids = &ids,
		.index = 0,
	};
	int err = map_foreach(&unit->fontMap, createLookup, &data);
	if(err != 0) {
		log_write(LEVEL_ERROR, "Failed to construct font selectors");
		return err;
	}
	size_t formatLen = strlen(unit->buffer)+1;
	const char* curPos = unit->buffer;
	const char* prevPos = NULL;
	while(curPos < unit->buffer + formatLen)
	{
		prevPos = curPos;
		int index = 0;
		curPos = getNext(curPos, &index, lookup, map_size(&unit->fontMap));

		if(curPos == NULL)
			break;

		vector_putListBack(&newOut, prevPos, curPos-prevPos);
		char buff[50];
		snprintf(buff, 50, "%d", ids[index]);
		vector_putListBack(&newOut, buff, strlen(buff));
		curPos += strlen(lookup[index]);
	}
	vector_putListBack(&newOut, prevPos, unit->buffer + formatLen - prevPos);
	if(vector_size(&newOut) > UNIT_BUFFLEN) {
		log_write(LEVEL_ERROR, "Output too long");
		return false;
	}
	strncpy(unit->buffer, newOut.data, vector_size(&newOut));
	return true;
}