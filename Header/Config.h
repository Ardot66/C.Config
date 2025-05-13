#ifndef ___CONFIG___
#define ___CONFIG___
 
#include <stdio.h>
#include "CollectionsPlus.h"

enum ConfigEntryType {ConfigEntryInvalid, ConfigEntryObject, ConfigEntryList, ConfigEntryString, ConfigEntryNumber};

typedef struct ConfigStream
{
    void *Context;
    int (*Seek)(void *context, off64_t offset);
    int (*ReadC)(void *context);
    int (*WriteC)(void *context, int character);
} ConfigStream;

typedef struct ConfigEntry
{
    char *Key;
    void *Value;
    int Type;
} ConfigEntry;

typedef struct ConfigList
{
    ListGeneric List;
    int Type;
} ConfigList;

TypedefList(ConfigEntry, ConfigObject);

ConfigObject *ConfigLoad(const ConfigStream *stream);
void ConfigFree(ConfigObject *configObject);
int ConfigSave(const ConfigStream *stream, ConfigObject *config); 

#endif