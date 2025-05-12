#include "Config.h"
#include "Try.h"
#include <stdlib.h>
#include <string.h>

char 
    *UnexpectedNonWhitespace = "Unexpected non-whitespace character while parsing Config",
    *UnexpectedEOF = "Unexpected EOF while parsing Config";

TypedefList(char, ListChar);

static size_t ConfigTypeSize(int configType)
{
    switch (configType)
    {
        case ConfigEntryObject: return sizeof(ConfigObject);
        case ConfigEntryList: return sizeof(ConfigList);
        case ConfigEntryString: return sizeof(char *);
        case ConfigEntryNumber: return sizeof(double);
        default: Throw(EINVAL, -1, "Invalid config data type");
    }
}

static int ListCharToString(ListChar *list, char **stringDest)
{
    TryNotNull(*stringDest = malloc(list->Count + 1), -1);
    memcpy(*stringDest, list->V, list->Count);
    (*stringDest)[list->Count] = '\0';
    ListClear(list);

    return 0;
}

static int GetNextCharacter(FILE *file, int skipComments)
{
    int c;
    int inComment = 0;
    while((c = fgetc(file)) != EOF)
    {
        if(c == ' ' || c == '\n' || c == '\t' || c == '\r')
            continue;

        if(skipComments && c == '#')
        {
            inComment = !inComment;
            continue;
        }

        if(inComment)
            continue;

        return c;
    }

    return EOF;
}

void ConfigTokenFree(void *token, int tokenType);

void ConfigEntryFree(ConfigEntry *entry)
{
    free(entry->Key);
    ConfigTokenFree(entry->Value, entry->Type);
    free(entry->Value);
}

void ConfigObjectFree(ConfigObject *configObject)
{
    for(int x = 0; x < configObject->Count; x++)
        ConfigEntryFree(configObject->V + x);

    free(configObject->V);
}

void ConfigListFree(ConfigList *list)
{
    size_t listTypeSize = ConfigTypeSize(list->Type);
    for(int x = 0; x < list->List.Count; x++)
    {
        void *element = list->List.V + x * listTypeSize;
        ConfigTokenFree(element, list->Type);
        if(list->Type == ConfigEntryString)
            free(*(char **)element);
    }

    free(list->List.V);
}

void ConfigTokenFree(void *token, int tokenType)
{
    switch (tokenType)
    {
        case ConfigEntryObject: ConfigObjectFree(token); break;
        case ConfigEntryList: ConfigListFree(token); break;
    }
}

int ConfigTokenLoadType(FILE *file);
int ConfigTokenLoad(FILE *file, ListChar *stringBuffer, int tokenType, void *tokenDest);
int ConfigEntryLoad(FILE *file, ListChar *stringBuffer, ConfigEntry *configEntryDest);

int ConfigObjectLoad(FILE *file, ListChar *stringBuffer, ConfigObject *configObjectDest)
{
    ConfigObject configObject;
    Try(ListInit(&configObject, 0), -1);

    int c;
    while(1)
    {
        fseek(file, -1, SEEK_CUR);
        ConfigEntry entry;
        Try(ConfigEntryLoad(file, stringBuffer, &entry), -1, ConfigObjectFree(&configObject););
        Try(ListAdd(&configObject, &entry), -1, ConfigObjectFree(&configObject););

        c = GetNextCharacter(file, 1);
        if(c == '}')
        {
            *configObjectDest = configObject;
            return 0;
        }
        else if(c == ',')
            continue;

        ConfigObjectFree(&configObject);
        Throw(EINVAL, -1, UnexpectedEOF);
    }
}

char *ConfigStringLoad(FILE *file, ListChar *stringBuffer)
{
    int c;
    while(1)
    {
        c = GetNextCharacter(file, 0);

        if(c == '"')
        {
            char *string;
            Try(ListCharToString(stringBuffer, &string), NULL);
            return string;
        }

        ListAdd(stringBuffer, &c);
    }

    Throw(EINVAL, NULL, UnexpectedEOF);
}

