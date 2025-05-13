#include <stdio.h>
#include "Try.h"
#include "Config.h"
#include "TestingUtilities.h"

TypedefList(char, ListChar);

int ConfigStreamSeek(void *context, off64_t offset)
{
    *(char **)context += offset;
    return 0;
}

int ConfigStreamRead(void *context)
{
    char character = **(char**)context; 
    *(char **)context += 1;
    return character == '\0' ? EOF : character;
}

int ConfigStreamWrite(void *context, char character)
{
    ListChar *list = context;
    Try(ListAdd(list, &character), -1);
    return 0;
}

char *TestCases[] =
{
    "{}",
    "{#a#}",
    "{number: 1}",
    "{list: [1, 2, 3]}",
    "{object: {}}",
    "{string: \"this is a string\"}",
    "{test:[[1, 2], [{}, {}]]}",
    "{##a:1}"
};

char *FailCases[] = 
{
    "",
    "}",
    "{",
    "{arg}",
    "{arg:}",
    "{arg:1,}",
};

const size_t TestCaseCount = sizeof(TestCases) / sizeof(*TestCases);
const size_t FailCaseCount = sizeof(FailCases) / sizeof(*FailCases);

int main(int argc, char **argv)
{
    ConfigObject *config;
    
    ListChar outputList;
    Try(ListInit(&outputList, 64), -1);
    ConfigStream stream = (ConfigStream){.Seek = ConfigStreamSeek, .ReadC = ConfigStreamRead, .WriteC = ConfigStreamWrite};
    for(int x = 0; x < TestCaseCount; x++)
    {
        char *testCase = TestCases[x];
        stream.Context = &testCase;

        TEST((config = ConfigLoad(&stream)), !=, NULL, p, 
            ErrorInfoPrint(&ErrorCurrent);
        );

        stream.Context = &outputList;
        TEST(ConfigSave(&stream, config), ==, 0, d,
            ErrorInfoPrint(&ErrorCurrent);
        );
        char end = '\0';
        Try(ListAdd(&outputList, &end), -1);
        printf("%s\n", outputList.V);
        ListClear(&outputList);

        ConfigFree(config);
    }

    printf("--- Testing Errors\n");

    for(int x = 0; x < FailCaseCount; x++)
    {
        char *failCase = FailCases[x];
        stream.Context = &failCase;

        TEST((config = ConfigLoad(&stream)), ==, NULL, p);
        printf("Testing config: \"%s\" ", FailCases[x]);
        ErrorInfoPrint(&ErrorCurrent);
    }

    TestsEnd();
}
