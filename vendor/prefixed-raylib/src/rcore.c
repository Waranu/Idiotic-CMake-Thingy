/**********************************************************************************************
*
*   rcore - Window/display management, Graphic device/context management and input management
*
*   PLATFORMS SUPPORTED:
*       > PLATFORM_DESKTOP_GLFW (GLFW backend):
*           - Windows (Win32, Win64)
*           - Linux (X11/Wayland desktop mode)
*           - macOS/OSX (x64, arm64)
*           - FreeBSD, OpenBSD, NetBSD, DragonFly (X11 desktop)
*       > PLATFORM_DESKTOP_SDL (SDL backend):
*           - Windows (Win32, Win64)
*           - Linux (X11/Wayland desktop mode)
*           - Others (not tested)
*       > PLATFORM_WEB:
*           - HTML5 (WebAssembly)
*       > PLATFORM_DRM:
*           - Raspberry Pi 0-5 (DRM/KMS)
*           - Linux DRM subsystem (KMS mode)
*       > PLATFORM_ANDROID:
*           - Android (ARM, ARM64)
*
*   CONFIGURATION:
*       #define SUPPORT_DEFAULT_FONT (default)
*           Default font is loaded on window initialization to be available for the user to render simple text.
*           NOTE: If enabled, uses external module functions to load default raylib font (module: text)
*
*       #define SUPPORT_CAMERA_SYSTEM
*           RL_Camera module is included (rcamera.h) and multiple predefined cameras are available:
*               free, 1st/3rd person, orbital, custom
*
*       #define SUPPORT_GESTURES_SYSTEM
*           Gestures module is included (rgestures.h) to support gestures detection: tap, hold, swipe, drag
*
*       #define SUPPORT_MOUSE_GESTURES
*           Mouse gestures are directly mapped like touches and processed by gestures system.
*
*       #define SUPPORT_BUSY_WAIT_LOOP
*           Use busy wait loop for timing sync, if not defined, a high-resolution timer is setup and used
*
*       #define SUPPORT_PARTIALBUSY_WAIT_LOOP
*           Use a partial-busy wait loop, in this case frame sleeps for most of the time and runs a busy-wait-loop at the end
*
*       #define SUPPORT_SCREEN_CAPTURE
*           Allow automatic screen capture of current screen pressing F12, defined in KeyCallback()
*
*       #define SUPPORT_GIF_RECORDING
*           Allow automatic gif recording of current screen pressing CTRL+F12, defined in KeyCallback()
*
*       #define SUPPORT_COMPRESSION_API
*           Support RL_CompressData() and RL_DecompressData() functions, those functions use zlib implementation
*           provided by stb_image and stb_image_write libraries, so, those libraries must be enabled on textures module
*           for linkage
*
*       #define SUPPORT_AUTOMATION_EVENTS
*           Support automatic events recording and playing, useful for automated testing systems or AI based game playing
*
*   DEPENDENCIES:
*       raymath  - 3D math functionality (RL_Vector2, RL_Vector3, RL_Matrix, RL_Quaternion)
*       camera   - Multiple 3D camera modes (free, orbital, 1st person, 3rd person)
*       gestures - Gestures system for touch-ready devices (or simulated from mouse inputs)
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2024 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

//----------------------------------------------------------------------------------
// Feature Test Macros required for this module
//----------------------------------------------------------------------------------
#if (defined(__linux__) || defined(PLATFORM_WEB)) && (_XOPEN_SOURCE < 500)
    #undef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 500 // Required for: readlink if compiled with c99 without gnu ext.
#endif

#if (defined(__linux__) || defined(PLATFORM_WEB)) && (_POSIX_C_SOURCE < 199309L)
    #undef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 199309L // Required for: CLOCK_MONOTONIC if compiled with c99 without gnu ext.
#endif

#include "raylib.h"                 // Declares module functions

// Check if config flags have been externally provided on compilation line
#if !defined(EXTERNAL_CONFIG_FLAGS)
    #include "config.h"             // Defines module configuration flags
#endif

#include "utils.h"                  // Required for: TRACELOG() macros

#include <stdlib.h>                 // Required for: srand(), rand(), atexit()
#include <stdio.h>                  // Required for: sprintf() [Used in RL_OpenURL()]
#include <string.h>                 // Required for: strrchr(), strcmp(), strlen(), memset()
#include <time.h>                   // Required for: time() [Used in InitTimer()]
#include <math.h>                   // Required for: tan() [Used in RL_BeginMode3D()], atan2f() [Used in RL_LoadVrStereoConfig()]

#define RLGL_IMPLEMENTATION
#include "rlgl.h"                   // OpenGL abstraction layer to OpenGL 1.1, 3.3+ or ES2

#define RAYMATH_IMPLEMENTATION
#include "raymath.h"                // RL_Vector2, RL_Vector3, RL_Quaternion and RL_Matrix functionality

#if defined(SUPPORT_GESTURES_SYSTEM)
    #define RGESTURES_IMPLEMENTATION
    #include "rgestures.h"          // Gestures detection functionality
#endif

#if defined(SUPPORT_CAMERA_SYSTEM)
    #define RCAMERA_IMPLEMENTATION
    #include "rcamera.h"            // RL_Camera system functionality
#endif

#if defined(SUPPORT_GIF_RECORDING)
    #define MSF_GIF_MALLOC(contextPointer, newSize) RL_MALLOC(newSize)
    #define MSF_GIF_REALLOC(contextPointer, oldMemory, oldSize, newSize) RL_REALLOC(oldMemory, newSize)
    #define MSF_GIF_FREE(contextPointer, oldMemory, oldSize) RL_FREE(oldMemory)

    #define MSF_GIF_IMPL
    #include "external/msf_gif.h"   // GIF recording functionality
#endif

#if defined(SUPPORT_COMPRESSION_API)
    #define SINFL_IMPLEMENTATION
    #define SINFL_NO_SIMD
    #include "external/sinfl.h"     // Deflate (RFC 1951) decompressor

    #define SDEFL_IMPLEMENTATION
    #include "external/sdefl.h"     // Deflate (RFC 1951) compressor
#endif

#if defined(SUPPORT_RPRAND_GENERATOR)
    #define RPRAND_IMPLEMENTATION
    #include "external/rprand.h"
#endif

#if defined(__linux__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

// Platform specific defines to handle RL_GetApplicationDirectory()
#if (defined(_WIN32) && !defined(PLATFORM_DESKTOP_RGFW)) || (defined(_MSC_VER) && defined(PLATFORM_DESKTOP_RGFW))
    #ifndef MAX_PATH
        #define MAX_PATH 1025
    #endif
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void *hModule, void *lpFilename, unsigned long nSize);
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void *hModule, void *lpFilename, unsigned long nSize);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, void *widestr, int cchwide, void *str, int cbmb, void *defchar, int *used_default);
unsigned int __stdcall timeBeginPeriod(unsigned int uPeriod);
unsigned int __stdcall timeEndPeriod(unsigned int uPeriod);
#elif defined(__linux__)
    #include <unistd.h>
#elif defined(__FreeBSD__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <sys/syslimits.h>
    #include <mach-o/dyld.h>
#endif // OSs

#define _CRT_INTERNAL_NONSTDC_NAMES  1
#include <sys/stat.h>               // Required for: stat(), S_ISREG [Used in RL_GetFileModTime(), IsFilePath()]

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#if defined(_WIN32) && (defined(_MSC_VER) || defined(__TINYC__))
    #define DIRENT_MALLOC RL_MALLOC
    #define DIRENT_FREE RL_FREE

    #include "external/dirent.h"    // Required for: DIR, opendir(), closedir() [Used in RL_LoadDirectoryFiles()]
#else
    #include <dirent.h>             // Required for: DIR, opendir(), closedir() [Used in RL_LoadDirectoryFiles()]
#endif

#if defined(_WIN32)
    #include <io.h>                 // Required for: _access() [Used in RL_FileExists()]
    #include <direct.h>             // Required for: _getch(), _chdir(), _mkdir()
    #define GETCWD _getcwd          // NOTE: MSDN recommends not to use getcwd(), chdir()
    #define CHDIR _chdir
    #define MKDIR(dir) _mkdir(dir)
#else
    #include <unistd.h>             // Required for: getch(), chdir(), mkdir(), access()
    #define GETCWD getcwd
    #define CHDIR chdir
    #define MKDIR(dir) mkdir(dir, 0777)
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_FILEPATH_CAPACITY
    #define MAX_FILEPATH_CAPACITY       8192        // Maximum capacity for filepath
#endif
#ifndef MAX_FILEPATH_LENGTH
    #if defined(_WIN32)
        #define MAX_FILEPATH_LENGTH      256        // On Win32, MAX_PATH = 260 (limits.h) but Windows 10, Version 1607 enables long paths...
    #else
        #define MAX_FILEPATH_LENGTH     4096        // On Linux, PATH_MAX = 4096 by default (limits.h)
    #endif
#endif

#ifndef MAX_KEYBOARD_KEYS
    #define MAX_KEYBOARD_KEYS            512        // Maximum number of keyboard keys supported
#endif
#ifndef MAX_MOUSE_BUTTONS
    #define MAX_MOUSE_BUTTONS              8        // Maximum number of mouse buttons supported
#endif
#ifndef MAX_GAMEPADS
    #define MAX_GAMEPADS                   4        // Maximum number of gamepads supported
#endif
#ifndef MAX_GAMEPAD_AXIS
    #define MAX_GAMEPAD_AXIS               8        // Maximum number of axis supported (per gamepad)
#endif
#ifndef MAX_GAMEPAD_BUTTONS
    #define MAX_GAMEPAD_BUTTONS           32        // Maximum number of buttons supported (per gamepad)
#endif
#ifndef MAX_GAMEPAD_VIBRATION_TIME
    #define MAX_GAMEPAD_VIBRATION_TIME     2.0f     // Maximum vibration time in seconds
#endif
#ifndef MAX_TOUCH_POINTS
    #define MAX_TOUCH_POINTS               8        // Maximum number of touch points supported
#endif
#ifndef MAX_KEY_PRESSED_QUEUE
    #define MAX_KEY_PRESSED_QUEUE         16        // Maximum number of keys in the key input queue
#endif
#ifndef MAX_CHAR_PRESSED_QUEUE
    #define MAX_CHAR_PRESSED_QUEUE        16        // Maximum number of characters in the char input queue
#endif

#ifndef MAX_DECOMPRESSION_SIZE
    #define MAX_DECOMPRESSION_SIZE        64        // Maximum size allocated for decompression in MB
#endif

#ifndef MAX_AUTOMATION_EVENTS
    #define MAX_AUTOMATION_EVENTS      16384        // Maximum number of automation events to record
#endif

#ifndef DIRECTORY_FILTER_TAG
    #define DIRECTORY_FILTER_TAG       "DIR"        // Name tag used to request directory inclusion on directory scan
#endif                                              // NOTE: Used in ScanDirectoryFiles(), ScanDirectoryFilesRecursively() and RL_LoadDirectoryFilesEx()

// Flags operation macros
#define FLAG_SET(n, f) ((n) |= (f))
#define FLAG_CLEAR(n, f) ((n) &= ~(f))
#define FLAG_TOGGLE(n, f) ((n) ^= (f))
#define FLAG_CHECK(n, f) ((n) & (f))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct { int x; int y; } Point;
typedef struct { unsigned int width; unsigned int height; } Size;

// Core global state context data
typedef struct CoreData {
    struct {
        const char *title;                  // Window text title const pointer
        unsigned int flags;                 // Configuration flags (bit based), keeps window state
        bool ready;                         // Check if window has been initialized successfully
        bool fullscreen;                    // Check if fullscreen mode is enabled
        bool shouldClose;                   // Check if window set for closing
        bool resizedLastFrame;              // Check if window has been resized last frame
        bool eventWaiting;                  // Wait for events before ending frame
        bool usingFbo;                      // Using FBO (RL_RenderTexture) for rendering instead of default framebuffer

        Point position;                     // Window position (required on fullscreen toggle)
        Point previousPosition;             // Window previous position (required on borderless windowed toggle)
        Size display;                       // Display width and height (monitor, device-screen, LCD, ...)
        Size screen;                        // Screen width and height (used render area)
        Size previousScreen;                // Screen previous width and height (required on borderless windowed toggle)
        Size currentFbo;                    // Current render width and height (depends on active fbo)
        Size render;                        // Framebuffer width and height (render area, including black bars if required)
        Point renderOffset;                 // Offset from render area (must be divided by 2)
        Size screenMin;                     // Screen minimum width and height (for resizable window)
        Size screenMax;                     // Screen maximum width and height (for resizable window)
        RL_Matrix screenScale;                 // RL_Matrix to scale screen (framebuffer rendering)

        char **dropFilepaths;               // Store dropped files paths pointers (provided by GLFW)
        unsigned int dropFileCount;         // Count dropped files strings

    } Window;
    struct {
        const char *basePath;               // Base path for data storage

    } Storage;
    struct {
        struct {
            int exitKey;                    // Default exit key
            char currentKeyState[MAX_KEYBOARD_KEYS];        // Registers current frame key state
            char previousKeyState[MAX_KEYBOARD_KEYS];       // Registers previous frame key state

            // NOTE: Since key press logic involves comparing prev vs cur key state, we need to handle key repeats specially
            char keyRepeatInFrame[MAX_KEYBOARD_KEYS];       // Registers key repeats for current frame

            int keyPressedQueue[MAX_KEY_PRESSED_QUEUE];     // Input keys queue
            int keyPressedQueueCount;       // Input keys queue count

            int charPressedQueue[MAX_CHAR_PRESSED_QUEUE];   // Input characters queue (unicode)
            int charPressedQueueCount;      // Input characters queue count

        } Keyboard;
        struct {
            RL_Vector2 offset;                 // Mouse offset
            RL_Vector2 scale;                  // Mouse scaling
            RL_Vector2 currentPosition;        // Mouse position on screen
            RL_Vector2 previousPosition;       // Previous mouse position

            int cursor;                     // Tracks current mouse cursor
            bool cursorHidden;              // Track if cursor is hidden
            bool cursorOnScreen;            // Tracks if cursor is inside client area

            char currentButtonState[MAX_MOUSE_BUTTONS];     // Registers current mouse button state
            char previousButtonState[MAX_MOUSE_BUTTONS];    // Registers previous mouse button state
            RL_Vector2 currentWheelMove;       // Registers current mouse wheel variation
            RL_Vector2 previousWheelMove;      // Registers previous mouse wheel variation

        } Mouse;
        struct {
            int pointCount;                             // Number of touch points active
            int pointId[MAX_TOUCH_POINTS];              // Point identifiers
            RL_Vector2 position[MAX_TOUCH_POINTS];         // Touch position on screen
            char currentTouchState[MAX_TOUCH_POINTS];   // Registers current touch state
            char previousTouchState[MAX_TOUCH_POINTS];  // Registers previous touch state

        } Touch;
        struct {
            int lastButtonPressed;          // Register last gamepad button pressed
            int axisCount[MAX_GAMEPADS];    // Register number of available gamepad axis
            bool ready[MAX_GAMEPADS];       // Flag to know if gamepad is ready
            char name[MAX_GAMEPADS][64];    // Gamepad name holder
            char currentButtonState[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];     // Current gamepad buttons state
            char previousButtonState[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];    // Previous gamepad buttons state
            float axisState[MAX_GAMEPADS][MAX_GAMEPAD_AXIS];                // Gamepad axis state

        } Gamepad;
    } Input;
    struct {
        double current;                     // Current time measure
        double previous;                    // Previous time measure
        double update;                      // Time measure for frame update
        double draw;                        // Time measure for frame draw
        double frame;                       // Time measure for one frame
        double target;                      // Desired time for one frame, if 0 not applied
        unsigned long long int base;        // Base time measure for hi-res timer (PLATFORM_ANDROID, PLATFORM_DRM)
        unsigned int frameCounter;          // Frame counter

    } Time;
} CoreData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
RLAPI const char *raylib_version = RAYLIB_VERSION;  // raylib version exported symbol, required for some bindings

CoreData CORE = { 0 };                      // Global CORE state context

// Flag to note GPU acceleration is available,
// referenced from other modules to support GPU data loading
// NOTE: Useful to allow RL_Texture, RL_RenderTexture, RL_Font.texture, RL_Mesh.vaoId/vboId, RL_Shader loading
bool isGpuReady = false;

#if defined(SUPPORT_SCREEN_CAPTURE)
static int screenshotCounter = 0;           // Screenshots counter
#endif

#if defined(SUPPORT_GIF_RECORDING)
static unsigned int gifFrameCounter = 0;    // GIF frames counter
static bool gifRecording = false;           // GIF recording state
static MsfGifState gifState = { 0 };        // MSGIF context state
#endif

#if defined(SUPPORT_AUTOMATION_EVENTS)
// Automation events type
typedef enum AutomationEventType {
    EVENT_NONE = 0,
    // Input events
    INPUT_KEY_UP,                   // param[0]: key
    INPUT_KEY_DOWN,                 // param[0]: key
    INPUT_KEY_PRESSED,              // param[0]: key
    INPUT_KEY_RELEASED,             // param[0]: key
    INPUT_MOUSE_BUTTON_UP,          // param[0]: button
    INPUT_MOUSE_BUTTON_DOWN,        // param[0]: button
    INPUT_MOUSE_POSITION,           // param[0]: x, param[1]: y
    INPUT_MOUSE_WHEEL_MOTION,       // param[0]: x delta, param[1]: y delta
    INPUT_GAMEPAD_CONNECT,          // param[0]: gamepad
    INPUT_GAMEPAD_DISCONNECT,       // param[0]: gamepad
    INPUT_GAMEPAD_BUTTON_UP,        // param[0]: button
    INPUT_GAMEPAD_BUTTON_DOWN,      // param[0]: button
    INPUT_GAMEPAD_AXIS_MOTION,      // param[0]: axis, param[1]: delta
    INPUT_TOUCH_UP,                 // param[0]: id
    INPUT_TOUCH_DOWN,               // param[0]: id
    INPUT_TOUCH_POSITION,           // param[0]: x, param[1]: y
    INPUT_GESTURE,                  // param[0]: gesture
    // Window events
    WINDOW_CLOSE,                   // no params
    WINDOW_MAXIMIZE,                // no params
    WINDOW_MINIMIZE,                // no params
    WINDOW_RESIZE,                  // param[0]: width, param[1]: height
    // Custom events
    ACTION_TAKE_SCREENSHOT,         // no params
    ACTION_SETTARGETFPS             // param[0]: fps
} AutomationEventType;

// Event type to config events flags
// TODO: Not used at the moment
typedef enum {
    EVENT_INPUT_KEYBOARD    = 0,
    EVENT_INPUT_MOUSE       = 1,
    EVENT_INPUT_GAMEPAD     = 2,
    EVENT_INPUT_TOUCH       = 4,
    EVENT_INPUT_GESTURE     = 8,
    EVENT_WINDOW            = 16,
    EVENT_CUSTOM            = 32
} EventType;

// Event type name strings, required for export
static const char *autoEventTypeName[] = {
    "EVENT_NONE",
    "INPUT_KEY_UP",
    "INPUT_KEY_DOWN",
    "INPUT_KEY_PRESSED",
    "INPUT_KEY_RELEASED",
    "INPUT_MOUSE_BUTTON_UP",
    "INPUT_MOUSE_BUTTON_DOWN",
    "INPUT_MOUSE_POSITION",
    "INPUT_MOUSE_WHEEL_MOTION",
    "INPUT_GAMEPAD_CONNECT",
    "INPUT_GAMEPAD_DISCONNECT",
    "INPUT_GAMEPAD_BUTTON_UP",
    "INPUT_GAMEPAD_BUTTON_DOWN",
    "INPUT_GAMEPAD_AXIS_MOTION",
    "INPUT_TOUCH_UP",
    "INPUT_TOUCH_DOWN",
    "INPUT_TOUCH_POSITION",
    "INPUT_GESTURE",
    "WINDOW_CLOSE",
    "WINDOW_MAXIMIZE",
    "WINDOW_MINIMIZE",
    "WINDOW_RESIZE",
    "ACTION_TAKE_SCREENSHOT",
    "ACTION_SETTARGETFPS"
};

/*
// Automation event (24 bytes)
// NOTE: Opaque struct, internal to raylib
struct RL_AutomationEvent {
    unsigned int frame;                 // Event frame
    unsigned int type;                  // Event type (AutomationEventType)
    int params[4];                      // Event parameters (if required)
};
*/

