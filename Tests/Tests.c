#include <stdio.h>
#include "Try.h"
#include "Config.h"
#include "TestingUtilities.h"

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

char *TestCases[] =
{
    "{}",
    "{#a#}",
    "{number: 1}"
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
    
    ConfigStream stream = (ConfigStream){.Seek = ConfigStreamSeek, .ReadC = ConfigStreamRead};
    for(int x = 0; x < TestCaseCount; x++)
    {
        char *testCase = TestCases[x];
        stream.Context = &testCase;

        TEST((config = ConfigLoad(&stream)), !=, NULL, p, 
            ErrorInfoPrint(&ErrorCurrent);
        );

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