int ConfigListLoad(FILE *file, ListChar *stringBuffer, ConfigList *configListDest)
{
    ConfigList list;
    Try(ListInitGeneric(&list.List, 0, 1), -1);
    list.Type = ConfigEntryInvalid;
    size_t listTypeSize;

    while (1)
    {
        int tokenType = ConfigTokenLoadType(file);
        if(list.Type == ConfigEntryInvalid)
        {
            list.Type = tokenType;
            listTypeSize = ConfigTypeSize(list.Type);
        }

        if(list.Type != tokenType)
        {
            ConfigListFree(&list); 
            Throw(EINVAL, -1, "Unexpected token type while parsing ConfigList");
        }

        char token[listTypeSize];
        Try(ConfigTokenLoad(file, stringBuffer, list.Type, token), -1,
            ConfigTokenFree(token, list.Type);
            ConfigListFree(&list);
        );
        Try(ListAddGeneric(&list.List, token, listTypeSize), -1,
            ConfigTokenFree(token, list.Type);
            ConfigListFree(&list);
        );

        int nextChar = GetNextCharacter(file, 1);
        if(nextChar == ',')
            continue;
        else if (nextChar == ']')
        {
            *configListDest = list;
            return 0;
        }
        
        ConfigListFree(&list);
        Throw(EINVAL, -1, UnexpectedNonWhitespace);
    }
}

int ConfigEntryLoad(FILE *file, ListChar *stringBuffer, ConfigEntry *configEntryDest)
{
    int c = 0;

    while (1)
    {
        if((c = GetNextCharacter(file, 1)) == EOF)  
            Throw(EINVAL, -1, UnexpectedEOF);
        if(c == ':')
            break;
        Try(ListAdd(stringBuffer, &c), -1);
    }

    Try(ListCharToString(stringBuffer, &configEntryDest->Key), -1);

    if((configEntryDest->Type = ConfigTokenLoadType(file)) == -1)
    {
        free(configEntryDest->Key);
        return -1;
    }

    TryNotNull(configEntryDest->Value = malloc(ConfigTypeSize(configEntryDest->Type)), -1,
        free(configEntryDest->Key);
    );
    Try(ConfigTokenLoad(file, stringBuffer, configEntryDest->Type, configEntryDest->Value), -1,
        free(configEntryDest->Key);
        free(configEntryDest->Value);
    );

    return 0;
}

int ConfigTokenLoadType(FILE *file)
{
    int c;
    switch(c = GetNextCharacter(file, 1))
    {
        case EOF: Throw(EINVAL, -1, UnexpectedEOF);
        case '{': return ConfigEntryObject;
        case '[': return ConfigEntryList;
        case '"': return ConfigEntryString;
        case '0' ... '9': return ConfigEntryNumber;
        default: Throw(EINVAL, -1, "Invalid token type '%c'", c);
    }
}

int ConfigTokenLoad(FILE *file, ListChar *stringBuffer, int tokenType, void *tokenDest)
{
    switch (tokenType)
    {
        case ConfigEntryObject:
            Try(ConfigObjectLoad(file, stringBuffer, tokenDest), -1);
            break;
        case ConfigEntryList:
            Try(ConfigListLoad(file, stringBuffer, tokenDest), -1);
            break;
        case ConfigEntryString:
            TryNotNull(*(char **)tokenDest = ConfigStringLoad(file, stringBuffer), -1);
            break;
        case ConfigEntryNumber:
        {
            fseek(file, -1, SEEK_CUR);
            Try(fscanf(file, "%f", tokenDest) < 1, -1);
            break;
        }
    }

    return 0;
}

ConfigObject *ConfigLoad(FILE *file)
{
    ListChar stringBuffer;
    Try(ListInit(&stringBuffer, 32), NULL);
    ConfigObject *object;
    TryNotNull(object = malloc(sizeof(*object)), NULL,
        free(stringBuffer.V);
    );

    int c = GetNextCharacter(file, 1);

    if(c != '{')
    {
        free(stringBuffer.V);
        Throw(EINVAL, NULL, UnexpectedNonWhitespace);
    }

    if(ConfigObjectLoad(file, &stringBuffer, object))
    {
        free(object);
        object = NULL;
    }
    free(stringBuffer.V);
    return object;
}

int ConfigSave(FILE *file, ConfigObject *config)
{

}