static RL_AutomationEventList *currentEventList = NULL;        // Current automation events list, set by user, keep internal pointer
static bool automationEventRecording = false;               // Recording automation events flag
//static short automationEventEnabled = 0b0000001111111111; // TODO: Automation events enabled for recording/playing
#endif
//-----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
// NOTE: Those functions are common for all platforms!
//----------------------------------------------------------------------------------

#if defined(SUPPORT_MODULE_RTEXT) && defined(SUPPORT_DEFAULT_FONT)
extern void LoadFontDefault(void);      // [Module: text] Loads default font on RL_InitWindow()
extern void UnloadFontDefault(void);    // [Module: text] Unloads default font from GPU memory
#endif

extern int InitPlatform(void);          // Initialize platform (graphics, inputs and more)
extern void ClosePlatform(void);        // Close platform

static void InitTimer(void);                                // Initialize timer, hi-resolution if available (required by InitPlatform())
static void SetupFramebuffer(int width, int height);        // Setup main framebuffer (required by InitPlatform())
static void SetupViewport(int width, int height);           // Set viewport for a provided width and height

static void ScanDirectoryFiles(const char *basePath, RL_FilePathList *list, const char *filter);   // Scan all files and directories in a base path
static void ScanDirectoryFilesRecursively(const char *basePath, RL_FilePathList *list, const char *filter);  // Scan all files and directories recursively from a base path

#if defined(SUPPORT_AUTOMATION_EVENTS)
static void RecordAutomationEvent(void); // Record frame events (to internal events array)
#endif

#if defined(_WIN32) && !defined(PLATFORM_DESKTOP_RGFW)
// NOTE: We declare Sleep() function symbol to avoid including windows.h (kernel32.lib linkage required)
void __stdcall Sleep(unsigned long msTimeout);              // Required for: RL_WaitTime()
#endif

#if !defined(SUPPORT_MODULE_RTEXT)
const char *RL_TextFormat(const char *text, ...);              // Formatting of text with variables to 'embed'
#endif // !SUPPORT_MODULE_RTEXT

#if defined(PLATFORM_DESKTOP)
    #define PLATFORM_DESKTOP_GLFW
#endif

// Include platform-specific submodules
#if defined(PLATFORM_DESKTOP_GLFW)
    #include "platforms/rcore_desktop_glfw.c"
#elif defined(PLATFORM_DESKTOP_SDL)
    #include "platforms/rcore_desktop_sdl.c"
#elif defined(PLATFORM_DESKTOP_RGFW)
    #include "platforms/rcore_desktop_rgfw.c"
#elif defined(PLATFORM_WEB)
    #include "platforms/rcore_web.c"
#elif defined(PLATFORM_DRM)
    #include "platforms/rcore_drm.c"
#elif defined(PLATFORM_ANDROID)
    #include "platforms/rcore_android.c"
#else
    // TODO: Include your custom platform backend!
    // i.e software rendering backend or console backend!
#endif

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//bool RL_WindowShouldClose(void)
//void RL_ToggleFullscreen(void)
//void RL_ToggleBorderlessWindowed(void)
//void RL_MaximizeWindow(void)
//void RL_MinimizeWindow(void)
//void RL_RestoreWindow(void)

//void RL_SetWindowState(unsigned int flags)
//void RL_ClearWindowState(unsigned int flags)

//void RL_SetWindowIcon(RL_Image image)
//void RL_SetWindowIcons(RL_Image *images, int count)
//void RL_SetWindowTitle(const char *title)
//void RL_SetWindowPosition(int x, int y)
//void RL_SetWindowMonitor(int monitor)
//void RL_SetWindowMinSize(int width, int height)
//void RL_SetWindowMaxSize(int width, int height)
//void RL_SetWindowSize(int width, int height)
//void RL_SetWindowOpacity(float opacity)
//void RL_SetWindowFocused(void)
//void *RL_GetWindowHandle(void)
//RL_Vector2 RL_GetWindowPosition(void)
//RL_Vector2 RL_GetWindowScaleDPI(void)

//int RL_GetMonitorCount(void)
//int RL_GetCurrentMonitor(void)
//int RL_GetMonitorWidth(int monitor)
//int RL_GetMonitorHeight(int monitor)
//int RL_GetMonitorPhysicalWidth(int monitor)
//int RL_GetMonitorPhysicalHeight(int monitor)
//int RL_GetMonitorRefreshRate(int monitor)
//RL_Vector2 RL_GetMonitorPosition(int monitor)
//const char *RL_GetMonitorName(int monitor)

//void RL_SetClipboardText(const char *text)
//const char *RL_GetClipboardText(void)

//void RL_ShowCursor(void)
//void RL_HideCursor(void)
//void RL_EnableCursor(void)
//void RL_DisableCursor(void)

