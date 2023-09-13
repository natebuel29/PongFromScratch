#include "logger.h"

void vk_compile_shader(char *shaderPath, char *sprivPath)
{
    char command[320] = {};

    sprintf(command, "lib\\glslc.exe %s -o %s", shaderPath, sprivPath);
    // This calls the command line
    int result = system(command);

    NB_ASSERT(!result, "Failed to compile shader: %s", shaderPath);
}