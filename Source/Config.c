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

static int GetNextCharacter(const ConfigStream *stream, int skipComments)
{
    int c;
    int inComment = 0;
    while((c = stream->ReadC(stream->Context)) != EOF)
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

static void ConfigTokenFree(void *token, int tokenType);

static void ConfigEntryFree(ConfigEntry *entry)
{
    free(entry->Key);
    ConfigTokenFree(entry->Value, entry->Type);
    free(entry->Value);
}

static void ConfigObjectFree(ConfigObject *configObject)
{
    for(int x = 0; x < configObject->Count; x++)
        ConfigEntryFree(configObject->V + x);

    free(configObject->V);
}

static void ConfigListFree(ConfigList *list)
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

static void ConfigTokenFree(void *token, int tokenType)
{
    switch (tokenType)
    {
        case ConfigEntryObject: ConfigObjectFree(token); break;
        case ConfigEntryList: ConfigListFree(token); break;
    }
}

static int ConfigTokenLoadType(const ConfigStream *stream);
static int ConfigTokenLoad(const ConfigStream *stream, ListChar *stringBuffer, int tokenType, void *tokenDest);
static int ConfigEntryLoad(const ConfigStream *stream, ListChar *stringBuffer, ConfigEntry *configEntryDest);

static int ConfigObjectLoad(const ConfigStream *stream, ListChar *stringBuffer, ConfigObject *configObjectDest)
{
    ConfigObject configObject;
    Try(ListInit(&configObject, 0), -1);

    int c;
    c = GetNextCharacter(stream, 1);
    if(c == '}')
    {
        *configObjectDest = configObject;
        return 0;
    }
    stream->Seek(stream->Context, -1);
    while(1)
    {
        ConfigEntry entry;
        Try(ConfigEntryLoad(stream, stringBuffer, &entry), -1, ConfigObjectFree(&configObject););
        Try(ListAdd(&configObject, &entry), -1, ConfigObjectFree(&configObject););

        c = GetNextCharacter(stream, 1);
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

static char *ConfigStringLoad(const ConfigStream *stream, ListChar *stringBuffer)
{
    int c;
    while(1)
    {
        c = GetNextCharacter(stream, 0);

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

static int ConfigListLoad(const ConfigStream *stream, ListChar *stringBuffer, ConfigList *configListDest)
{
    ConfigList list;
    Try(ListInitGeneric(&list.List, 0, 1), -1);
    list.Type = ConfigEntryInvalid;
    size_t listTypeSize;

    while (1)
    {
        int tokenType;
        Try((tokenType = ConfigTokenLoadType(stream)) == -1, -1, free(list.List.V););
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
        Try(ConfigTokenLoad(stream, stringBuffer, list.Type, token), -1,
            ConfigTokenFree(token, list.Type);
            ConfigListFree(&list);
        );
        Try(ListAddGeneric(&list.List, token, listTypeSize), -1,
            ConfigTokenFree(token, list.Type);
            ConfigListFree(&list);
        );

        int nextChar = GetNextCharacter(stream, 1);
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

static int ConfigEntryLoad(const ConfigStream *stream, ListChar *stringBuffer, ConfigEntry *configEntryDest)
{
    int c = 0;

    while (1)
    {
        if((c = GetNextCharacter(stream, 1)) == EOF)  
            Throw(EINVAL, -1, UnexpectedEOF);
        if(c == ':')
            break;
        Try(ListAdd(stringBuffer, &c), -1);
    }

    Try(ListCharToString(stringBuffer, &configEntryDest->Key), -1);

    if((configEntryDest->Type = ConfigTokenLoadType(stream)) == -1)
    {
        free(configEntryDest->Key);
        return -1;
    }

    TryNotNull(configEntryDest->Value = malloc(ConfigTypeSize(configEntryDest->Type)), -1,
        free(configEntryDest->Key);
    );
    Try(ConfigTokenLoad(stream, stringBuffer, configEntryDest->Type, configEntryDest->Value), -1,
        free(configEntryDest->Key);
        free(configEntryDest->Value);
    );

    return 0;
}

static int ConfigNumberLoad(const ConfigStream *stream, ListChar *stringBuffer, double *numberDest)
{
    stream->Seek(stream->Context, -1);
    int c;
    while((c = GetNextCharacter(stream, 1)) != EOF)
    {
        switch(c)
        {
            case '.':
            case '0' ... '9': 
                Try(ListAdd(stringBuffer, &c), -1);
                break;
            default: goto Done;
        }
    }

    Done:
    stream->Seek(stream->Context, -1);
    *(double *)numberDest = strtod(stringBuffer->V, NULL);
    ListClear(stringBuffer);
    return 0;
}

static int ConfigTokenLoadType(const ConfigStream *stream)
{
    int c;
    switch(c = GetNextCharacter(stream, 1))
    {
        case EOF: Throw(EINVAL, -1, UnexpectedEOF);
        case '{': return ConfigEntryObject;
        case '[': return ConfigEntryList;
        case '"': return ConfigEntryString;
        case '0' ... '9': return ConfigEntryNumber;
        default: Throw(EINVAL, -1, "Invalid token type '%c'", c);
    }
}

static int ConfigTokenLoad(const ConfigStream *stream, ListChar *stringBuffer, int tokenType, void *tokenDest)
{
    switch (tokenType)
    {
        case ConfigEntryObject:
            Try(ConfigObjectLoad(stream, stringBuffer, tokenDest), -1);
            break;
        case ConfigEntryList:
            Try(ConfigListLoad(stream, stringBuffer, tokenDest), -1);
            break;
        case ConfigEntryString:
            TryNotNull(*(char **)tokenDest = ConfigStringLoad(stream, stringBuffer), -1);
            break;
        case ConfigEntryNumber:
            Try(ConfigNumberLoad(stream, stringBuffer, tokenDest), -1);
            break;
    }

    return 0;
}

ConfigObject *ConfigLoad(const ConfigStream *stream)
{
    ListChar stringBuffer;
    Try(ListInit(&stringBuffer, 32), NULL);
    ConfigObject *object;
    TryNotNull(object = malloc(sizeof(*object)), NULL,
        free(stringBuffer.V);
    );

    int c = GetNextCharacter(stream, 1);

    if(c != '{')
    {
        free(stringBuffer.V);
        Throw(EINVAL, NULL, UnexpectedNonWhitespace);
    }

    if(ConfigObjectLoad(stream, &stringBuffer, object))
    {
        free(object);
        object = NULL;
    }
    free(stringBuffer.V);
    return object;
}

void ConfigFree(ConfigObject *configObject)
{
    ConfigObjectFree(configObject);
}

static int ConfigTokenSave(const ConfigStream *stream, int tokenType, void *token);

static int ConfigEntrySave(const ConfigStream *stream, ConfigEntry *entry)
{
    for(char *key = entry->Key; *key != '\0'; key++)
        Try(stream->WriteC(stream->Context, *key), -1);

    Try(stream->WriteC(stream->Context, ':'), -1);
    Try(ConfigTokenSave(stream, entry->Type, entry->Value), -1);
    return 0;
}

static int ConfigObjectSave(const ConfigStream *stream, ConfigObject *object)
{
    Try(stream->WriteC(stream->Context, '{'), -1);
    for(int x = 0; x < object->Count; x++)
    {
        Try(ConfigEntrySave(stream, object->V + x), -1);
        if(x + 1 < object->Count)
            Try(stream->WriteC(stream->Context, ','), -1);
    }
    Try(stream->WriteC(stream->Context, '}'), -1);
    return 0;
}

static int ConfigListSave(const ConfigStream *stream, ConfigList *list)
{
    Try(stream->WriteC(stream->Context, '['), -1);

    size_t configTypeSize = ConfigTypeSize(list->Type);
    for(int x = 0; x < list->List.Count; x++)
    {
        void *token  = list->List.V + x * configTypeSize;

        Try(ConfigTokenSave(stream, list->Type, token), -1);
        if(x + 1 < list->List.Count)
            Try(stream->WriteC(stream->Context, ','), -1);
    }

    Try(stream->WriteC(stream->Context, ']'), -1);
}

static int ConfigStringSave(const ConfigStream *stream, char *string)
{
    Try(stream->WriteC(stream->Context, '"'), -1);
    for(; *string != '\0'; string++)
        Try(stream->WriteC(stream->Context, *string), -1);
    Try(stream->WriteC(stream->Context, '"'), -1);
        
    return 0;
}

static int ConfigNumberSave(const ConfigStream *stream, double *number)
{
    size_t length = snprintf(NULL, 0, "%f", *number);
    char numString[length];
    snprintf(numString, length + 1, "%f", *number);
    for(int x = 0; x < length; x++)
        Try(stream->WriteC(stream->Context, numString[x]), -1);

    return 0;
}

static int ConfigTokenSave(const ConfigStream *stream, int tokenType, void *token)
{
    switch(tokenType)
    {
        case ConfigEntryObject: return ConfigObjectSave(stream, token);
        case ConfigEntryList: return ConfigListSave(stream, token);
        case ConfigEntryString: return ConfigStringSave(stream, *(char **)token);
        case ConfigEntryNumber: return ConfigNumberSave(stream, token);
        default: Throw(EINVAL, -1, "Invalid config token type");
    }
}

int ConfigSave(const ConfigStream *stream, ConfigObject *config)
{
    return ConfigObjectSave(stream, config);
}