// Initialize window and OpenGL context
// NOTE: data parameter could be used to pass any kind of required data to the initialization
void RL_InitWindow(int width, int height, const char *title)
{
    TRACELOG(LOG_INFO, "Initializing raylib %s", RAYLIB_VERSION);

#if defined(PLATFORM_DESKTOP_GLFW)
    TRACELOG(LOG_INFO, "Platform backend: DESKTOP (GLFW)");
#elif defined(PLATFORM_DESKTOP_SDL)
    TRACELOG(LOG_INFO, "Platform backend: DESKTOP (SDL)");
#elif defined(PLATFORM_DESKTOP_RGFW)
    TRACELOG(LOG_INFO, "Platform backend: DESKTOP (RGFW)");
#elif defined(PLATFORM_WEB)
    TRACELOG(LOG_INFO, "Platform backend: WEB (HTML5)");
#elif defined(PLATFORM_DRM)
    TRACELOG(LOG_INFO, "Platform backend: NATIVE DRM");
#elif defined(PLATFORM_ANDROID)
    TRACELOG(LOG_INFO, "Platform backend: ANDROID");
#else
    // TODO: Include your custom platform backend!
    // i.e software rendering backend or console backend!
    TRACELOG(LOG_INFO, "Platform backend: CUSTOM");
#endif

    TRACELOG(LOG_INFO, "Supported raylib modules:");
    TRACELOG(LOG_INFO, "    > rcore:..... loaded (mandatory)");
    TRACELOG(LOG_INFO, "    > rlgl:...... loaded (mandatory)");
#if defined(SUPPORT_MODULE_RSHAPES)
    TRACELOG(LOG_INFO, "    > rshapes:... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rshapes:... not loaded (optional)");
#endif
#if defined(SUPPORT_MODULE_RTEXTURES)
    TRACELOG(LOG_INFO, "    > rtextures:. loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rtextures:. not loaded (optional)");
#endif
#if defined(SUPPORT_MODULE_RTEXT)
    TRACELOG(LOG_INFO, "    > rtext:..... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rtext:..... not loaded (optional)");
#endif
#if defined(SUPPORT_MODULE_RMODELS)
    TRACELOG(LOG_INFO, "    > rmodels:... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rmodels:... not loaded (optional)");
#endif
#if defined(SUPPORT_MODULE_RAUDIO)
    TRACELOG(LOG_INFO, "    > raudio:.... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > raudio:.... not loaded (optional)");
#endif

    // Initialize window data
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;
    CORE.Window.eventWaiting = false;
    CORE.Window.screenScale = MatrixIdentity();     // No draw scaling required by default
    if ((title != NULL) && (title[0] != 0)) CORE.Window.title = title;

    // Initialize global input state
    memset(&CORE.Input, 0, sizeof(CORE.Input));     // Reset CORE.Input structure to 0
    CORE.Input.Keyboard.exitKey = KEY_ESCAPE;
    CORE.Input.Mouse.scale = (RL_Vector2){ 1.0f, 1.0f };
    CORE.Input.Mouse.cursor = MOUSE_CURSOR_ARROW;
    CORE.Input.Gamepad.lastButtonPressed = GAMEPAD_BUTTON_UNKNOWN;

    // Initialize platform
    //--------------------------------------------------------------
    InitPlatform();
    //--------------------------------------------------------------

    // Initialize rlgl default data (buffers and shaders)
    // NOTE: CORE.Window.currentFbo.width and CORE.Window.currentFbo.height not used, just stored as globals in rlgl
    rlglInit(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);
    isGpuReady = true; // Flag to note GPU has been initialized successfully

    // Setup default viewport
    SetupViewport(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);

#if defined(SUPPORT_MODULE_RTEXT)
    #if defined(SUPPORT_DEFAULT_FONT)
        // Load default font
        // WARNING: External function: Module required: rtext
        LoadFontDefault();
        #if defined(SUPPORT_MODULE_RSHAPES)
        // Set font white rectangle for shapes drawing, so shapes and text can be batched together
        // WARNING: rshapes module is required, if not available, default internal white rectangle is used
        RL_Rectangle rec = RL_GetFontDefault().recs[95];
        if (CORE.Window.flags & FLAG_MSAA_4X_HINT)
        {
            // NOTE: We try to maxime rec padding to avoid pixel bleeding on MSAA filtering
            RL_SetShapesTexture(RL_GetFontDefault().texture, (RL_Rectangle){ rec.x + 2, rec.y + 2, 1, 1 });
        }
        else
        {
            // NOTE: We set up a 1px padding on char rectangle to avoid pixel bleeding
            RL_SetShapesTexture(RL_GetFontDefault().texture, (RL_Rectangle){ rec.x + 1, rec.y + 1, rec.width - 2, rec.height - 2 });
        }
        #endif
    #endif
#else
    #if defined(SUPPORT_MODULE_RSHAPES)
    // Set default texture and rectangle to be used for shapes drawing
    // NOTE: rlgl default texture is a 1x1 pixel UNCOMPRESSED_R8G8B8A8
    RL_Texture2D texture = { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    RL_SetShapesTexture(texture, (RL_Rectangle){ 0.0f, 0.0f, 1.0f, 1.0f });    // WARNING: Module required: rshapes
    #endif
#endif

    CORE.Time.frameCounter = 0;
    CORE.Window.shouldClose = false;

    // Initialize random seed
    RL_SetRandomSeed((unsigned int)time(NULL));
    
    TRACELOG(LOG_INFO, "SYSTEM: Working Directory: %s", RL_GetWorkingDirectory());
}

// Close window and unload OpenGL context
void RL_CloseWindow(void)
{
#if defined(SUPPORT_GIF_RECORDING)
    if (gifRecording)
    {
        MsfGifResult result = msf_gif_end(&gifState);
        msf_gif_free(result);
        gifRecording = false;
    }
#endif

#if defined(SUPPORT_MODULE_RTEXT) && defined(SUPPORT_DEFAULT_FONT)
    UnloadFontDefault();        // WARNING: Module required: rtext
#endif

    rlglClose();                // De-init rlgl

    // De-initialize platform
    //--------------------------------------------------------------
    ClosePlatform();
    //--------------------------------------------------------------

    CORE.Window.ready = false;
    TRACELOG(LOG_INFO, "Window closed successfully");
}

// Check if window has been initialized successfully
bool RL_IsWindowReady(void)
{
    return CORE.Window.ready;
}

// Check if window is currently fullscreen
bool RL_IsWindowFullscreen(void)
{
    return CORE.Window.fullscreen;
}

// Check if window is currently hidden
bool RL_IsWindowHidden(void)
{
    return ((CORE.Window.flags & FLAG_WINDOW_HIDDEN) > 0);
}

// Check if window has been minimized
bool RL_IsWindowMinimized(void)
{
    return ((CORE.Window.flags & FLAG_WINDOW_MINIMIZED) > 0);
}

// Check if window has been maximized
bool RL_IsWindowMaximized(void)
{
    return ((CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) > 0);
}

// Check if window has the focus
bool RL_IsWindowFocused(void)
{
    return ((CORE.Window.flags & FLAG_WINDOW_UNFOCUSED) == 0);
}

// Check if window has been resizedLastFrame
bool RL_IsWindowResized(void)
{
    return CORE.Window.resizedLastFrame;
}

// Check if one specific window flag is enabled
bool RL_IsWindowState(unsigned int flag)
{
    return ((CORE.Window.flags & flag) > 0);
}

// Get current screen width
int RL_GetScreenWidth(void)
{
    return CORE.Window.screen.width;
}

// Get current screen height
int RL_GetScreenHeight(void)
{
    return CORE.Window.screen.height;
}

// Get current render width which is equal to screen width*dpi scale
int RL_GetRenderWidth(void)
{
    int width = 0;
#if defined(__APPLE__)
    RL_Vector2 scale = RL_GetWindowScaleDPI();
    width = (int)((float)CORE.Window.render.width*scale.x);
#else
    width = CORE.Window.render.width;
#endif
    return width;
}

// Get current screen height which is equal to screen height*dpi scale
int RL_GetRenderHeight(void)
{
    int height = 0;
#if defined(__APPLE__)
    RL_Vector2 scale = RL_GetWindowScaleDPI();
    height = (int)((float)CORE.Window.render.height*scale.y);
#else
    height = CORE.Window.render.height;
#endif
    return height;
}

// Enable waiting for events on RL_EndDrawing(), no automatic event polling
void RL_EnableEventWaiting(void)
{
    CORE.Window.eventWaiting = true;
}

// Disable waiting for events on RL_EndDrawing(), automatic events polling
void RL_DisableEventWaiting(void)
{
    CORE.Window.eventWaiting = false;
}

// Check if cursor is not visible
bool RL_IsCursorHidden(void)
{
    return CORE.Input.Mouse.cursorHidden;
}

// Check if cursor is on the current screen
bool RL_IsCursorOnScreen(void)
{
    return CORE.Input.Mouse.cursorOnScreen;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Screen Drawing
//----------------------------------------------------------------------------------

// Set background color (framebuffer clear color)
void RL_ClearBackground(RL_Color color)
{
    rlClearColor(color.r, color.g, color.b, color.a);   // Set clear color
    rlClearScreenBuffers();                             // Clear current framebuffers
}

// Setup canvas (framebuffer) to start drawing
void RL_BeginDrawing(void)
{
    // WARNING: Previously to RL_BeginDrawing() other render textures drawing could happen,
    // consequently the measure for update vs draw is not accurate (only the total frame time is accurate)

    CORE.Time.current = RL_GetTime();      // Number of elapsed seconds since InitTimer()
    CORE.Time.update = CORE.Time.current - CORE.Time.previous;
    CORE.Time.previous = CORE.Time.current;

    rlLoadIdentity();                   // Reset current matrix (modelview)
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling

    //rlTranslatef(0.375, 0.375, 0);    // HACK to have 2D pixel-perfect drawing on OpenGL 1.1
                                        // NOTE: Not required with OpenGL 3.3+
}

// End canvas drawing and swap buffers (double buffering)
void RL_EndDrawing(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

#if defined(SUPPORT_GIF_RECORDING)
    // Draw record indicator
    if (gifRecording)
    {
        #ifndef GIF_RECORD_FRAMERATE
        #define GIF_RECORD_FRAMERATE    10
        #endif
        gifFrameCounter += (unsigned int)(RL_GetFrameTime()*1000);

        // NOTE: We record one gif frame depending on the desired gif framerate
        if (gifFrameCounter > 1000/GIF_RECORD_FRAMERATE)
        {
            // Get image data for the current frame (from backbuffer)
            // NOTE: This process is quite slow... :(
            RL_Vector2 scale = RL_GetWindowScaleDPI();
            unsigned char *screenData = rlReadScreenPixels((int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y));

            #ifndef GIF_RECORD_BITRATE
            #define GIF_RECORD_BITRATE 16
            #endif

            // Add the frame to the gif recording, given how many frames have passed in centiseconds
            msf_gif_frame(&gifState, screenData, gifFrameCounter/10, GIF_RECORD_BITRATE, (int)((float)CORE.Window.render.width*scale.x)*4);
            gifFrameCounter -= 1000/GIF_RECORD_FRAMERATE;

            RL_FREE(screenData);    // Free image data
        }

    #if defined(SUPPORT_MODULE_RSHAPES) && defined(SUPPORT_MODULE_RTEXT)
        // Display the recording indicator every half-second
        if ((int)(RL_GetTime()/0.5)%2 == 1)
        {
            RL_DrawCircle(30, CORE.Window.screen.height - 20, 10, RL_MAROON);                 // WARNING: Module required: rshapes
            RL_DrawText("GIF RECORDING", 50, CORE.Window.screen.height - 25, 10, RL_RED);     // WARNING: Module required: rtext
        }
    #endif

        rlDrawRenderBatchActive();  // Update and draw internal render batch
    }
#endif

#if defined(SUPPORT_AUTOMATION_EVENTS)
    if (automationEventRecording) RecordAutomationEvent();    // Event recording
#endif

#if !defined(SUPPORT_CUSTOM_FRAME_CONTROL)
    RL_SwapScreenBuffer();                  // Copy back buffer to front buffer (screen)

    // Frame time control system
    CORE.Time.current = RL_GetTime();
    CORE.Time.draw = CORE.Time.current - CORE.Time.previous;
    CORE.Time.previous = CORE.Time.current;

    CORE.Time.frame = CORE.Time.update + CORE.Time.draw;

    // Wait for some milliseconds...
    if (CORE.Time.frame < CORE.Time.target)
    {
        RL_WaitTime(CORE.Time.target - CORE.Time.frame);

        CORE.Time.current = RL_GetTime();
        double waitTime = CORE.Time.current - CORE.Time.previous;
        CORE.Time.previous = CORE.Time.current;

        CORE.Time.frame += waitTime;    // Total frame time: update + draw + wait
    }

    RL_PollInputEvents();      // Poll user events (before next frame update)
#endif

#if defined(SUPPORT_SCREEN_CAPTURE)
    if (RL_IsKeyPressed(KEY_F12))
    {
#if defined(SUPPORT_GIF_RECORDING)
        if (RL_IsKeyDown(KEY_LEFT_CONTROL))
        {
            if (gifRecording)
            {
                gifRecording = false;

                MsfGifResult result = msf_gif_end(&gifState);

                RL_SaveFileData(RL_TextFormat("%s/screenrec%03i.gif", CORE.Storage.basePath, screenshotCounter), result.data, (unsigned int)result.dataSize);
                msf_gif_free(result);

                TRACELOG(LOG_INFO, "SYSTEM: Finish animated GIF recording");
            }
            else
            {
                gifRecording = true;
                gifFrameCounter = 0;

                RL_Vector2 scale = RL_GetWindowScaleDPI();
                msf_gif_begin(&gifState, (int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y));
                screenshotCounter++;

                TRACELOG(LOG_INFO, "SYSTEM: Start animated GIF recording: %s", RL_TextFormat("screenrec%03i.gif", screenshotCounter));
            }
        }
        else
#endif  // SUPPORT_GIF_RECORDING
        {
            RL_TakeScreenshot(RL_TextFormat("screenshot%03i.png", screenshotCounter));
            screenshotCounter++;
        }
    }
#endif  // SUPPORT_SCREEN_CAPTURE

    CORE.Time.frameCounter++;
}

// Initialize 2D mode with custom camera (2D)
void RL_BeginMode2D(RL_Camera2D camera)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlLoadIdentity();               // Reset current matrix (modelview)

    // Apply 2d camera transformation to modelview
    rlMultMatrixf(MatrixToFloat(RL_GetCameraMatrix2D(camera)));
}

// Ends 2D mode with custom camera
void RL_EndMode2D(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlLoadIdentity();               // Reset current matrix (modelview)

    if (rlGetActiveFramebuffer() == 0) rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required
}

// Initializes 3D mode with custom camera (3D)
void RL_BeginMode3D(RL_Camera camera)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlPushMatrix();                 // Save previous matrix, which contains the settings for the 2d ortho projection
    rlLoadIdentity();               // Reset current matrix (projection)

    float aspect = (float)CORE.Window.currentFbo.width/(float)CORE.Window.currentFbo.height;

    // NOTE: zNear and zFar values are important when computing depth buffer values
    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Setup perspective projection
        double top = rlGetCullDistanceNear()*tan(camera.fovy*0.5*DEG2RAD);
        double right = top*aspect;

        rlFrustum(-right, right, -top, top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        // Setup orthographic projection
        double top = camera.fovy/2.0;
        double right = top*aspect;

        rlOrtho(-right, right, -top,top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    // Setup RL_Camera view
    RL_Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);
    rlMultMatrixf(MatrixToFloat(matView));      // Multiply modelview matrix by view matrix (camera)

    rlEnableDepthTest();            // Enable DEPTH_TEST for 3D
}

// Ends 3D mode and returns to default 2D orthographic mode
void RL_EndMode3D(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlPopMatrix();                  // Restore previous matrix (projection) from matrix stack

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    if (rlGetActiveFramebuffer() == 0) rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required

    rlDisableDepthTest();           // Disable DEPTH_TEST for 2D
}

// Initializes render texture for drawing
void RL_BeginTextureMode(RL_RenderTexture2D target)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlEnableFramebuffer(target.id); // Enable render target

    // Set viewport and RLGL internal framebuffer size
    rlViewport(0, 0, target.texture.width, target.texture.height);
    rlSetFramebufferWidth(target.texture.width);
    rlSetFramebufferHeight(target.texture.height);

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlLoadIdentity();               // Reset current matrix (projection)

    // Set orthographic projection to current framebuffer size
    // NOTE: Configured top-left corner as (0, 0)
    rlOrtho(0, target.texture.width, target.texture.height, 0, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    //rlScalef(0.0f, -1.0f, 0.0f);  // Flip Y-drawing (?)

    // Setup current width/height for proper aspect ratio
    // calculation when using RL_BeginMode3D()
    CORE.Window.currentFbo.width = target.texture.width;
    CORE.Window.currentFbo.height = target.texture.height;
    CORE.Window.usingFbo = true;
}

// Ends drawing to render texture
void RL_EndTextureMode(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlDisableFramebuffer();         // Disable render target (fbo)

    // Set viewport to default framebuffer size
    SetupViewport(CORE.Window.render.width, CORE.Window.render.height);

    // Go back to the modelview state from RL_BeginDrawing since we are back to the default FBO
    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required

    // Reset current fbo to screen size
    CORE.Window.currentFbo.width = CORE.Window.render.width;
    CORE.Window.currentFbo.height = CORE.Window.render.height;
    CORE.Window.usingFbo = false;
}

// Begin custom shader mode
void RL_BeginShaderMode(RL_Shader shader)
{
    rlSetShader(shader.id, shader.locs);
}

// End custom shader mode (returns to default shader)
void RL_EndShaderMode(void)
{
    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
}

// Begin blending mode (alpha, additive, multiplied, subtract, custom)
// NOTE: Blend modes supported are enumerated in RL_BlendMode enum
void RL_BeginBlendMode(int mode)
{
    rlSetBlendMode(mode);
}

// End blending mode (reset to default: alpha blending)
void RL_EndBlendMode(void)
{
    rlSetBlendMode(BLEND_ALPHA);
}

// Begin scissor mode (define screen area for following drawing)
// NOTE: Scissor rec refers to bottom-left corner, we change it to upper-left
void RL_BeginScissorMode(int x, int y, int width, int height)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlEnableScissorTest();

#if defined(__APPLE__)
    if (!CORE.Window.usingFbo)
    {
        RL_Vector2 scale = RL_GetWindowScaleDPI();
        rlScissor((int)(x*scale.x), (int)(RL_GetScreenHeight()*scale.y - (((y + height)*scale.y))), (int)(width*scale.x), (int)(height*scale.y));
    }
#else
    if (!CORE.Window.usingFbo && ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0))
    {
        RL_Vector2 scale = RL_GetWindowScaleDPI();
        rlScissor((int)(x*scale.x), (int)(CORE.Window.currentFbo.height - (y + height)*scale.y), (int)(width*scale.x), (int)(height*scale.y));
    }
#endif
    else
    {
        rlScissor(x, CORE.Window.currentFbo.height - (y + height), width, height);
    }
}

// End scissor mode
void RL_EndScissorMode(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch
    rlDisableScissorTest();
}

//----------------------------------------------------------------------------------
// Module Functions Definition: VR Stereo Rendering
//----------------------------------------------------------------------------------

// Begin VR drawing configuration
void RL_BeginVrStereoMode(RL_VrStereoConfig config)
{
    rlEnableStereoRender();

    // Set stereo render matrices
    rlSetMatrixProjectionStereo(config.projection[0], config.projection[1]);
    rlSetMatrixViewOffsetStereo(config.viewOffset[0], config.viewOffset[1]);
}

// End VR drawing process (and desktop mirror)
void RL_EndVrStereoMode(void)
{
    rlDisableStereoRender();
}

// Load VR stereo config for VR simulator device parameters
RL_VrStereoConfig RL_LoadVrStereoConfig(RL_VrDeviceInfo device)
{
    RL_VrStereoConfig config = { 0 };

    if (rlGetVersion() != RL_OPENGL_11)
    {
        // Compute aspect ratio
        float aspect = ((float)device.hResolution*0.5f)/(float)device.vResolution;

        // Compute lens parameters
        float lensShift = (device.hScreenSize*0.25f - device.lensSeparationDistance*0.5f)/device.hScreenSize;
        config.leftLensCenter[0] = 0.25f + lensShift;
        config.leftLensCenter[1] = 0.5f;
        config.rightLensCenter[0] = 0.75f - lensShift;
        config.rightLensCenter[1] = 0.5f;
        config.leftScreenCenter[0] = 0.25f;
        config.leftScreenCenter[1] = 0.5f;
        config.rightScreenCenter[0] = 0.75f;
        config.rightScreenCenter[1] = 0.5f;

        // Compute distortion scale parameters
        // NOTE: To get lens max radius, lensShift must be normalized to [-1..1]
        float lensRadius = fabsf(-1.0f - 4.0f*lensShift);
        float lensRadiusSq = lensRadius*lensRadius;
        float distortionScale = device.lensDistortionValues[0] +
                                device.lensDistortionValues[1]*lensRadiusSq +
                                device.lensDistortionValues[2]*lensRadiusSq*lensRadiusSq +
                                device.lensDistortionValues[3]*lensRadiusSq*lensRadiusSq*lensRadiusSq;

        float normScreenWidth = 0.5f;
        float normScreenHeight = 1.0f;
        config.scaleIn[0] = 2.0f/normScreenWidth;
        config.scaleIn[1] = 2.0f/normScreenHeight/aspect;
        config.scale[0] = normScreenWidth*0.5f/distortionScale;
        config.scale[1] = normScreenHeight*0.5f*aspect/distortionScale;

        // Fovy is normally computed with: 2*atan2f(device.vScreenSize, 2*device.eyeToScreenDistance)
        // ...but with lens distortion it is increased (see Oculus SDK Documentation)
        float fovy = 2.0f*atan2f(device.vScreenSize*0.5f*distortionScale, device.eyeToScreenDistance);     // Really need distortionScale?
       // float fovy = 2.0f*(float)atan2f(device.vScreenSize*0.5f, device.eyeToScreenDistance);

        // Compute camera projection matrices
        float projOffset = 4.0f*lensShift;      // Scaled to projection space coordinates [-1..1]
        RL_Matrix proj = MatrixPerspective(fovy, aspect, rlGetCullDistanceNear(), rlGetCullDistanceFar());

        config.projection[0] = MatrixMultiply(proj, MatrixTranslate(projOffset, 0.0f, 0.0f));
        config.projection[1] = MatrixMultiply(proj, MatrixTranslate(-projOffset, 0.0f, 0.0f));

        // Compute camera transformation matrices
        // NOTE: RL_Camera movement might seem more natural if we model the head
        // Our axis of rotation is the base of our head, so we might want to add
        // some y (base of head to eye level) and -z (center of head to eye protrusion) to the camera positions
        config.viewOffset[0] = MatrixTranslate(device.interpupillaryDistance*0.5f, 0.075f, 0.045f);
        config.viewOffset[1] = MatrixTranslate(-device.interpupillaryDistance*0.5f, 0.075f, 0.045f);

        // Compute eyes Viewports
        /*
        config.eyeViewportRight[0] = 0;
        config.eyeViewportRight[1] = 0;
        config.eyeViewportRight[2] = device.hResolution/2;
        config.eyeViewportRight[3] = device.vResolution;

        config.eyeViewportLeft[0] = device.hResolution/2;
        config.eyeViewportLeft[1] = 0;
        config.eyeViewportLeft[2] = device.hResolution/2;
        config.eyeViewportLeft[3] = device.vResolution;
        */
    }
    else TRACELOG(LOG_WARNING, "RLGL: VR Simulator not supported on OpenGL 1.1");

    return config;
}

