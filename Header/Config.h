#ifndef ___CONFIG___
#define ___CONFIG___
 
#include <stdio.h>
#include "CollectionsPlus.h"

enum ConfigEntryType {ConfigEntryInvalid, ConfigEntryObject, ConfigEntryList, ConfigEntryString, ConfigEntryNumber};

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

ConfigObject *ConfigLoad(FILE *file);
int ConfigSave(FILE *file, ConfigObject *config); 

#endif