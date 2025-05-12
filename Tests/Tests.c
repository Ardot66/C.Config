#include <stdio.h>
#include "Try.h"
#include "Config.h"
#include "TestingUtilities.h"

int main(int argc, char **argv)
{
    FILE *file = fopen("Tests/TestFile.cfg", "r");
    ConfigObject *config;

    TEST((config = ConfigLoad(file)), !=, NULL, p, 
        ErrorInfoPrint(&ErrorCurrent);
    );

    fclose(file);
}