// Unload VR stereo config properties
void RL_UnloadVrStereoConfig(RL_VrStereoConfig config)
{
    TRACELOG(LOG_INFO, "RL_UnloadVrStereoConfig not implemented in rcore.c");
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Shaders Management
//----------------------------------------------------------------------------------

// Load shader from files and bind default locations
// NOTE: If shader string is NULL, using default vertex/fragment shaders
RL_Shader RL_LoadShader(const char *vsFileName, const char *fsFileName)
{
    RL_Shader shader = { 0 };

    char *vShaderStr = NULL;
    char *fShaderStr = NULL;

    if (vsFileName != NULL) vShaderStr = RL_LoadFileText(vsFileName);
    if (fsFileName != NULL) fShaderStr = RL_LoadFileText(fsFileName);

    shader = RL_LoadShaderFromMemory(vShaderStr, fShaderStr);

    RL_UnloadFileText(vShaderStr);
    RL_UnloadFileText(fShaderStr);

    return shader;
}

// Load shader from code strings and bind default locations
RL_Shader RL_LoadShaderFromMemory(const char *vsCode, const char *fsCode)
{
    RL_Shader shader = { 0 };

    shader.id = rlLoadShaderCode(vsCode, fsCode);

    // After shader loading, we TRY to set default location names
    if (shader.id > 0)
    {
        // Default shader attribute locations have been binded before linking:
        //          vertex position location    = 0
        //          vertex texcoord location    = 1
        //          vertex normal location      = 2
        //          vertex color location       = 3
        //          vertex tangent location     = 4
        //          vertex texcoord2 location   = 5
        //          vertex boneIds location     = 6
        //          vertex boneWeights location = 7

        // NOTE: If any location is not found, loc point becomes -1

        shader.locs = (int *)RL_CALLOC(RL_MAX_SHADER_LOCATIONS, sizeof(int));

        // All locations reset to -1 (no location)
        for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; i++) shader.locs[i] = -1;

        // Get handles to GLSL input attribute locations
        shader.locs[SHADER_LOC_VERTEX_POSITION] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_POSITION);
        shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TEXCOORD);
        shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TEXCOORD2);
        shader.locs[SHADER_LOC_VERTEX_NORMAL] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_NORMAL);
        shader.locs[SHADER_LOC_VERTEX_TANGENT] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TANGENT);
        shader.locs[SHADER_LOC_VERTEX_COLOR] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_COLOR);
        shader.locs[SHADER_LOC_VERTEX_BONEIDS] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_BONEIDS);
        shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_BONEWEIGHTS);

        // Get handles to GLSL uniform locations (vertex shader)
        shader.locs[SHADER_LOC_MATRIX_MVP] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_MVP);
        shader.locs[SHADER_LOC_MATRIX_VIEW] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_VIEW);
        shader.locs[SHADER_LOC_MATRIX_PROJECTION] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_PROJECTION);
        shader.locs[SHADER_LOC_MATRIX_MODEL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_MODEL);
        shader.locs[SHADER_LOC_MATRIX_NORMAL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_NORMAL);
        shader.locs[SHADER_LOC_BONE_MATRICES] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_BONE_MATRICES);

        // Get handles to GLSL uniform locations (fragment shader)
        shader.locs[SHADER_LOC_COLOR_DIFFUSE] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_COLOR);
        shader.locs[SHADER_LOC_MAP_DIFFUSE] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE0);  // SHADER_LOC_MAP_ALBEDO
        shader.locs[SHADER_LOC_MAP_SPECULAR] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE1); // SHADER_LOC_MAP_METALNESS
        shader.locs[SHADER_LOC_MAP_NORMAL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE2);
    }

    return shader;
}

// Check if a shader is ready
bool RL_IsShaderReady(RL_Shader shader)
{
    return ((shader.id > 0) &&          // Validate shader id (loaded successfully)
            (shader.locs != NULL));     // Validate memory has been allocated for default shader locations

    // The following locations are tried to be set automatically (locs[i] >= 0),
    // any of them can be checked for validation but the only mandatory one is, afaik, SHADER_LOC_VERTEX_POSITION
    // NOTE: Users can also setup manually their own attributes/uniforms and do not used the default raylib ones

    // Vertex shader attribute locations (default)
    // shader.locs[SHADER_LOC_VERTEX_POSITION]      // Set by default internal shader
    // shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]    // Set by default internal shader
    // shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]
    // shader.locs[SHADER_LOC_VERTEX_NORMAL]
    // shader.locs[SHADER_LOC_VERTEX_TANGENT]
    // shader.locs[SHADER_LOC_VERTEX_COLOR]         // Set by default internal shader

    // Vertex shader uniform locations (default)
    // shader.locs[SHADER_LOC_MATRIX_MVP]           // Set by default internal shader
    // shader.locs[SHADER_LOC_MATRIX_VIEW]
    // shader.locs[SHADER_LOC_MATRIX_PROJECTION]
    // shader.locs[SHADER_LOC_MATRIX_MODEL]
    // shader.locs[SHADER_LOC_MATRIX_NORMAL]

    // Fragment shader uniform locations (default)
    // shader.locs[SHADER_LOC_COLOR_DIFFUSE]        // Set by default internal shader
    // shader.locs[SHADER_LOC_MAP_DIFFUSE]          // Set by default internal shader
    // shader.locs[SHADER_LOC_MAP_SPECULAR]
    // shader.locs[SHADER_LOC_MAP_NORMAL]
}

// Unload shader from GPU memory (VRAM)
void RL_UnloadShader(RL_Shader shader)
{
    if (shader.id != rlGetShaderIdDefault())
    {
        rlUnloadShaderProgram(shader.id);

        // NOTE: If shader loading failed, it should be 0
        RL_FREE(shader.locs);
    }
}

// Get shader uniform location
int RL_GetShaderLocation(RL_Shader shader, const char *uniformName)
{
    return rlGetLocationUniform(shader.id, uniformName);
}

// Get shader attribute location
int RL_GetShaderLocationAttrib(RL_Shader shader, const char *attribName)
{
    return rlGetLocationAttrib(shader.id, attribName);
}

// Set shader uniform value
void RL_SetShaderValue(RL_Shader shader, int locIndex, const void *value, int uniformType)
{
    RL_SetShaderValueV(shader, locIndex, value, uniformType, 1);
}

// Set shader uniform value vector
void RL_SetShaderValueV(RL_Shader shader, int locIndex, const void *value, int uniformType, int count)
{
    if (locIndex > -1)
    {
        rlEnableShader(shader.id);
        rlSetUniform(locIndex, value, uniformType, count);
        //rlDisableShader();      // Avoid resetting current shader program, in case other uniforms are set
    }
}

// Set shader uniform value (matrix 4x4)
void RL_SetShaderValueMatrix(RL_Shader shader, int locIndex, RL_Matrix mat)
{
    if (locIndex > -1)
    {
        rlEnableShader(shader.id);
        rlSetUniformMatrix(locIndex, mat);
        //rlDisableShader();
    }
}

