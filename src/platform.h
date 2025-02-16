#pragma once

enum TextColor
{
    TEXT_COLOR_WHITE,
    TEXT_COLOR_GREEN,
    TEXT_COLOR_YELLOW,
    TEXT_COLOR_RED,
    TEXT_COLOR_LIGHT_READ,
};

void platform_get_window_size(uint32_t *width, uint32_t *height);
char *platform_read_file(char *path, uint32_t *length);
void platform_log(const char *msg, TextColor color);
void platform_exit_application();
