#include <iostream>
#include <defines.h>
#include <windows.h>
#include "renderer/vk_renderer.cpp"
#include <platform.h>

global_variable bool running = true;
global_variable HWND window;

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        running = false;
        break;
    }

    return DefWindowProcA(window, msg, wParam, lParam);
}

bool platform_create_window()
{
    HINSTANCE instance = GetModuleHandleA(0);

    WNDCLASS wc = {};
    wc.lpfnWndProc = platform_window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = "vulkan_engine_class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassA(&wc))
    {
        MessageBoxA(0, "Failed registering window class", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    window = CreateWindowExA(
        WS_EX_APPWINDOW,
        "vulkan_engine_class",
        "pong",
        WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED,
        100, 100, 1600, 720, 0, 0, instance, 0);

    if (window == 0)
    {
        MessageBoxA(0, "Failed creating window", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    ShowWindow(window, SW_SHOW);

    return true;
}

void platform_update_window(HWND window)
{
    MSG msg;
    while (PeekMessageA(&msg, window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int main()
{
    VkContext vkcontext = {};

    if (!platform_create_window())
    {
        return -1;
    }

    if (!vk_init(&vkcontext, window))
    {
        return -1;
    }

    while (running)
    {
        platform_update_window(window);
        if (!vk_render(&vkcontext))
        {
            return -1;
        }
    }

    return 0;
}

void platform_get_window_size(uint32_t *width, uint32_t *height)
{
    RECT rect;
    GetClientRect(window, &rect);

    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;
}

char *platform_read_file(char *path, uint32_t *length)
{
    char *result = 0;

    HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file, &size))
        {
            *length = (uint32_t)size.QuadPart;
            result = new char[*length];
            DWORD bytesRead;
            if (ReadFile(file, result, *length, &bytesRead, 0))
            {
                // success
            }
            else
            {
                // TODO assert and error
                std::cout << "Failed reading file" << std::endl;
            }
        }
        else
        {
            // TODO: assert
            std::cout << "Failed getting size of file" << std::endl;
        }

        CloseHandle(file);
    }
    else
    {
        // TODO Assert
        std::cout << "failed opening file" << std::endl;
    }

    return result;
}