// Set shader uniform value for texture
void RL_SetShaderValueTexture(RL_Shader shader, int locIndex, RL_Texture2D texture)
{
    if (locIndex > -1)
    {
        rlEnableShader(shader.id);
        rlSetUniformSampler(locIndex, texture.id);
        //rlDisableShader();
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Screen-space Queries
//----------------------------------------------------------------------------------

// Get a ray trace from screen position (i.e mouse)
RL_Ray RL_GetScreenToWorldRay(RL_Vector2 position, RL_Camera camera)
{
    RL_Ray ray = RL_GetScreenToWorldRayEx(position, camera, RL_GetScreenWidth(), RL_GetScreenHeight());

    return ray;
}

// Get a ray trace from the screen position (i.e mouse) within a specific section of the screen
RL_Ray RL_GetScreenToWorldRayEx(RL_Vector2 position, RL_Camera camera, int width, int height)
{
    RL_Ray ray = { 0 };

    // Calculate normalized device coordinates
    // NOTE: y value is negative
    float x = (2.0f*position.x)/(float)width - 1.0f;
    float y = 1.0f - (2.0f*position.y)/(float)height;
    float z = 1.0f;

    // Store values in a vector
    RL_Vector3 deviceCoords = { x, y, z };

    // Calculate view matrix from camera look at
    RL_Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    RL_Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy*DEG2RAD, ((double)width/(double)height), rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        double aspect = (double)width/(double)height;
        double top = camera.fovy/2.0;
        double right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    }

    // Unproject far/near points
    RL_Vector3 nearPoint = Vector3Unproject((RL_Vector3){ deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
    RL_Vector3 farPoint = Vector3Unproject((RL_Vector3){ deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);

    // Unproject the mouse cursor in the near plane
    // We need this as the source position because orthographic projects,
    // compared to perspective doesn't have a convergence point,
    // meaning that the "eye" of the camera is more like a plane than a point
    RL_Vector3 cameraPlanePointerPos = Vector3Unproject((RL_Vector3){ deviceCoords.x, deviceCoords.y, -1.0f }, matProj, matView);

    // Calculate normalized direction vector
    RL_Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

    if (camera.projection == CAMERA_PERSPECTIVE) ray.position = camera.position;
    else if (camera.projection == CAMERA_ORTHOGRAPHIC) ray.position = cameraPlanePointerPos;

    // Apply calculated vectors to ray
    ray.direction = direction;

    return ray;
}

// Get transform matrix for camera
RL_Matrix RL_GetCameraMatrix(RL_Camera camera)
{
    RL_Matrix mat = MatrixLookAt(camera.position, camera.target, camera.up);

    return mat;
}

// Get camera 2d transform matrix
RL_Matrix RL_GetCameraMatrix2D(RL_Camera2D camera)
{
    RL_Matrix matTransform = { 0 };
    // The camera in world-space is set by
    //   1. Move it to target
    //   2. Rotate by -rotation and scale by (1/zoom)
    //      When setting higher scale, it's more intuitive for the world to become bigger (= camera become smaller),
    //      not for the camera getting bigger, hence the invert. Same deal with rotation
    //   3. Move it by (-offset);
    //      Offset defines target transform relative to screen, but since we're effectively "moving" screen (camera)
    //      we need to do it into opposite direction (inverse transform)

    // Having camera transform in world-space, inverse of it gives the modelview transform
    // Since (A*B*C)' = C'*B'*A', the modelview is
    //   1. Move to offset
    //   2. Rotate and Scale
    //   3. Move by -target
    RL_Matrix matOrigin = MatrixTranslate(-camera.target.x, -camera.target.y, 0.0f);
    RL_Matrix matRotation = MatrixRotate((RL_Vector3){ 0.0f, 0.0f, 1.0f }, camera.rotation*DEG2RAD);
    RL_Matrix matScale = MatrixScale(camera.zoom, camera.zoom, 1.0f);
    RL_Matrix matTranslation = MatrixTranslate(camera.offset.x, camera.offset.y, 0.0f);

    matTransform = MatrixMultiply(MatrixMultiply(matOrigin, MatrixMultiply(matScale, matRotation)), matTranslation);

    return matTransform;
}

// Get the screen space position from a 3d world space position
RL_Vector2 RL_GetWorldToScreen(RL_Vector3 position, RL_Camera camera)
{
    RL_Vector2 screenPosition = RL_GetWorldToScreenEx(position, camera, RL_GetScreenWidth(), RL_GetScreenHeight());

    return screenPosition;
}

// Get size position for a 3d world space position (useful for texture drawing)
RL_Vector2 RL_GetWorldToScreenEx(RL_Vector3 position, RL_Camera camera, int width, int height)
{
    // Calculate projection matrix (from perspective instead of frustum
    RL_Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy*DEG2RAD, ((double)width/(double)height), rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        double aspect = (double)width/(double)height;
        double top = camera.fovy/2.0;
        double right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }

    // Calculate view matrix from camera look at (and transpose it)
    RL_Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    // TODO: Why not use Vector3Transform(RL_Vector3 v, RL_Matrix mat)?

    // Convert world position vector to quaternion
    RL_Quaternion worldPos = { position.x, position.y, position.z, 1.0f };

    // RL_Transform world position to view
    worldPos = QuaternionTransform(worldPos, matView);

    // RL_Transform result to projection (clip space position)
    worldPos = QuaternionTransform(worldPos, matProj);

    // Calculate normalized device coordinates (inverted y)
    RL_Vector3 ndcPos = { worldPos.x/worldPos.w, -worldPos.y/worldPos.w, worldPos.z/worldPos.w };

    // Calculate 2d screen position vector
    RL_Vector2 screenPosition = { (ndcPos.x + 1.0f)/2.0f*(float)width, (ndcPos.y + 1.0f)/2.0f*(float)height };

    return screenPosition;
}

// Get the screen space position for a 2d camera world space position
RL_Vector2 RL_GetWorldToScreen2D(RL_Vector2 position, RL_Camera2D camera)
{
    RL_Matrix matCamera = RL_GetCameraMatrix2D(camera);
    RL_Vector3 transform = Vector3Transform((RL_Vector3){ position.x, position.y, 0 }, matCamera);

    return (RL_Vector2){ transform.x, transform.y };
}

// Get the world space position for a 2d camera screen space position
RL_Vector2 RL_GetScreenToWorld2D(RL_Vector2 position, RL_Camera2D camera)
{
    RL_Matrix invMatCamera = MatrixInvert(RL_GetCameraMatrix2D(camera));
    RL_Vector3 transform = Vector3Transform((RL_Vector3){ position.x, position.y, 0 }, invMatCamera);

    return (RL_Vector2){ transform.x, transform.y };
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Timming
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//double RL_GetTime(void)

// Set target FPS (maximum)
void RL_SetTargetFPS(int fps)
{
    if (fps < 1) CORE.Time.target = 0.0;
    else CORE.Time.target = 1.0/(double)fps;

    TRACELOG(LOG_INFO, "TIMER: Target time per frame: %02.03f milliseconds", (float)CORE.Time.target*1000.0f);
}

// Get current FPS
// NOTE: We calculate an average framerate
int RL_GetFPS(void)
{
    int fps = 0;

#if !defined(SUPPORT_CUSTOM_FRAME_CONTROL)
    #define FPS_CAPTURE_FRAMES_COUNT    30      // 30 captures
    #define FPS_AVERAGE_TIME_SECONDS   0.5f     // 500 milliseconds
    #define FPS_STEP (FPS_AVERAGE_TIME_SECONDS/FPS_CAPTURE_FRAMES_COUNT)

    static int index = 0;
    static float history[FPS_CAPTURE_FRAMES_COUNT] = { 0 };
    static float average = 0, last = 0;
    float fpsFrame = RL_GetFrameTime();

    // if we reset the window, reset the FPS info
    if (CORE.Time.frameCounter == 0)
    {
        average = 0;
        last = 0;
        index = 0;

        for (int i = 0; i < FPS_CAPTURE_FRAMES_COUNT; i++) history[i] = 0;
    }

    if (fpsFrame == 0) return 0;

    if ((RL_GetTime() - last) > FPS_STEP)
    {
        last = (float)RL_GetTime();
        index = (index + 1)%FPS_CAPTURE_FRAMES_COUNT;
        average -= history[index];
        history[index] = fpsFrame/FPS_CAPTURE_FRAMES_COUNT;
        average += history[index];
    }

    fps = (int)roundf(1.0f/average);
#endif

    return fps;
}

// Get time in seconds for last frame drawn (delta time)
float RL_GetFrameTime(void)
{
    return (float)CORE.Time.frame;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Custom frame control
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//void RL_SwapScreenBuffer(void);
//void RL_PollInputEvents(void);

// Wait for some time (stop program execution)
// NOTE: Sleep() granularity could be around 10 ms, it means, Sleep() could
// take longer than expected... for that reason we use the busy wait loop
// Ref: http://stackoverflow.com/questions/43057578/c-programming-win32-games-sleep-taking-longer-than-expected
// Ref: http://www.geisswerks.com/ryan/FAQS/timing.html --> All about timing on Win32!
void RL_WaitTime(double seconds)
{
    if (seconds < 0) return;    // Security check

#if defined(SUPPORT_BUSY_WAIT_LOOP) || defined(SUPPORT_PARTIALBUSY_WAIT_LOOP)
    double destinationTime = RL_GetTime() + seconds;
#endif

#if defined(SUPPORT_BUSY_WAIT_LOOP)
    while (RL_GetTime() < destinationTime) { }
#else
    #if defined(SUPPORT_PARTIALBUSY_WAIT_LOOP)
        double sleepSeconds = seconds - seconds*0.05;  // NOTE: We reserve a percentage of the time for busy waiting
    #else
        double sleepSeconds = seconds;
    #endif

    // System halt functions
    #if defined(_WIN32)
        Sleep((unsigned long)(sleepSeconds*1000.0));
    #endif
    #if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__EMSCRIPTEN__)
        struct timespec req = { 0 };
        time_t sec = sleepSeconds;
        long nsec = (sleepSeconds - sec)*1000000000L;
        req.tv_sec = sec;
        req.tv_nsec = nsec;

        // NOTE: Use nanosleep() on Unix platforms... usleep() it's deprecated
        while (nanosleep(&req, &req) == -1) continue;
    #endif
    #if defined(__APPLE__)
        usleep(sleepSeconds*1000000.0);
    #endif

    #if defined(SUPPORT_PARTIALBUSY_WAIT_LOOP)
        while (RL_GetTime() < destinationTime) { }
    #endif
#endif
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//void RL_OpenURL(const char *url)

// Set the seed for the random number generator
void RL_SetRandomSeed(unsigned int seed)
{
#if defined(SUPPORT_RPRAND_GENERATOR)
    rprand_set_seed(seed);
#else
    srand(seed);
#endif
}

// Get a random value between min and max included
int RL_GetRandomValue(int min, int max)
{
    int value = 0;

    if (min > max)
    {
        int tmp = max;
        max = min;
        min = tmp;
    }

#if defined(SUPPORT_RPRAND_GENERATOR)
    value = rprand_get_value(min, max);
#else
    // WARNING: Ranges higher than RAND_MAX will return invalid results
    // More specifically, if (max - min) > INT_MAX there will be an overflow,
    // and otherwise if (max - min) > RAND_MAX the random value will incorrectly never exceed a certain threshold
    // NOTE: Depending on the library it can be as low as 32767
    if ((unsigned int)(max - min) > (unsigned int)RAND_MAX)
    {
        TRACELOG(LOG_WARNING, "Invalid RL_GetRandomValue() arguments, range should not be higher than %i", RAND_MAX);
    }

    value = (rand()%(abs(max - min) + 1) + min);
#endif
    return value;
}

// Load random values sequence, no values repeated, min and max included
int *RL_LoadRandomSequence(unsigned int count, int min, int max)
{
    int *values = NULL;

#if defined(SUPPORT_RPRAND_GENERATOR)
    values = rprand_load_sequence(count, min, max);
#else
    if (count > ((unsigned int)abs(max - min) + 1)) return values;  // Security check

    values = (int *)RL_CALLOC(count, sizeof(int));

    int value = 0;
    bool dupValue = false;

    for (int i = 0; i < (int)count;)
    {
        value = (rand()%(abs(max - min) + 1) + min);
        dupValue = false;

        for (int j = 0; j < i; j++)
        {
            if (values[j] == value)
            {
                dupValue = true;
                break;
            }
        }

        if (!dupValue)
        {
            values[i] = value;
            i++;
        }
    }
#endif
    return values;
}

// Unload random values sequence
void RL_UnloadRandomSequence(int *sequence)
{
#if defined(SUPPORT_RPRAND_GENERATOR)
    rprand_unload_sequence(sequence);
#else
    RL_FREE(sequence);
#endif
}

// Takes a screenshot of current screen
// NOTE: Provided fileName should not contain paths, saving to working directory
void RL_TakeScreenshot(const char *fileName)
{
#if defined(SUPPORT_MODULE_RTEXTURES)
    // Security check to (partially) avoid malicious code
    if (strchr(fileName, '\'') != NULL) { TRACELOG(LOG_WARNING, "SYSTEM: Provided fileName could be potentially malicious, avoid [\'] character"); return; }

    RL_Vector2 scale = RL_GetWindowScaleDPI();
    unsigned char *imgData = rlReadScreenPixels((int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y));
    RL_Image image = { imgData, (int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

    char path[512] = { 0 };
    strcpy(path, RL_TextFormat("%s/%s", CORE.Storage.basePath, RL_GetFileName(fileName)));

    RL_ExportImage(image, path);           // WARNING: Module required: rtextures
    RL_FREE(imgData);

    if (RL_FileExists(path)) TRACELOG(LOG_INFO, "SYSTEM: [%s] Screenshot taken successfully", path);
    else TRACELOG(LOG_WARNING, "SYSTEM: [%s] Screenshot could not be saved", path);
#else
    TRACELOG(LOG_WARNING,"IMAGE: RL_ExportImage() requires module: rtextures");
#endif
}

// Setup window configuration flags (view FLAGS)
// NOTE: This function is expected to be called before window creation,
// because it sets up some flags for the window creation process
// To configure window states after creation, just use RL_SetWindowState()
void RL_SetConfigFlags(unsigned int flags)
{
    // Selected flags are set but not evaluated at this point,
    // flag evaluation happens at RL_InitWindow() or RL_SetWindowState()
    CORE.Window.flags |= flags;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: File system
//----------------------------------------------------------------------------------

// Check if the file exists
bool RL_FileExists(const char *fileName)
{
    bool result = false;

#if defined(_WIN32)
    if (_access(fileName, 0) != -1) result = true;
#else
    if (access(fileName, F_OK) != -1) result = true;
#endif

    // NOTE: Alternatively, stat() can be used instead of access()
    //#include <sys/stat.h>
    //struct stat statbuf;
    //if (stat(filename, &statbuf) == 0) result = true;

    return result;
}

// Check file extension
// NOTE: Extensions checking is not case-sensitive
bool RL_IsFileExtension(const char *fileName, const char *ext)
{
    #define MAX_FILE_EXTENSION_LENGTH  16

    bool result = false;
    const char *fileExt = RL_GetFileExtension(fileName);

    if (fileExt != NULL)
    {
#if defined(SUPPORT_MODULE_RTEXT) && defined(SUPPORT_TEXT_MANIPULATION)
        int extCount = 0;
        const char **checkExts = RL_TextSplit(ext, ';', &extCount); // WARNING: Module required: rtext

        char fileExtLower[MAX_FILE_EXTENSION_LENGTH + 1] = { 0 };
        strncpy(fileExtLower, RL_TextToLower(fileExt), MAX_FILE_EXTENSION_LENGTH); // WARNING: Module required: rtext

        for (int i = 0; i < extCount; i++)
        {
            if (strcmp(fileExtLower, RL_TextToLower(checkExts[i])) == 0)
            {
                result = true;
                break;
            }
        }
#else
        if (strcmp(fileExt, ext) == 0) result = true;
#endif
    }

    return result;
}

// Check if a directory path exists
bool RL_DirectoryExists(const char *dirPath)
{
    bool result = false;
    DIR *dir = opendir(dirPath);

    if (dir != NULL)
    {
        result = true;
        closedir(dir);
    }

    return result;
}

// Get file length in bytes
// NOTE: GetFileSize() conflicts with windows.h
int RL_GetFileLength(const char *fileName)
{
    int size = 0;

    // NOTE: On Unix-like systems, it can by used the POSIX system call: stat(),
    // but depending on the platform that call could not be available
    //struct stat result = { 0 };
    //stat(fileName, &result);
    //return result.st_size;

    FILE *file = fopen(fileName, "rb");

    if (file != NULL)
    {
        fseek(file, 0L, SEEK_END);
        long int fileSize = ftell(file);

        // Check for size overflow (INT_MAX)
        if (fileSize > 2147483647) TRACELOG(LOG_WARNING, "[%s] File size overflows expected limit, do not use RL_GetFileLength()", fileName);
        else size = (int)fileSize;

        fclose(file);
    }

    return size;
}

// Get pointer to extension for a filename string (includes the dot: .png)
const char *RL_GetFileExtension(const char *fileName)
{
    const char *dot = strrchr(fileName, '.');

    if (!dot || dot == fileName) return NULL;

    return dot;
}

// String pointer reverse break: returns right-most occurrence of charset in s
static const char *strprbrk(const char *s, const char *charset)
{
    const char *latestMatch = NULL;

    for (; s = strpbrk(s, charset), s != NULL; latestMatch = s++) { }

    return latestMatch;
}

// Get pointer to filename for a path string
const char *RL_GetFileName(const char *filePath)
{
    const char *fileName = NULL;

    if (filePath != NULL) fileName = strprbrk(filePath, "\\/");

    if (fileName == NULL) return filePath;

    return fileName + 1;
}

// Get filename string without extension (uses static string)
const char *RL_GetFileNameWithoutExt(const char *filePath)
{
    #define MAX_FILENAME_LENGTH     256

    static char fileName[MAX_FILENAME_LENGTH] = { 0 };
    memset(fileName, 0, MAX_FILENAME_LENGTH);

    if (filePath != NULL)
    {
        strcpy(fileName, RL_GetFileName(filePath)); // Get filename.ext without path
        int size = (int)strlen(fileName); // Get size in bytes

        for (int i = size; i > 0; i--) // Reverse search '.'
        {
            if (fileName[i] == '.')
            {
                // NOTE: We break on first '.' found
                fileName[i] = '\0';
                break;
            }
        }
    }

    return fileName;
}

// Get directory for a given filePath
const char *RL_GetDirectoryPath(const char *filePath)
{
    /*
    // NOTE: Directory separator is different in Windows and other platforms,
    // fortunately, Windows also support the '/' separator, that's the one should be used
    #if defined(_WIN32)
        char separator = '\\';
    #else
        char separator = '/';
    #endif
    */
    const char *lastSlash = NULL;
    static char dirPath[MAX_FILEPATH_LENGTH] = { 0 };
    memset(dirPath, 0, MAX_FILEPATH_LENGTH);

    // In case provided path does not contain a root drive letter (C:\, D:\) nor leading path separator (\, /),
    // we add the current directory path to dirPath
    if (filePath[1] != ':' && filePath[0] != '\\' && filePath[0] != '/')
    {
        // For security, we set starting path to current directory,
        // obtained path will be concatenated to this
        dirPath[0] = '.';
        dirPath[1] = '/';
    }

    lastSlash = strprbrk(filePath, "\\/");
    if (lastSlash)
    {
        if (lastSlash == filePath)
        {
            // The last and only slash is the leading one: path is in a root directory
            dirPath[0] = filePath[0];
            dirPath[1] = '\0';
        }
        else
        {
            // NOTE: Be careful, strncpy() is not safe, it does not care about '\0'
            char *dirPathPtr = dirPath;
            if ((filePath[1] != ':') && (filePath[0] != '\\') && (filePath[0] != '/')) dirPathPtr += 2;     // Skip drive letter, "C:"
            memcpy(dirPathPtr, filePath, strlen(filePath) - (strlen(lastSlash) - 1));
            dirPath[strlen(filePath) - strlen(lastSlash) + (((filePath[1] != ':') && (filePath[0] != '\\') && (filePath[0] != '/'))? 2 : 0)] = '\0';  // Add '\0' manually
        }
    }

    return dirPath;
}

// Get previous directory path for a given path
const char *RL_GetPrevDirectoryPath(const char *dirPath)
{
    static char prevDirPath[MAX_FILEPATH_LENGTH] = { 0 };
    memset(prevDirPath, 0, MAX_FILEPATH_LENGTH);
    int pathLen = (int)strlen(dirPath);

    if (pathLen <= 3) strcpy(prevDirPath, dirPath);

    for (int i = (pathLen - 1); (i >= 0) && (pathLen > 3); i--)
    {
        if ((dirPath[i] == '\\') || (dirPath[i] == '/'))
        {
            // Check for root: "C:\" or "/"
            if (((i == 2) && (dirPath[1] ==':')) || (i == 0)) i++;

            strncpy(prevDirPath, dirPath, i);
            break;
        }
    }

    return prevDirPath;
}

// Get current working directory
const char *RL_GetWorkingDirectory(void)
{
    static char currentDir[MAX_FILEPATH_LENGTH] = { 0 };
    memset(currentDir, 0, MAX_FILEPATH_LENGTH);

    char *path = GETCWD(currentDir, MAX_FILEPATH_LENGTH - 1);

    return path;
}

const char *RL_GetApplicationDirectory(void)
{
    static char appDir[MAX_FILEPATH_LENGTH] = { 0 };
    memset(appDir, 0, MAX_FILEPATH_LENGTH);

#if defined(_WIN32)
    int len = 0;
#if defined(UNICODE)
    unsigned short widePath[MAX_PATH];
    len = GetModuleFileNameW(NULL, widePath, MAX_PATH);
    len = WideCharToMultiByte(0, 0, widePath, len, appDir, MAX_PATH, NULL, NULL);
#else
    len = GetModuleFileNameA(NULL, appDir, MAX_PATH);
#endif
    if (len > 0)
    {
        for (int i = len; i >= 0; --i)
        {
            if (appDir[i] == '\\')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '\\';
    }

#elif defined(__linux__)
    unsigned int size = sizeof(appDir);
    ssize_t len = readlink("/proc/self/exe", appDir, size);

    if (len > 0)
    {
        for (int i = len; i >= 0; --i)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }
#elif defined(__APPLE__)
    uint32_t size = sizeof(appDir);

    if (_NSGetExecutablePath(appDir, &size) == 0)
    {
        int len = strlen(appDir);
        for (int i = len; i >= 0; --i)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }
#elif defined(__FreeBSD__)
    size_t size = sizeof(appDir);
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};

    if (sysctl(mib, 4, appDir, &size, NULL, 0) == 0)
    {
        int len = strlen(appDir);
        for (int i = len; i >= 0; --i)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }

#endif

    return appDir;
}

// Load directory filepaths
// NOTE: Base path is prepended to the scanned filepaths
// WARNING: Directory is scanned twice, first time to get files count
// No recursive scanning is done!
RL_FilePathList RL_LoadDirectoryFiles(const char *dirPath)
{
    RL_FilePathList files = { 0 };
    unsigned int fileCounter = 0;

    struct dirent *entity;
    DIR *dir = opendir(dirPath);

    if (dir != NULL) // It's a directory
    {
        // SCAN 1: Count files
        while ((entity = readdir(dir)) != NULL)
        {
            // NOTE: We skip '.' (current dir) and '..' (parent dir) filepaths
            if ((strcmp(entity->d_name, ".") != 0) && (strcmp(entity->d_name, "..") != 0)) fileCounter++;
        }

        // Memory allocation for dirFileCount
        files.capacity = fileCounter;
        files.paths = (char **)RL_MALLOC(files.capacity*sizeof(char *));
        for (unsigned int i = 0; i < files.capacity; i++) files.paths[i] = (char *)RL_MALLOC(MAX_FILEPATH_LENGTH*sizeof(char));

        closedir(dir);

        // SCAN 2: Read filepaths
        // NOTE: Directory paths are also registered
        ScanDirectoryFiles(dirPath, &files, NULL);

        // Security check: read files.count should match fileCounter
        if (files.count != files.capacity) TRACELOG(LOG_WARNING, "FILEIO: Read files count do not match capacity allocated");
    }
    else TRACELOG(LOG_WARNING, "FILEIO: Failed to open requested directory");  // Maybe it's a file...

    return files;
}

// Load directory filepaths with extension filtering and recursive directory scan
// NOTE: On recursive loading we do not pre-scan for file count, we use MAX_FILEPATH_CAPACITY
RL_FilePathList RL_LoadDirectoryFilesEx(const char *basePath, const char *filter, bool scanSubdirs)
{
    RL_FilePathList files = { 0 };

    files.capacity = MAX_FILEPATH_CAPACITY;
    files.paths = (char **)RL_CALLOC(files.capacity, sizeof(char *));
    for (unsigned int i = 0; i < files.capacity; i++) files.paths[i] = (char *)RL_CALLOC(MAX_FILEPATH_LENGTH, sizeof(char));

    // WARNING: basePath is always prepended to scanned paths
    if (scanSubdirs) ScanDirectoryFilesRecursively(basePath, &files, filter);
    else ScanDirectoryFiles(basePath, &files, filter);

    return files;
}

// Unload directory filepaths
// WARNING: files.count is not reseted to 0 after unloading
void RL_UnloadDirectoryFiles(RL_FilePathList files)
{
    for (unsigned int i = 0; i < files.capacity; i++) RL_FREE(files.paths[i]);

    RL_FREE(files.paths);
}

// Create directories (including full path requested), returns 0 on success
int MakeDirectory(const char *dirPath)
{
    if ((dirPath == NULL) || (dirPath[0] == '\0')) return 1; // Path is not valid
    if (RL_DirectoryExists(dirPath)) return 0; // Path already exists (is valid)

    // Copy path string to avoid modifying original
    int len = (int)strlen(dirPath) + 1;
    char *pathcpy = (char *)RL_CALLOC(len, 1);
    memcpy(pathcpy, dirPath, len);

    // Iterate over pathcpy, create each subdirectory as needed
    for (int i = 0; (i < len) && (pathcpy[i] != '\0'); i++)
    {
        if (pathcpy[i] == ':') i++;
        else
        {
            if ((pathcpy[i] == '\\') || (pathcpy[i] == '/'))
            {
                pathcpy[i] = '\0';
                if (!RL_DirectoryExists(pathcpy)) MKDIR(pathcpy);
                pathcpy[i] = '/';
            }
        }
    }

    // Create final directory
    if (!RL_DirectoryExists(pathcpy)) MKDIR(pathcpy);

    RL_FREE(pathcpy);

    return 0;
}

// Change working directory, returns true on success
bool RL_ChangeDirectory(const char *dir)
{
    bool result = CHDIR(dir);

    if (result != 0) TRACELOG(LOG_WARNING, "SYSTEM: Failed to change to directory: %s", dir);

    return (result == 0);
}

// Check if a given path point to a file
bool RL_IsPathFile(const char *path)
{
    struct stat result = { 0 };
    stat(path, &result);

    return S_ISREG(result.st_mode);
}

// Check if fileName is valid for the platform/OS
bool RL_IsFileNameValid(const char *fileName)
{
    bool valid = true;

    if ((fileName != NULL) && (fileName[0] != '\0'))
    {
        int length = (int)strlen(fileName);
        bool allPeriods = true;

        for (int i = 0; i < length; i++)
        {
            // Check invalid characters
            if ((fileName[i] == '<') ||
                (fileName[i] == '>') ||
                (fileName[i] == ':') ||
                (fileName[i] == '\"') ||
                (fileName[i] == '/') ||
                (fileName[i] == '\\') ||
                (fileName[i] == '|') ||
                (fileName[i] == '?') ||
                (fileName[i] == '*')) { valid = false; break; }

            // Check non-glyph characters
            if ((unsigned char)fileName[i] < 32) { valid = false; break; }

            // TODO: Check trailing periods/spaces?

            // Check if filename is not all periods
            if (fileName[i] != '.') allPeriods = false;
        }

        if (allPeriods) valid = false;

/*
        if (valid)
        {
            // Check invalid DOS names
            if (length >= 3)
            {
                if (((fileName[0] == 'C') && (fileName[1] == 'O') && (fileName[2] == 'N')) ||   // CON
                    ((fileName[0] == 'P') && (fileName[1] == 'R') && (fileName[2] == 'N')) ||   // PRN
                    ((fileName[0] == 'A') && (fileName[1] == 'U') && (fileName[2] == 'X')) ||   // AUX
                    ((fileName[0] == 'N') && (fileName[1] == 'U') && (fileName[2] == 'L'))) valid = false; // NUL
            }

            if (length >= 4)
            {
                if (((fileName[0] == 'C') && (fileName[1] == 'O') && (fileName[2] == 'M') && ((fileName[3] >= '0') && (fileName[3] <= '9'))) ||  // COM0-9
                    ((fileName[0] == 'L') && (fileName[1] == 'P') && (fileName[2] == 'T') && ((fileName[3] >= '0') && (fileName[3] <= '9')))) valid = false; // LPT0-9
            }
        }
*/
    }

    return valid;
}

// Check if a file has been dropped into window
bool RL_IsFileDropped(void)
{
    bool result = false;

    if (CORE.Window.dropFileCount > 0) result = true;

    return result;
}

// Load dropped filepaths
RL_FilePathList RL_LoadDroppedFiles(void)
{
    RL_FilePathList files = { 0 };

    files.count = CORE.Window.dropFileCount;
    files.paths = CORE.Window.dropFilepaths;

    return files;
}

// Unload dropped filepaths
void RL_UnloadDroppedFiles(RL_FilePathList files)
{
    // WARNING: files pointers are the same as internal ones

    if (files.count > 0)
    {
        for (unsigned int i = 0; i < files.count; i++) RL_FREE(files.paths[i]);

        RL_FREE(files.paths);

        CORE.Window.dropFileCount = 0;
        CORE.Window.dropFilepaths = NULL;
    }
}

// Get file modification time (last write time)
long RL_GetFileModTime(const char *fileName)
{
    struct stat result = { 0 };
    long modTime = 0;

    if (stat(fileName, &result) == 0)
    {
        time_t mod = result.st_mtime;

        modTime = (long)mod;
    }

    return modTime;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Compression and Encoding
//----------------------------------------------------------------------------------

// Compress data (DEFLATE algorithm)
unsigned char *RL_CompressData(const unsigned char *data, int dataSize, int *compDataSize)
{
    #define COMPRESSION_QUALITY_DEFLATE  8

    unsigned char *compData = NULL;

#if defined(SUPPORT_COMPRESSION_API)
    // Compress data and generate a valid DEFLATE stream
    struct sdefl *sdefl = RL_CALLOC(1, sizeof(struct sdefl));   // WARNING: Possible stack overflow, struct sdefl is almost 1MB
    int bounds = sdefl_bound(dataSize);
    compData = (unsigned char *)RL_CALLOC(bounds, 1);

    *compDataSize = sdeflate(sdefl, compData, data, dataSize, COMPRESSION_QUALITY_DEFLATE);   // Compression level 8, same as stbiw
    RL_FREE(sdefl);

    TRACELOG(LOG_INFO, "SYSTEM: Compress data: Original size: %i -> Comp. size: %i", dataSize, *compDataSize);
#endif

    return compData;
}

// Decompress data (DEFLATE algorithm)
unsigned char *RL_DecompressData(const unsigned char *compData, int compDataSize, int *dataSize)
{
    unsigned char *data = NULL;

#if defined(SUPPORT_COMPRESSION_API)
    // Decompress data from a valid DEFLATE stream
    data = (unsigned char *)RL_CALLOC(MAX_DECOMPRESSION_SIZE*1024*1024, 1);
    int length = sinflate(data, MAX_DECOMPRESSION_SIZE*1024*1024, compData, compDataSize);

    // WARNING: RL_REALLOC can make (and leave) data copies in memory, be careful with sensitive compressed data!
    // TODO: Use a different approach, create another buffer, copy data manually to it and wipe original buffer memory
    unsigned char *temp = (unsigned char *)RL_REALLOC(data, length);

    if (temp != NULL) data = temp;
    else TRACELOG(LOG_WARNING, "SYSTEM: Failed to re-allocate required decompression memory");

    *dataSize = length;

    TRACELOG(LOG_INFO, "SYSTEM: Decompress data: Comp. size: %i -> Original size: %i", compDataSize, *dataSize);
#endif

    return data;
}

// Encode data to Base64 string
char *RL_EncodeDataBase64(const unsigned char *data, int dataSize, int *outputSize)
{
    static const unsigned char base64encodeTable[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
    };

    static const int modTable[] = { 0, 2, 1 };

    *outputSize = 4*((dataSize + 2)/3);

    char *encodedData = (char *)RL_MALLOC(*outputSize);

    if (encodedData == NULL) return NULL;   // Security check

    for (int i = 0, j = 0; i < dataSize;)
    {
        unsigned int octetA = (i < dataSize)? (unsigned char)data[i++] : 0;
        unsigned int octetB = (i < dataSize)? (unsigned char)data[i++] : 0;
        unsigned int octetC = (i < dataSize)? (unsigned char)data[i++] : 0;

        unsigned int triple = (octetA << 0x10) + (octetB << 0x08) + octetC;

        encodedData[j++] = base64encodeTable[(triple >> 3*6) & 0x3F];
        encodedData[j++] = base64encodeTable[(triple >> 2*6) & 0x3F];
        encodedData[j++] = base64encodeTable[(triple >> 1*6) & 0x3F];
        encodedData[j++] = base64encodeTable[(triple >> 0*6) & 0x3F];
    }

    for (int i = 0; i < modTable[dataSize%3]; i++) encodedData[*outputSize - 1 - i] = '=';  // Padding character

    return encodedData;
}

// Decode Base64 string data
unsigned char *RL_DecodeDataBase64(const unsigned char *data, int *outputSize)
{
    static const unsigned char base64decodeTable[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0, 0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
        37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
    };

    // Get output size of Base64 input data
    int outSize = 0;
    for (int i = 0; data[4*i] != 0; i++)
    {
        if (data[4*i + 3] == '=')
        {
            if (data[4*i + 2] == '=') outSize += 1;
            else outSize += 2;
        }
        else outSize += 3;
    }

    // Allocate memory to store decoded Base64 data
    unsigned char *decodedData = (unsigned char *)RL_MALLOC(outSize);

    for (int i = 0; i < outSize/3; i++)
    {
        unsigned char a = base64decodeTable[(int)data[4*i]];
        unsigned char b = base64decodeTable[(int)data[4*i + 1]];
        unsigned char c = base64decodeTable[(int)data[4*i + 2]];
        unsigned char d = base64decodeTable[(int)data[4*i + 3]];

        decodedData[3*i] = (a << 2) | (b >> 4);
        decodedData[3*i + 1] = (b << 4) | (c >> 2);
        decodedData[3*i + 2] = (c << 6) | d;
    }

    if (outSize%3 == 1)
    {
        int n = outSize/3;
        unsigned char a = base64decodeTable[(int)data[4*n]];
        unsigned char b = base64decodeTable[(int)data[4*n + 1]];
        decodedData[outSize - 1] = (a << 2) | (b >> 4);
    }
    else if (outSize%3 == 2)
    {
        int n = outSize/3;
        unsigned char a = base64decodeTable[(int)data[4*n]];
        unsigned char b = base64decodeTable[(int)data[4*n + 1]];
        unsigned char c = base64decodeTable[(int)data[4*n + 2]];
        decodedData[outSize - 2] = (a << 2) | (b >> 4);
        decodedData[outSize - 1] = (b << 4) | (c >> 2);
    }

    *outputSize = outSize;
    return decodedData;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Automation Events Recording and Playing
//----------------------------------------------------------------------------------

// Load automation events list from file, NULL for empty list, capacity = MAX_AUTOMATION_EVENTS
RL_AutomationEventList RL_LoadAutomationEventList(const char *fileName)
{
    RL_AutomationEventList list = { 0 };

    // Allocate and empty automation event list, ready to record new events
    list.events = (RL_AutomationEvent *)RL_CALLOC(MAX_AUTOMATION_EVENTS, sizeof(RL_AutomationEvent));
    list.capacity = MAX_AUTOMATION_EVENTS;

#if defined(SUPPORT_AUTOMATION_EVENTS)
    if (fileName == NULL) TRACELOG(LOG_INFO, "AUTOMATION: New empty events list loaded successfully");
    else
    {
        // Load automation events file (binary)
        /*
        //int dataSize = 0;
        //unsigned char *data = RL_LoadFileData(fileName, &dataSize);

        FILE *raeFile = fopen(fileName, "rb");
        unsigned char fileId[4] = { 0 };

        fread(fileId, 1, 4, raeFile);

        if ((fileId[0] == 'r') && (fileId[1] == 'A') && (fileId[2] == 'E') && (fileId[1] == ' '))
        {
            fread(&eventCount, sizeof(int), 1, raeFile);
            TRACELOG(LOG_WARNING, "Events loaded: %i\n", eventCount);
            fread(events, sizeof(RL_AutomationEvent), eventCount, raeFile);
        }

        fclose(raeFile);
        */

        // Load events file (text)
        //unsigned char *buffer = RL_LoadFileText(fileName);
        FILE *raeFile = fopen(fileName, "rt");

        if (raeFile != NULL)
        {
            unsigned int counter = 0;
            char buffer[256] = { 0 };
            char eventDesc[64] = { 0 };

            fgets(buffer, 256, raeFile);

            while (!feof(raeFile))
            {
                switch (buffer[0])
                {
                    case 'c': sscanf(buffer, "c %i", &list.count); break;
                    case 'e':
                    {
                        sscanf(buffer, "e %d %d %d %d %d %d %[^\n]s", &list.events[counter].frame, &list.events[counter].type,
                               &list.events[counter].params[0], &list.events[counter].params[1], &list.events[counter].params[2], &list.events[counter].params[3], eventDesc);

                        counter++;
                    } break;
                    default: break;
                }

                fgets(buffer, 256, raeFile);
            }

            if (counter != list.count)
            {
                TRACELOG(LOG_WARNING, "AUTOMATION: Events read from file [%i] do not mach event count specified [%i]", counter, list.count);
                list.count = counter;
            }

            fclose(raeFile);

            TRACELOG(LOG_INFO, "AUTOMATION: Events file loaded successfully");
        }

        TRACELOG(LOG_INFO, "AUTOMATION: Events loaded from file: %i", list.count);
    }
#endif
    return list;
}

// Unload automation events list from file
void RL_UnloadAutomationEventList(RL_AutomationEventList list)
{
#if defined(SUPPORT_AUTOMATION_EVENTS)
    RL_FREE(list.events);
#endif
}

// Export automation events list as text file
bool RL_ExportAutomationEventList(RL_AutomationEventList list, const char *fileName)
{
    bool success = false;

#if defined(SUPPORT_AUTOMATION_EVENTS)
    // Export events as binary file
    // TODO: Save to memory buffer and RL_SaveFileData()
    /*
    unsigned char fileId[4] = "rAE ";
    FILE *raeFile = fopen(fileName, "wb");
    fwrite(fileId, sizeof(unsigned char), 4, raeFile);
    fwrite(&eventCount, sizeof(int), 1, raeFile);
    fwrite(events, sizeof(RL_AutomationEvent), eventCount, raeFile);
    fclose(raeFile);
    */

    // Export events as text
    // TODO: Save to memory buffer and RL_SaveFileText()
    char *txtData = (char *)RL_CALLOC(256*list.count + 2048, sizeof(char)); // 256 characters per line plus some header

    int byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "#\n");
    byteCount += sprintf(txtData + byteCount, "# Automation events exporter v1.0 - raylib automation events list\n");
    byteCount += sprintf(txtData + byteCount, "#\n");
    byteCount += sprintf(txtData + byteCount, "#    c <events_count>\n");
    byteCount += sprintf(txtData + byteCount, "#    e <frame> <event_type> <param0> <param1> <param2> <param3> // <event_type_name>\n");
    byteCount += sprintf(txtData + byteCount, "#\n");
    byteCount += sprintf(txtData + byteCount, "# more info and bugs-report:  github.com/raysan5/raylib\n");
    byteCount += sprintf(txtData + byteCount, "# feedback and support:       ray[at]raylib.com\n");
    byteCount += sprintf(txtData + byteCount, "#\n");
    byteCount += sprintf(txtData + byteCount, "# Copyright (c) 2023-2024 Ramon Santamaria (@raysan5)\n");
    byteCount += sprintf(txtData + byteCount, "#\n\n");

    // Add events data
    byteCount += sprintf(txtData + byteCount, "c %i\n", list.count);
    for (unsigned int i = 0; i < list.count; i++)
    {
        byteCount += snprintf(txtData + byteCount, 256, "e %i %i %i %i %i %i // Event: %s\n", list.events[i].frame, list.events[i].type,
            list.events[i].params[0], list.events[i].params[1], list.events[i].params[2], list.events[i].params[3], autoEventTypeName[list.events[i].type]);
    }

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    success = RL_SaveFileText(fileName, txtData);

    RL_FREE(txtData);
#endif

    return success;
}

// Setup automation event list to record to
void RL_SetAutomationEventList(RL_AutomationEventList *list)
{
#if defined(SUPPORT_AUTOMATION_EVENTS)
    currentEventList = list;
#endif
}

// Set automation event internal base frame to start recording
void RL_SetAutomationEventBaseFrame(int frame)
{
    CORE.Time.frameCounter = frame;
}

// Start recording automation events (RL_AutomationEventList must be set)
void RL_StartAutomationEventRecording(void)
{
#if defined(SUPPORT_AUTOMATION_EVENTS)
    automationEventRecording = true;
#endif
}

// Stop recording automation events
void RL_StopAutomationEventRecording(void)
{
#if defined(SUPPORT_AUTOMATION_EVENTS)
    automationEventRecording = false;
#endif
}

// Play a recorded automation event
void RL_PlayAutomationEvent(RL_AutomationEvent event)
{
#if defined(SUPPORT_AUTOMATION_EVENTS)
    // WARNING: When should event be played? After/before/replace RL_PollInputEvents()? -> Up to the user!

    if (!automationEventRecording)      // TODO: Allow recording events while playing?
    {
        switch (event.type)
        {
            // Input event
            case INPUT_KEY_UP: CORE.Input.Keyboard.currentKeyState[event.params[0]] = false; break;             // param[0]: key
            case INPUT_KEY_DOWN: {                                                                              // param[0]: key
                CORE.Input.Keyboard.currentKeyState[event.params[0]] = true;

                if (CORE.Input.Keyboard.previousKeyState[event.params[0]] == false)
                {
                    if (CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE)
                    {
                        // Add character to the queue
                        CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount] = event.params[0];
                        CORE.Input.Keyboard.keyPressedQueueCount++;
                    }
                }
            } break;
            case INPUT_MOUSE_BUTTON_UP: CORE.Input.Mouse.currentButtonState[event.params[0]] = false; break;    // param[0]: key
            case INPUT_MOUSE_BUTTON_DOWN: CORE.Input.Mouse.currentButtonState[event.params[0]] = true; break;   // param[0]: key
            case INPUT_MOUSE_POSITION:      // param[0]: x, param[1]: y
            {
                CORE.Input.Mouse.currentPosition.x = (float)event.params[0];
                CORE.Input.Mouse.currentPosition.y = (float)event.params[1];
            } break;
            case INPUT_MOUSE_WHEEL_MOTION:  // param[0]: x delta, param[1]: y delta
            {
                CORE.Input.Mouse.currentWheelMove.x = (float)event.params[0];
                CORE.Input.Mouse.currentWheelMove.y = (float)event.params[1];
            } break;
            case INPUT_TOUCH_UP: CORE.Input.Touch.currentTouchState[event.params[0]] = false; break;            // param[0]: id
            case INPUT_TOUCH_DOWN: CORE.Input.Touch.currentTouchState[event.params[0]] = true; break;           // param[0]: id
            case INPUT_TOUCH_POSITION:      // param[0]: id, param[1]: x, param[2]: y
            {
                CORE.Input.Touch.position[event.params[0]].x = (float)event.params[1];
                CORE.Input.Touch.position[event.params[0]].y = (float)event.params[2];
            } break;
            case INPUT_GAMEPAD_CONNECT: CORE.Input.Gamepad.ready[event.params[0]] = true; break;                // param[0]: gamepad
            case INPUT_GAMEPAD_DISCONNECT: CORE.Input.Gamepad.ready[event.params[0]] = false; break;            // param[0]: gamepad
            case INPUT_GAMEPAD_BUTTON_UP: CORE.Input.Gamepad.currentButtonState[event.params[0]][event.params[1]] = false; break;    // param[0]: gamepad, param[1]: button
            case INPUT_GAMEPAD_BUTTON_DOWN: CORE.Input.Gamepad.currentButtonState[event.params[0]][event.params[1]] = true; break;   // param[0]: gamepad, param[1]: button
            case INPUT_GAMEPAD_AXIS_MOTION: // param[0]: gamepad, param[1]: axis, param[2]: delta
            {
                CORE.Input.Gamepad.axisState[event.params[0]][event.params[1]] = ((float)event.params[2]/32768.0f);
            } break;
    #if defined(SUPPORT_GESTURES_SYSTEM)
            case INPUT_GESTURE: GESTURES.current = event.params[0]; break;     // param[0]: gesture (enum RL_Gesture) -> rgestures.h: GESTURES.current
    #endif
            // Window event
            case WINDOW_CLOSE: CORE.Window.shouldClose = true; break;
            case WINDOW_MAXIMIZE: RL_MaximizeWindow(); break;
            case WINDOW_MINIMIZE: RL_MinimizeWindow(); break;
            case WINDOW_RESIZE: RL_SetWindowSize(event.params[0], event.params[1]); break;

            // Custom event
    #if defined(SUPPORT_SCREEN_CAPTURE)
            case ACTION_TAKE_SCREENSHOT:
            {
                RL_TakeScreenshot(RL_TextFormat("screenshot%03i.png", screenshotCounter));
                screenshotCounter++;
            } break;
    #endif
            case ACTION_SETTARGETFPS: RL_SetTargetFPS(event.params[0]); break;
            default: break;
        }

        TRACELOG(LOG_INFO, "AUTOMATION PLAY: Frame: %i | Event type: %i | Event parameters: %i, %i, %i", event.frame, event.type, event.params[0], event.params[1], event.params[2]);
    }
#endif
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Input Handling: Keyboard
//----------------------------------------------------------------------------------

// Check if a key has been pressed once
bool RL_IsKeyPressed(int key)
{

    bool pressed = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if ((CORE.Input.Keyboard.previousKeyState[key] == 0) && (CORE.Input.Keyboard.currentKeyState[key] == 1)) pressed = true;
    }

    return pressed;
}

// Check if a key has been pressed again
bool RL_IsKeyPressedRepeat(int key)
{
    bool repeat = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if (CORE.Input.Keyboard.keyRepeatInFrame[key] == 1) repeat = true;
    }

    return repeat;
}

// Check if a key is being pressed (key held down)
bool RL_IsKeyDown(int key)
{
    bool down = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if (CORE.Input.Keyboard.currentKeyState[key] == 1) down = true;
    }

    return down;
}

// Check if a key has been released once
bool RL_IsKeyReleased(int key)
{
    bool released = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if ((CORE.Input.Keyboard.previousKeyState[key] == 1) && (CORE.Input.Keyboard.currentKeyState[key] == 0)) released = true;
    }

    return released;
}

// Check if a key is NOT being pressed (key not held down)
bool RL_IsKeyUp(int key)
{
    bool up = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if (CORE.Input.Keyboard.currentKeyState[key] == 0) up = true;
    }

    return up;
}

// Get the last key pressed
int RL_GetKeyPressed(void)
{
    int value = 0;

    if (CORE.Input.Keyboard.keyPressedQueueCount > 0)
    {
        // Get character from the queue head
        value = CORE.Input.Keyboard.keyPressedQueue[0];

        // Shift elements 1 step toward the head
        for (int i = 0; i < (CORE.Input.Keyboard.keyPressedQueueCount - 1); i++)
            CORE.Input.Keyboard.keyPressedQueue[i] = CORE.Input.Keyboard.keyPressedQueue[i + 1];

        // Reset last character in the queue
        CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount - 1] = 0;
        CORE.Input.Keyboard.keyPressedQueueCount--;
    }

    return value;
}

// Get the last char pressed
int RL_GetCharPressed(void)
{
    int value = 0;

    if (CORE.Input.Keyboard.charPressedQueueCount > 0)
    {
        // Get character from the queue head
        value = CORE.Input.Keyboard.charPressedQueue[0];

        // Shift elements 1 step toward the head
        for (int i = 0; i < (CORE.Input.Keyboard.charPressedQueueCount - 1); i++)
            CORE.Input.Keyboard.charPressedQueue[i] = CORE.Input.Keyboard.charPressedQueue[i + 1];

        // Reset last character in the queue
        CORE.Input.Keyboard.charPressedQueue[CORE.Input.Keyboard.charPressedQueueCount - 1] = 0;
        CORE.Input.Keyboard.charPressedQueueCount--;
    }

    return value;
}

// Set a custom key to exit program
// NOTE: default exitKey is set to ESCAPE
void RL_SetExitKey(int key)
{
    CORE.Input.Keyboard.exitKey = key;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Input Handling: Gamepad
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//int RL_SetGamepadMappings(const char *mappings)

// Check if a gamepad is available
bool RL_IsGamepadAvailable(int gamepad)
{
    bool result = false;

    if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad]) result = true;

    return result;
}

// Get gamepad internal name id
const char *RL_GetGamepadName(int gamepad)
{
    return CORE.Input.Gamepad.name[gamepad];
}

// Check if a gamepad button has been pressed once
bool RL_IsGamepadButtonPressed(int gamepad, int button)
{
    bool pressed = false;

    if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button < MAX_GAMEPAD_BUTTONS) &&
        (CORE.Input.Gamepad.previousButtonState[gamepad][button] == 0) && (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 1)) pressed = true;

    return pressed;
}

// Check if a gamepad button is being pressed
bool RL_IsGamepadButtonDown(int gamepad, int button)
{
    bool down = false;

    if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button < MAX_GAMEPAD_BUTTONS) &&
        (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 1)) down = true;

    return down;
}

