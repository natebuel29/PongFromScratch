#include <defines.h>

// This is the game layer
#include "game/game.cpp"

// this is the ui layer

#include "ui/ui.cpp"

// this is the asset layer
#include "assets/assets.cpp"

// Platform
#include <windows.h>
#include <windowsx.h>

// this is the render layer
#include "renderer/vk_renderer.cpp"

// This is the input layer
#include "input.cpp"

// This is platform layer
#include <platform.h>
#include <logger.h>

global_variable bool running = true;
global_variable HWND window;
global_variable InputState input;

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        uint32_t keyID = INVALID_IDX;
        bool isDown = msg == WM_KEYDOWN ? true : false;

        switch ((int)wParam)
        {
        case 'A':
            keyID = KEY_A;
            break;
        case 'D':
            keyID = KEY_D;
            break;

        case 'S':
            keyID = KEY_S;
            break;
        case 'W':
            keyID = KEY_W;
            break;
        case VK_ESCAPE:
            keyID = KEY_ESC;
            break;
        }

        if (keyID < KEY_COUNT)
        {
            input.keys[keyID].isDown = isDown;
            input.keys[keyID].halfTransitionCount++;
        }
    }
    break;

    case WM_MOUSEMOVE:
    {
        input.prevMousePos = input.mousePos;
        input.mousePos = {(float)GET_X_LPARAM(lParam),
                          (float)GET_Y_LPARAM(lParam)};
    }
    break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    {
        bool isDown = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN
                          ? true
                          : false;
        KeyID keyID = (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)
                          ? KEY_LEFT_MOUSE
                      : (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP)
                          ? KEY_MIDDLE_MOUSE
                          : KEY_RIGHT_MOUSE;

        input.keys[keyID].halfTransitionCount++;
        input.keys[keyID].isDown = isDown;

        input.mouseClickPos = {(float)GET_X_LPARAM(lParam),
                               (float)GET_Y_LPARAM(lParam)};
        break;
    }

    case WM_CLOSE:
        running = false;
        break;
    }

    return DefWindowProcA(window, msg, wParam, lParam);
}

bool platform_create_window(Vec2 screenSize)
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

    uint32_t windowStyle = WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED;

    // obtain the size of the border
    {
        RECT borderRect = {};
        AdjustWindowRectEx(&borderRect, windowStyle, 0, WS_EX_APPWINDOW);

        // Grow the screenSize by the size of the border of the window;
        screenSize.x += borderRect.right - borderRect.left;
        screenSize.y += borderRect.bottom - borderRect.top;
    }
    window = CreateWindowExA(
        WS_EX_APPWINDOW,
        "vulkan_engine_class",
        "pong",
        windowStyle,
        100, 100, screenSize.x, screenSize.y, 0, 0, instance, 0);

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
    GameState gameState = {};
    UIState ui = {};

    input.screenSize = {1000,
                        700};
    if (!platform_create_window(input.screenSize))
    {
        NB_FATAL("FAILED TO OPEN WINDOW");
        return -1;
    }

    if (!init_game(&gameState, &input))
    {
        NB_FATAL("FAILED TO INITIALIZE GAME");
        return -1;
    }

    if (!vk_init(&vkcontext, window))
    {
        NB_FATAL("FAILED TO INITIALIZE VULKAN");
        return -1;
    }

    LARGE_INTEGER ticksPerSecond, lastTickCount, currentTickCount;
    QueryPerformanceFrequency(&ticksPerSecond);
    QueryPerformanceCounter(&lastTickCount);
    float dt;

    while (running)
    {
        // Clear out transition count
        {
            for (uint32_t i = 0; i < KEY_COUNT; i++)
            {
                input.keys[i].halfTransitionCount = 0;
            }
        }

        platform_update_window(window);

        // Evaludate Delta Time
        {
            QueryPerformanceCounter(&currentTickCount);

            uint64_t elapsedTicks = currentTickCount.QuadPart - lastTickCount.QuadPart;

            // Convert to Microseconds to not loose precision, by deviding a small numbner by a large one
            uint64_t elapsedTimeInMicroseconds = (elapsedTicks * 1000000) / ticksPerSecond.QuadPart;

            lastTickCount = currentTickCount;

            // Time in milliseconds
            dt = (float)elapsedTimeInMicroseconds / 1000.0f;

            // Time in Seconds
            dt /= 1000.0f;
        }
        update_ui(&ui);
        update_game(&gameState, &input, &ui, dt);
        if (!vk_render(&vkcontext, &gameState, &ui))
        {
            NB_FATAL("FAILED TO RENDER");
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
                NB_ASSERT(0, "Failed reading file: %s", path);
                NB_ERROR("Failed reading file: %s", path);
            }
        }
        else
        {
            NB_ASSERT(0, "Failed getting size of file: %s", path);
            NB_ERROR("Failed getting size of file: %s", path);
        }

        CloseHandle(file);
    }
    else
    {
        NB_ASSERT(0, "Failed opening file: %s", path);
        NB_ERROR("Failed opening file: %s", path);
    }

    return result;
}

void platform_log(const char *msg, TextColor color)
{
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    uint32_t colorBits = 0;

    switch (color)
    {
    case TEXT_COLOR_WHITE:
        colorBits = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        break;
    case TEXT_COLOR_GREEN:
        colorBits = FOREGROUND_GREEN;
        break;
    case TEXT_COLOR_YELLOW:
        colorBits = FOREGROUND_GREEN | FOREGROUND_RED;
        break;
    case TEXT_COLOR_RED:
        colorBits = FOREGROUND_RED;
        break;
    case TEXT_COLOR_LIGHT_READ:
        colorBits = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    }

    SetConsoleTextAttribute(consoleHandle, colorBits);

#ifdef DEBUG
    OutputDebugStringA(msg);
#endif

    WriteConsoleA(consoleHandle, msg, strlen(msg), 0, 0);
}

void platform_exit_application()
{
    running = false;
}