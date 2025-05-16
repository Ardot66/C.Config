#ifndef ___CONFIG___
#define ___CONFIG___
 
#include <stdio.h>
#include <stdint.h>
#include "CollectionsPlus.h"

enum ConfigDataType {ConfigTypeInvalid, ConfigTypeObject, ConfigTypeList, ConfigTypeString, ConfigTypeNumber};
enum ConfigFlags {ConfigFlagHeap = 1, ConfigFlagKeyHeap = 2};

typedef struct ConfigType
{
    uint8_t Type;
    uint8_t Flags;
} ConfigType;

typedef struct ConfigStream
{
    void *Context;
    int (*Seek)(void *context, off64_t offset);
    int (*ReadC)(void *context);
    int (*WriteC)(void *context, char character);
} ConfigStream;

typedef struct ConfigEntry
{
    char *Key;
    void *Value;
    ConfigType Type;
} ConfigEntry;

typedef struct ConfigList
{
    ListGeneric List;
    ConfigType Type;
} ConfigList;

TypedefList(ConfigEntry, ConfigObject);

ConfigObject *ConfigLoad(const ConfigStream *stream);
void ConfigFree(ConfigObject *configObject);
int ConfigSave(const ConfigStream *stream, ConfigObject *config); 
const ConfigEntry *ConfigEntryGet(const ConfigObject *configObject, const char *key);
const ConfigEntry *ConfigEntryGetTyped(const ConfigObject *configObject, const char *key, const int type);
ConfigEntry *ConfigEntryAddFlags(ConfigObject *configObject, const char *key, const void *value, int type, int flags);
ConfigEntry *ConfigEntryAdd(ConfigObject *configObject, const char *key, const void *value, int type);
int ConfigEntryRemove(const ConfigObject *configObject, const char *key);
int ConfigObjectInit(ConfigObject *configObject);
int ConfigListInit(ConfigList *configList, int type);
int ConfigListInitFlags(ConfigList *configList, int type, int flags);

#endif