// Check if a gamepad button has NOT been pressed once
bool RL_IsGamepadButtonReleased(int gamepad, int button)
{
    bool released = false;

    if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button < MAX_GAMEPAD_BUTTONS) &&
        (CORE.Input.Gamepad.previousButtonState[gamepad][button] == 1) && (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 0)) released = true;

    return released;
}

// Check if a gamepad button is NOT being pressed
bool RL_IsGamepadButtonUp(int gamepad, int button)
{
    bool up = false;

    if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button < MAX_GAMEPAD_BUTTONS) &&
        (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 0)) up = true;

    return up;
}

// Get the last gamepad button pressed
int RL_GetGamepadButtonPressed(void)
{
    return CORE.Input.Gamepad.lastButtonPressed;
}

// Get gamepad axis count
int RL_GetGamepadAxisCount(int gamepad)
{
    return CORE.Input.Gamepad.axisCount[gamepad];
}

// Get axis movement vector for a gamepad
float RL_GetGamepadAxisMovement(int gamepad, int axis)
{
    float value = (axis == GAMEPAD_AXIS_LEFT_TRIGGER || axis == GAMEPAD_AXIS_RIGHT_TRIGGER)? -1.0f : 0.0f;

    if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (axis < MAX_GAMEPAD_AXIS)) {
        float movement = value < 0.0f ? CORE.Input.Gamepad.axisState[gamepad][axis] : fabsf(CORE.Input.Gamepad.axisState[gamepad][axis]);

        // 0.1f = GAMEPAD_AXIS_MINIMUM_DRIFT/DELTA
        if (movement > value + 0.1f) value = CORE.Input.Gamepad.axisState[gamepad][axis];
    }

    return value;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Input Handling: Mouse
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//void RL_SetMousePosition(int x, int y)
//void RL_SetMouseCursor(int cursor)

// Check if a mouse button has been pressed once
bool RL_IsMouseButtonPressed(int button)
{
    bool pressed = false;

    if ((CORE.Input.Mouse.currentButtonState[button] == 1) && (CORE.Input.Mouse.previousButtonState[button] == 0)) pressed = true;

    // Map touches to mouse buttons checking
    if ((CORE.Input.Touch.currentTouchState[button] == 1) && (CORE.Input.Touch.previousTouchState[button] == 0)) pressed = true;

    return pressed;
}

// Check if a mouse button is being pressed
bool RL_IsMouseButtonDown(int button)
{
    bool down = false;

    if (CORE.Input.Mouse.currentButtonState[button] == 1) down = true;

    // NOTE: Touches are considered like mouse buttons
    if (CORE.Input.Touch.currentTouchState[button] == 1) down = true;

    return down;
}

// Check if a mouse button has been released once
bool RL_IsMouseButtonReleased(int button)
{
    bool released = false;

    if ((CORE.Input.Mouse.currentButtonState[button] == 0) && (CORE.Input.Mouse.previousButtonState[button] == 1)) released = true;

    // Map touches to mouse buttons checking
    if ((CORE.Input.Touch.currentTouchState[button] == 0) && (CORE.Input.Touch.previousTouchState[button] == 1)) released = true;

    return released;
}

// Check if a mouse button is NOT being pressed
bool RL_IsMouseButtonUp(int button)
{
    bool up = false;

    if (CORE.Input.Mouse.currentButtonState[button] == 0) up = true;

    // NOTE: Touches are considered like mouse buttons
    if (CORE.Input.Touch.currentTouchState[button] == 0) up = true;

    return up;
}

// Get mouse position X
int RL_GetMouseX(void)
{
    int mouseX = (int)((CORE.Input.Mouse.currentPosition.x + CORE.Input.Mouse.offset.x)*CORE.Input.Mouse.scale.x);
    return mouseX;
}

// Get mouse position Y
int RL_GetMouseY(void)
{
    int mouseY = (int)((CORE.Input.Mouse.currentPosition.y + CORE.Input.Mouse.offset.y)*CORE.Input.Mouse.scale.y);
    return mouseY;
}

// Get mouse position XY
RL_Vector2 RL_GetMousePosition(void)
{
    RL_Vector2 position = { 0 };

    position.x = (CORE.Input.Mouse.currentPosition.x + CORE.Input.Mouse.offset.x)*CORE.Input.Mouse.scale.x;
    position.y = (CORE.Input.Mouse.currentPosition.y + CORE.Input.Mouse.offset.y)*CORE.Input.Mouse.scale.y;

    return position;
}

// Get mouse delta between frames
RL_Vector2 RL_GetMouseDelta(void)
{
    RL_Vector2 delta = { 0 };

    delta.x = CORE.Input.Mouse.currentPosition.x - CORE.Input.Mouse.previousPosition.x;
    delta.y = CORE.Input.Mouse.currentPosition.y - CORE.Input.Mouse.previousPosition.y;

    return delta;
}

// Set mouse offset
// NOTE: Useful when rendering to different size targets
void RL_SetMouseOffset(int offsetX, int offsetY)
{
    CORE.Input.Mouse.offset = (RL_Vector2){ (float)offsetX, (float)offsetY };
}

// Set mouse scaling
// NOTE: Useful when rendering to different size targets
void RL_SetMouseScale(float scaleX, float scaleY)
{
    CORE.Input.Mouse.scale = (RL_Vector2){ scaleX, scaleY };
}

// Get mouse wheel movement Y
float RL_GetMouseWheelMove(void)
{
    float result = 0.0f;

    if (fabsf(CORE.Input.Mouse.currentWheelMove.x) > fabsf(CORE.Input.Mouse.currentWheelMove.y)) result = (float)CORE.Input.Mouse.currentWheelMove.x;
    else result = (float)CORE.Input.Mouse.currentWheelMove.y;

    return result;
}

// Get mouse wheel movement X/Y as a vector
RL_Vector2 RL_GetMouseWheelMoveV(void)
{
    RL_Vector2 result = { 0 };

    result = CORE.Input.Mouse.currentWheelMove;

    return result;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Input Handling: Touch
//----------------------------------------------------------------------------------

// Get touch position X for touch point 0 (relative to screen size)
int RL_GetTouchX(void)
{
    int touchX = (int)CORE.Input.Touch.position[0].x;
    return touchX;
}

// Get touch position Y for touch point 0 (relative to screen size)
int RL_GetTouchY(void)
{
    int touchY = (int)CORE.Input.Touch.position[0].y;
    return touchY;
}

// Get touch position XY for a touch point index (relative to screen size)
// TODO: Touch position should be scaled depending on display size and render size
RL_Vector2 RL_GetTouchPosition(int index)
{
    RL_Vector2 position = { -1.0f, -1.0f };

    if (index < MAX_TOUCH_POINTS) position = CORE.Input.Touch.position[index];
    else TRACELOG(LOG_WARNING, "INPUT: Required touch point out of range (Max touch points: %i)", MAX_TOUCH_POINTS);

    return position;
}

// Get touch point identifier for given index
int RL_GetTouchPointId(int index)
{
    int id = -1;

    if (index < MAX_TOUCH_POINTS) id = CORE.Input.Touch.pointId[index];

    return id;
}

// Get number of touch points
int RL_GetTouchPointCount(void)
{
    return CORE.Input.Touch.pointCount;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//int InitPlatform(void)
//void ClosePlatform(void)

// Initialize hi-resolution timer
void InitTimer(void)
{
    // Setting a higher resolution can improve the accuracy of time-out intervals in wait functions
    // However, it can also reduce overall system performance, because the thread scheduler switches tasks more often
    // High resolutions can also prevent the CPU power management system from entering power-saving modes
    // Setting a higher resolution does not improve the accuracy of the high-resolution performance counter
#if defined(_WIN32) && defined(SUPPORT_WINMM_HIGHRES_TIMER) && !defined(SUPPORT_BUSY_WAIT_LOOP) && !defined(PLATFORM_DESKTOP_SDL)
    timeBeginPeriod(1);                 // Setup high-resolution timer to 1ms (granularity of 1-2 ms)
#endif

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__EMSCRIPTEN__)
    struct timespec now = { 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)  // Success
    {
        CORE.Time.base = (unsigned long long int)now.tv_sec*1000000000LLU + (unsigned long long int)now.tv_nsec;
    }
    else TRACELOG(LOG_WARNING, "TIMER: Hi-resolution timer not available");
#endif

    CORE.Time.previous = RL_GetTime();     // Get time as double
}

// Set viewport for a provided width and height
void SetupViewport(int width, int height)
{
    CORE.Window.render.width = width;
    CORE.Window.render.height = height;

    // Set viewport width and height
    // NOTE: We consider render size (scaled) and offset in case black bars are required and
    // render area does not match full display area (this situation is only applicable on fullscreen mode)
#if defined(__APPLE__)
    RL_Vector2 scale = RL_GetWindowScaleDPI();
    rlViewport(CORE.Window.renderOffset.x/2*scale.x, CORE.Window.renderOffset.y/2*scale.y, (CORE.Window.render.width)*scale.x, (CORE.Window.render.height)*scale.y);
#else
    rlViewport(CORE.Window.renderOffset.x/2, CORE.Window.renderOffset.y/2, CORE.Window.render.width, CORE.Window.render.height);
#endif

    rlMatrixMode(RL_PROJECTION);        // Switch to projection matrix
    rlLoadIdentity();                   // Reset current matrix (projection)

    // Set orthographic projection to current framebuffer size
    // NOTE: Configured top-left corner as (0, 0)
    rlOrtho(0, CORE.Window.render.width, CORE.Window.render.height, 0, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);         // Switch back to modelview matrix
    rlLoadIdentity();                   // Reset current matrix (modelview)
}

// Compute framebuffer size relative to screen size and display size
// NOTE: Global variables CORE.Window.render.width/CORE.Window.render.height and CORE.Window.renderOffset.x/CORE.Window.renderOffset.y can be modified
void SetupFramebuffer(int width, int height)
{
    // Calculate CORE.Window.render.width and CORE.Window.render.height, we have the display size (input params) and the desired screen size (global var)
    if ((CORE.Window.screen.width > CORE.Window.display.width) || (CORE.Window.screen.height > CORE.Window.display.height))
    {
        TRACELOG(LOG_WARNING, "DISPLAY: Downscaling required: Screen size (%ix%i) is bigger than display size (%ix%i)", CORE.Window.screen.width, CORE.Window.screen.height, CORE.Window.display.width, CORE.Window.display.height);

        // Downscaling to fit display with border-bars
        float widthRatio = (float)CORE.Window.display.width/(float)CORE.Window.screen.width;
        float heightRatio = (float)CORE.Window.display.height/(float)CORE.Window.screen.height;

        if (widthRatio <= heightRatio)
        {
            CORE.Window.render.width = CORE.Window.display.width;
            CORE.Window.render.height = (int)round((float)CORE.Window.screen.height*widthRatio);
            CORE.Window.renderOffset.x = 0;
            CORE.Window.renderOffset.y = (CORE.Window.display.height - CORE.Window.render.height);
        }
        else
        {
            CORE.Window.render.width = (int)round((float)CORE.Window.screen.width*heightRatio);
            CORE.Window.render.height = CORE.Window.display.height;
            CORE.Window.renderOffset.x = (CORE.Window.display.width - CORE.Window.render.width);
            CORE.Window.renderOffset.y = 0;
        }

        // Screen scaling required
        float scaleRatio = (float)CORE.Window.render.width/(float)CORE.Window.screen.width;
        CORE.Window.screenScale = MatrixScale(scaleRatio, scaleRatio, 1.0f);

        // NOTE: We render to full display resolution!
        // We just need to calculate above parameters for downscale matrix and offsets
        CORE.Window.render.width = CORE.Window.display.width;
        CORE.Window.render.height = CORE.Window.display.height;

        TRACELOG(LOG_WARNING, "DISPLAY: Downscale matrix generated, content will be rendered at (%ix%i)", CORE.Window.render.width, CORE.Window.render.height);
    }
    else if ((CORE.Window.screen.width < CORE.Window.display.width) || (CORE.Window.screen.height < CORE.Window.display.height))
    {
        // Required screen size is smaller than display size
        TRACELOG(LOG_INFO, "DISPLAY: Upscaling required: Screen size (%ix%i) smaller than display size (%ix%i)", CORE.Window.screen.width, CORE.Window.screen.height, CORE.Window.display.width, CORE.Window.display.height);

        if ((CORE.Window.screen.width == 0) || (CORE.Window.screen.height == 0))
        {
            CORE.Window.screen.width = CORE.Window.display.width;
            CORE.Window.screen.height = CORE.Window.display.height;
        }

        // Upscaling to fit display with border-bars
        float displayRatio = (float)CORE.Window.display.width/(float)CORE.Window.display.height;
        float screenRatio = (float)CORE.Window.screen.width/(float)CORE.Window.screen.height;

        if (displayRatio <= screenRatio)
        {
            CORE.Window.render.width = CORE.Window.screen.width;
            CORE.Window.render.height = (int)round((float)CORE.Window.screen.width/displayRatio);
            CORE.Window.renderOffset.x = 0;
            CORE.Window.renderOffset.y = (CORE.Window.render.height - CORE.Window.screen.height);
        }
        else
        {
            CORE.Window.render.width = (int)round((float)CORE.Window.screen.height*displayRatio);
            CORE.Window.render.height = CORE.Window.screen.height;
            CORE.Window.renderOffset.x = (CORE.Window.render.width - CORE.Window.screen.width);
            CORE.Window.renderOffset.y = 0;
        }
    }
    else
    {
        CORE.Window.render.width = CORE.Window.screen.width;
        CORE.Window.render.height = CORE.Window.screen.height;
        CORE.Window.renderOffset.x = 0;
        CORE.Window.renderOffset.y = 0;
    }
}

// Scan all files and directories in a base path
// WARNING: files.paths[] must be previously allocated and
// contain enough space to store all required paths
static void ScanDirectoryFiles(const char *basePath, RL_FilePathList *files, const char *filter)
{
    static char path[MAX_FILEPATH_LENGTH] = { 0 };
    memset(path, 0, MAX_FILEPATH_LENGTH);

    struct dirent *dp = NULL;
    DIR *dir = opendir(basePath);

    if (dir != NULL)
    {
        while ((dp = readdir(dir)) != NULL)
        {
            if ((strcmp(dp->d_name, ".") != 0) &&
                (strcmp(dp->d_name, "..") != 0))
            {
            #if defined(_WIN32)
                sprintf(path, "%s\\%s", basePath, dp->d_name);
            #else
                sprintf(path, "%s/%s", basePath, dp->d_name);
            #endif

                if (filter != NULL)
                {
                    if (RL_IsPathFile(path))
                    {
                        if (RL_IsFileExtension(path, filter))
                        {
                            strcpy(files->paths[files->count], path);
                            files->count++;
                        }
                    }
                    else
                    {
                        if (RL_TextFindIndex(filter, DIRECTORY_FILTER_TAG) >= 0)
                        {
                            strcpy(files->paths[files->count], path);
                            files->count++;
                        }
                    }
                }
                else
                {
                    strcpy(files->paths[files->count], path);
                    files->count++;
                }
            }
        }

        closedir(dir);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: Directory cannot be opened (%s)", basePath);
}

// Scan all files and directories recursively from a base path
static void ScanDirectoryFilesRecursively(const char *basePath, RL_FilePathList *files, const char *filter)
{
    char path[MAX_FILEPATH_LENGTH] = { 0 };
    memset(path, 0, MAX_FILEPATH_LENGTH);

    struct dirent *dp = NULL;
    DIR *dir = opendir(basePath);

    if (dir != NULL)
    {
        while (((dp = readdir(dir)) != NULL) && (files->count < files->capacity))
        {
            if ((strcmp(dp->d_name, ".") != 0) && (strcmp(dp->d_name, "..") != 0))
            {
                // Construct new path from our base path
            #if defined(_WIN32)
                sprintf(path, "%s\\%s", basePath, dp->d_name);
            #else
                sprintf(path, "%s/%s", basePath, dp->d_name);
            #endif

                if (RL_IsPathFile(path))
                {
                    if (filter != NULL)
                    {
                        if (RL_IsFileExtension(path, filter))
                        {
                            strcpy(files->paths[files->count], path);
                            files->count++;
                        }
                    }
                    else
                    {
                        strcpy(files->paths[files->count], path);
                        files->count++;
                    }

                    if (files->count >= files->capacity)
                    {
                        TRACELOG(LOG_WARNING, "FILEIO: Maximum filepath scan capacity reached (%i files)", files->capacity);
                        break;
                    }
                }
                else 
                {
                    if ((filter != NULL) && (RL_TextFindIndex(filter, DIRECTORY_FILTER_TAG) >= 0))
                    {
                        strcpy(files->paths[files->count], path);
                        files->count++;
                    }
                    
                    if (files->count >= files->capacity)
                    {
                        TRACELOG(LOG_WARNING, "FILEIO: Maximum filepath scan capacity reached (%i files)", files->capacity);
                        break;
                    }

                    ScanDirectoryFilesRecursively(path, files, filter);
                }
            }
        }

        closedir(dir);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: Directory cannot be opened (%s)", basePath);
}

#if defined(SUPPORT_AUTOMATION_EVENTS)
// Automation event recording
// NOTE: Recording is by default done at RL_EndDrawing(), before RL_PollInputEvents()
static void RecordAutomationEvent(void)
{
    // Checking events in current frame and save them into currentEventList
    // TODO: How important is the current frame? Could it be modified?

    if (currentEventList->count == currentEventList->capacity) return;    // Security check

    // Keyboard input events recording
    //-------------------------------------------------------------------------------------
    for (int key = 0; key < MAX_KEYBOARD_KEYS; key++)
    {
        // Event type: INPUT_KEY_UP (only saved once)
        if (CORE.Input.Keyboard.previousKeyState[key] && !CORE.Input.Keyboard.currentKeyState[key])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_KEY_UP;
            currentEventList->events[currentEventList->count].params[0] = key;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_KEY_UP | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check

        // Event type: INPUT_KEY_DOWN
        if (CORE.Input.Keyboard.currentKeyState[key])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_KEY_DOWN;
            currentEventList->events[currentEventList->count].params[0] = key;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_KEY_DOWN | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }
    //-------------------------------------------------------------------------------------

    // Mouse input currentEventList->events recording
    //-------------------------------------------------------------------------------------
    for (int button = 0; button < MAX_MOUSE_BUTTONS; button++)
    {
        // Event type: INPUT_MOUSE_BUTTON_UP
        if (CORE.Input.Mouse.previousButtonState[button] && !CORE.Input.Mouse.currentButtonState[button])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_MOUSE_BUTTON_UP;
            currentEventList->events[currentEventList->count].params[0] = button;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_MOUSE_BUTTON_UP | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check

        // Event type: INPUT_MOUSE_BUTTON_DOWN
        if (CORE.Input.Mouse.currentButtonState[button])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_MOUSE_BUTTON_DOWN;
            currentEventList->events[currentEventList->count].params[0] = button;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_MOUSE_BUTTON_DOWN | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }

    // Event type: INPUT_MOUSE_POSITION (only saved if changed)
    if (((int)CORE.Input.Mouse.currentPosition.x != (int)CORE.Input.Mouse.previousPosition.x) ||
        ((int)CORE.Input.Mouse.currentPosition.y != (int)CORE.Input.Mouse.previousPosition.y))
    {
        currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
        currentEventList->events[currentEventList->count].type = INPUT_MOUSE_POSITION;
        currentEventList->events[currentEventList->count].params[0] = (int)CORE.Input.Mouse.currentPosition.x;
        currentEventList->events[currentEventList->count].params[1] = (int)CORE.Input.Mouse.currentPosition.y;
        currentEventList->events[currentEventList->count].params[2] = 0;

        TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_MOUSE_POSITION | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
        currentEventList->count++;

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }

    // Event type: INPUT_MOUSE_WHEEL_MOTION
    if (((int)CORE.Input.Mouse.currentWheelMove.x != (int)CORE.Input.Mouse.previousWheelMove.x) ||
        ((int)CORE.Input.Mouse.currentWheelMove.y != (int)CORE.Input.Mouse.previousWheelMove.y))
    {
        currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
        currentEventList->events[currentEventList->count].type = INPUT_MOUSE_WHEEL_MOTION;
        currentEventList->events[currentEventList->count].params[0] = (int)CORE.Input.Mouse.currentWheelMove.x;
        currentEventList->events[currentEventList->count].params[1] = (int)CORE.Input.Mouse.currentWheelMove.y;
        currentEventList->events[currentEventList->count].params[2] = 0;

        TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_MOUSE_WHEEL_MOTION | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
        currentEventList->count++;

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }
    //-------------------------------------------------------------------------------------

    // Touch input currentEventList->events recording
    //-------------------------------------------------------------------------------------
    for (int id = 0; id < MAX_TOUCH_POINTS; id++)
    {
        // Event type: INPUT_TOUCH_UP
        if (CORE.Input.Touch.previousTouchState[id] && !CORE.Input.Touch.currentTouchState[id])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_TOUCH_UP;
            currentEventList->events[currentEventList->count].params[0] = id;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_TOUCH_UP | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check

        // Event type: INPUT_TOUCH_DOWN
        if (CORE.Input.Touch.currentTouchState[id])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_TOUCH_DOWN;
            currentEventList->events[currentEventList->count].params[0] = id;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_TOUCH_DOWN | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check

        // Event type: INPUT_TOUCH_POSITION
        // TODO: It requires the id!
        /*
        if (((int)CORE.Input.Touch.currentPosition[id].x != (int)CORE.Input.Touch.previousPosition[id].x) ||
            ((int)CORE.Input.Touch.currentPosition[id].y != (int)CORE.Input.Touch.previousPosition[id].y))
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_TOUCH_POSITION;
            currentEventList->events[currentEventList->count].params[0] = id;
            currentEventList->events[currentEventList->count].params[1] = (int)CORE.Input.Touch.currentPosition[id].x;
            currentEventList->events[currentEventList->count].params[2] = (int)CORE.Input.Touch.currentPosition[id].y;

            TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_TOUCH_POSITION | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
            currentEventList->count++;
        }
        */

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }
    //-------------------------------------------------------------------------------------

    // Gamepad input currentEventList->events recording
    //-------------------------------------------------------------------------------------
    for (int gamepad = 0; gamepad < MAX_GAMEPADS; gamepad++)
    {
        // Event type: INPUT_GAMEPAD_CONNECT
        /*
        if ((CORE.Input.Gamepad.currentState[gamepad] != CORE.Input.Gamepad.previousState[gamepad]) &&
            (CORE.Input.Gamepad.currentState[gamepad])) // Check if changed to ready
        {
            // TODO: Save gamepad connect event
        }
        */

        // Event type: INPUT_GAMEPAD_DISCONNECT
        /*
        if ((CORE.Input.Gamepad.currentState[gamepad] != CORE.Input.Gamepad.previousState[gamepad]) &&
            (!CORE.Input.Gamepad.currentState[gamepad])) // Check if changed to not-ready
        {
            // TODO: Save gamepad disconnect event
        }
        */

        for (int button = 0; button < MAX_GAMEPAD_BUTTONS; button++)
        {
            // Event type: INPUT_GAMEPAD_BUTTON_UP
            if (CORE.Input.Gamepad.previousButtonState[gamepad][button] && !CORE.Input.Gamepad.currentButtonState[gamepad][button])
            {
                currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
                currentEventList->events[currentEventList->count].type = INPUT_GAMEPAD_BUTTON_UP;
                currentEventList->events[currentEventList->count].params[0] = gamepad;
                currentEventList->events[currentEventList->count].params[1] = button;
                currentEventList->events[currentEventList->count].params[2] = 0;

                TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_GAMEPAD_BUTTON_UP | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
                currentEventList->count++;
            }

            if (currentEventList->count == currentEventList->capacity) return;    // Security check

            // Event type: INPUT_GAMEPAD_BUTTON_DOWN
            if (CORE.Input.Gamepad.currentButtonState[gamepad][button])
            {
                currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
                currentEventList->events[currentEventList->count].type = INPUT_GAMEPAD_BUTTON_DOWN;
                currentEventList->events[currentEventList->count].params[0] = gamepad;
                currentEventList->events[currentEventList->count].params[1] = button;
                currentEventList->events[currentEventList->count].params[2] = 0;

                TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_GAMEPAD_BUTTON_DOWN | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
                currentEventList->count++;
            }

            if (currentEventList->count == currentEventList->capacity) return;    // Security check
        }

        for (int axis = 0; axis < MAX_GAMEPAD_AXIS; axis++)
        {
            // Event type: INPUT_GAMEPAD_AXIS_MOTION
            float defaultMovement = (axis == GAMEPAD_AXIS_LEFT_TRIGGER || axis == GAMEPAD_AXIS_RIGHT_TRIGGER)? -1.0f : 0.0f;
            if (RL_GetGamepadAxisMovement(gamepad, axis) != defaultMovement)
            {
                currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
                currentEventList->events[currentEventList->count].type = INPUT_GAMEPAD_AXIS_MOTION;
                currentEventList->events[currentEventList->count].params[0] = gamepad;
                currentEventList->events[currentEventList->count].params[1] = axis;
                currentEventList->events[currentEventList->count].params[2] = (int)(CORE.Input.Gamepad.axisState[gamepad][axis]*32768.0f);

                TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_GAMEPAD_AXIS_MOTION | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
                currentEventList->count++;
            }

            if (currentEventList->count == currentEventList->capacity) return;    // Security check
        }
    }
    //-------------------------------------------------------------------------------------

#if defined(SUPPORT_GESTURES_SYSTEM)
    // Gestures input currentEventList->events recording
    //-------------------------------------------------------------------------------------
    if (GESTURES.current != GESTURE_NONE)
    {
        // Event type: INPUT_GESTURE
        currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
        currentEventList->events[currentEventList->count].type = INPUT_GESTURE;
        currentEventList->events[currentEventList->count].params[0] = GESTURES.current;
        currentEventList->events[currentEventList->count].params[1] = 0;
        currentEventList->events[currentEventList->count].params[2] = 0;

        TRACELOG(LOG_INFO, "AUTOMATION: Frame: %i | Event type: INPUT_GESTURE | Event parameters: %i, %i, %i", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]);
        currentEventList->count++;

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }
    //-------------------------------------------------------------------------------------
#endif
}
#endif

#if !defined(SUPPORT_MODULE_RTEXT)
// Formatting of text with variables to 'embed'
// WARNING: String returned will expire after this function is called MAX_TEXTFORMAT_BUFFERS times
const char *RL_TextFormat(const char *text, ...)
{
#ifndef MAX_TEXTFORMAT_BUFFERS
    #define MAX_TEXTFORMAT_BUFFERS      4        // Maximum number of static buffers for text formatting
#endif
#ifndef MAX_TEXT_BUFFER_LENGTH
    #define MAX_TEXT_BUFFER_LENGTH   1024        // Maximum size of static text buffer
#endif

    // We create an array of buffers so strings don't expire until MAX_TEXTFORMAT_BUFFERS invocations
    static char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { 0 };
    static int index = 0;

    char *currentBuffer = buffers[index];
    memset(currentBuffer, 0, MAX_TEXT_BUFFER_LENGTH);   // Clear buffer before using

    va_list args;
    va_start(args, text);
    int requiredByteCount = vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, args);
    va_end(args);

    // If requiredByteCount is larger than the MAX_TEXT_BUFFER_LENGTH, then overflow occured
    if (requiredByteCount >= MAX_TEXT_BUFFER_LENGTH)
    {
        // Inserting "..." at the end of the string to mark as truncated
        char *truncBuffer = buffers[index] + MAX_TEXT_BUFFER_LENGTH - 4; // Adding 4 bytes = "...\0"
        sprintf(truncBuffer, "...");
    }

    index += 1;     // Move to next buffer for next function call
    if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;

    return currentBuffer;
}

#endif // !SUPPORT_MODULE_RTEXT
