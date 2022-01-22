#define NOMINMAX
#define UNICODE
#include <assert.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <windows.h>
#include <shobjidl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../libs/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION 
#include "../libs/stb_truetype.h"

#define EASY_STRING_IMPLEMENTATION 1
#include "easy_string_utf8.h"

#include <stdint.h> //for the type uint8_t for our text input buffer
#include <stdio.h>

#define Megabytes(value) value*1000*1000

#define DEFAULT_WINDOW_WIDTH             1280
#define DEFAULT_WINDOW_HEIGHT             720
#define PERMANENT_STORAGE_SIZE  Megabytes(32)
#define SCRATCH_STORAGE_SIZE    Megabytes(32)
#define RENDERER_STORAGE_SIZE    Megabytes(32)

#include "platform.h"


static PlatformLayer global_platform;
static HWND global_wndHandle;
static ID3D11Device1 *global_d3d11Device;

enum PlatformKeyType {
    PLATFORM_KEY_NULL,
    PLATFORM_KEY_UP,
    PLATFORM_KEY_DOWN,
    PLATFORM_KEY_RIGHT,
    PLATFORM_KEY_LEFT,
    PLATFORM_KEY_X,
    PLATFORM_KEY_Z,

    PLATFORM_KEY_LEFT_CTRL,
    PLATFORM_KEY_O,

    PLATFORM_KEY_BACKSPACE,

    PLATFORM_MOUSE_LEFT_BUTTON,
    PLATFORM_MOUSE_RIGHT_BUTTON,
    
    // NOTE: Everything before here
    PLATFORM_KEY_TOTAL_COUNT
};

struct PlatformKeyState {
    bool isDown;
    int pressedCount;
    int releasedCount;
};

#define PLATFORM_MAX_TEXT_BUFFER_SIZE_IN_BYTES 256
#define PLATFORM_MAX_KEY_INPUT_BUFFER 16

struct PlatformInputState {

    PlatformKeyState keyStates[PLATFORM_KEY_TOTAL_COUNT]; 

    //NOTE: Mouse data
    float mouseX;
    float mouseY;
    float mouseScrollX;
    float mouseScrollY;

    //NOTE: Text Input
    uint8_t textInput_utf8[PLATFORM_MAX_TEXT_BUFFER_SIZE_IN_BYTES];
    int textInput_bytesUsed;

    PlatformKeyType keyInputCommandBuffer[PLATFORM_MAX_KEY_INPUT_BUFFER];
    int keyInputCommand_count;

    WCHAR low_surrogate;

    UINT dpi_for_window;
};

static PlatformInputState global_platformInput;



LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    //quit our program
    if(msg == WM_CLOSE || msg == WM_DESTROY) {
        PostQuitMessage(0);
        //NOTE: quit program handled in our loop

    } else if(msg == WM_DPICHANGED) {
        global_platformInput.dpi_for_window = GetDpiForWindow(hwnd);
    } else if(msg == WM_CHAR) {
        
        //NOTE: Dont add backspace to the buffer
        if(wparam != VK_BACK) {

            WCHAR utf16_character = (WCHAR)wparam;

            int characterCount = 0;
            WCHAR characters[2];


            //NOTE: Build the utf-16 string
            if (IS_LOW_SURROGATE(utf16_character))
            {
                if (global_platformInput.low_surrogate != 0)
                {
                    // received two low surrogates in a row, just ignore the first one
                }
                global_platformInput.low_surrogate = utf16_character;
            }
            else if (IS_HIGH_SURROGATE(utf16_character))
            {
                if (global_platformInput.low_surrogate == 0)
                {
                    // received hight surrogate without low one first, just ignore it
                    
                }
                else if (!IS_SURROGATE_PAIR(utf16_character, global_platformInput.low_surrogate))
                {
                    // invalid surrogate pair, ignore the two pairs
                    global_platformInput.low_surrogate = 0;
                } 
                else 
                {
                    //NOTE: We got a surrogate pair. The string we convert to utf8 will be 2 characters long - 32bits not 16bits
                    characterCount = 2;
                    characters[0] = global_platformInput.low_surrogate;
                    characters[1] = utf16_character;

                }
            }
            else
            {
                if (global_platformInput.low_surrogate != 0)
                {
                    // expected high surrogate after low one, but received normal char
                    // accept normal char message (ignore low surrogate)
                }

                //NOTE: always add non-pair characters. The string will be one character long - 16bits
                characterCount = 1;
                characters[0] = utf16_character;

                global_platformInput.low_surrogate = 0;
            }

            if(characterCount > 0) {
            
                //NOTE: Convert the utf16 character to utf8

                //NOTE: Get the size of the utf8 character in bytes
                int bufferSize_inBytes = WideCharToMultiByte(
                  CP_UTF8,
                  0,
                  (LPCWCH)characters,
                  characterCount,
                  (LPSTR)global_platformInput.textInput_utf8, 
                  0,
                  0, 
                  0
                );

                //NOTE: See if we can still fit the character in our buffer. We don't do <= to the max buffer size since we want to keep one character to create a null terminated string.
                if((global_platformInput.textInput_bytesUsed + bufferSize_inBytes) < PLATFORM_MAX_TEXT_BUFFER_SIZE_IN_BYTES) {
                        
                    //NOTE: Add the utf8 value of the character to our buffer
                    int bytesWritten = WideCharToMultiByte(
                      CP_UTF8,
                      0,
                      (LPCWCH)characters,
                      characterCount,
                      (LPSTR)(global_platformInput.textInput_utf8 + global_platformInput.textInput_bytesUsed), 
                      bufferSize_inBytes,
                      0, 
                      0
                    );

                    //NOTE: Increment the buffer size
                    global_platformInput.textInput_bytesUsed += bufferSize_inBytes;

                    //NOTE: Make the string null terminated
                    assert(bufferSize_inBytes < PLATFORM_MAX_TEXT_BUFFER_SIZE_IN_BYTES);
                    global_platformInput.textInput_utf8[global_platformInput.textInput_bytesUsed] = '\0';
                }

                global_platformInput.low_surrogate = 0;
            }
        }

    } else if(msg == WM_LBUTTONDOWN) {
        if(!global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].isDown) {
            global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].pressedCount++;
        }
        
        global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].isDown = true;

    } else if(msg == WM_LBUTTONUP) {
        global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].releasedCount++;
        global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].isDown = false;

    } else if(msg == WM_RBUTTONDOWN) {
        if(!global_platformInput.keyStates[PLATFORM_MOUSE_RIGHT_BUTTON].isDown) {
            global_platformInput.keyStates[PLATFORM_MOUSE_RIGHT_BUTTON].pressedCount++;
        }
        
        global_platformInput.keyStates[PLATFORM_MOUSE_RIGHT_BUTTON].isDown = true;
    } else if(msg == WM_RBUTTONUP) {
        global_platformInput.keyStates[PLATFORM_MOUSE_RIGHT_BUTTON].releasedCount++;
        global_platformInput.keyStates[PLATFORM_MOUSE_RIGHT_BUTTON].isDown = false;

    } else if(msg == WM_MOUSEWHEEL) {
        //NOTE: We use the HIWORD macro defined in windows.h to get the high 16 bits
        short wheel_delta = HIWORD(wparam);
        global_platformInput.mouseScrollY = (float)wheel_delta;

    } else if(msg == WM_MOUSEHWHEEL) {
        //NOTE: We use the HIWORD macro defined in windows.h to get the high 16 bits
        short wheel_delta = HIWORD(wparam);
        global_platformInput.mouseScrollX = (float)wheel_delta;

    } else if(msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {

        bool keyWasDown = ((lparam & (1 << 30)) != 0);

        // char str[256];
        // sprintf(str, "%d\n", (u32)lparam);
        // OutputDebugStringA((char *)str);

        bool keyIsDown = ((lparam & (1 << 31)) == 0);

        WPARAM vk_code = wparam;        

        PlatformKeyType keyType = PLATFORM_KEY_NULL; 

        bool addToCommandBuffer = false;

        //NOTE: match our internal key names to the vk code
        if(vk_code == VK_UP) { 
            keyType = PLATFORM_KEY_UP;
        } else if(vk_code == VK_DOWN) {
            keyType = PLATFORM_KEY_DOWN;
        } else if(vk_code == VK_LEFT) {
            keyType = PLATFORM_KEY_LEFT;

            //NOTE: Also add the message to our command buffer if it was a KEYDOWN message
            addToCommandBuffer = keyIsDown;
        } else if(vk_code == VK_RIGHT) {
            keyType = PLATFORM_KEY_RIGHT;

            //NOTE: Also add the message to our command buffer if it was a KEYDOWN message
            addToCommandBuffer = keyIsDown;
        } else if(vk_code == 'Z') {
            keyType = PLATFORM_KEY_Z;
        } else if(vk_code == 'X') {
            keyType = PLATFORM_KEY_X;
        } else if(vk_code == 'O') {
            keyType = PLATFORM_KEY_O;
        } else if(vk_code == VK_CONTROL) {
            keyType = PLATFORM_KEY_LEFT_CTRL;
        } else if(vk_code == VK_BACK) {
            keyType = PLATFORM_KEY_BACKSPACE;

            //NOTE: Also add the message to our command buffer if it was a KEYDOWN message
            addToCommandBuffer = keyIsDown;
        }

        //NOTE: Add the command message here 
        if(addToCommandBuffer && global_platformInput.keyInputCommand_count < PLATFORM_MAX_KEY_INPUT_BUFFER) {
            global_platformInput.keyInputCommandBuffer[global_platformInput.keyInputCommand_count++] = keyType;
        }


        //NOTE: Key pressed, is down and release events  
        if(keyType != PLATFORM_KEY_NULL) {
            int wasPressed = (keyIsDown && !keyWasDown) ? 1 : 0;
            int wasReleased = (!keyIsDown) ? 1 : 0;

            global_platformInput.keyStates[keyType].pressedCount += wasPressed;
            global_platformInput.keyStates[keyType].releasedCount += wasReleased;

            global_platformInput.keyStates[keyType].isDown = keyIsDown;
        }

    } else {
        result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    return result;
} 

static void *
Win32HeapAlloc(size_t size, bool zeroOut)
{
    void *result = HeapAlloc(GetProcessHeap(), 0, size);

    if(zeroOut) {
        memset(result, 0, size);
    }

    return result;
}

static void
Win32HeapFree(void *data)
{
    HeapFree(GetProcessHeap(), 0, data);
}



static u8 *platform_realloc_memory(void *src, u32 bytesToMove, size_t sizeToAlloc) {
    u8 *result = (u8 *)Win32HeapAlloc(sizeToAlloc, true);

    memmove(result, src, bytesToMove);

    Win32HeapFree(src);

    return result;

}

static void *Platform_OpenFile_withDialog_wideChar_haveToFree() {
    OPENFILENAME config = {};
    config.lStructSize = sizeof(OPENFILENAME);
    config.hwndOwner = global_wndHandle; // Put the owner window handle here.
    config.lpstrFilter = L"\0\0"; // Put the file extension here.

    wchar_t *path = (wchar_t *)Win32HeapAlloc(MAX_PATH*sizeof(wchar_t), true);
    path[0] = 0;
    
    config.lpstrFile = path;
    config.lpstrDefExt = L"cpp" + 1;
    config.nMaxFile = MAX_PATH;
    config.Flags = OFN_FILEMUSTEXIST;
    config.Flags |= OFN_NOCHANGEDIR;//To prevent GetOpenFileName() from changing the working directory

    if (GetOpenFileNameW(&config)) {
        //NOTE: Success
    } 

    return path;
}


// void *result = 0;

// OPENFILENAME config = {};
// config.lStructSize = sizeof(OPENFILENAME);
// config.hwndOwner = global_wndHandle; // Put the owner window handle here.
// config.lpstrFilter = L"\0\0"; // Put the file extension here.

// wchar_t *path = (wchar_t *)Win32HeapAlloc(MAX_PATH*sizeof(wchar_t), true);
// path[0] = 0;

// config.lpstrFile = path;
// config.lpstrDefExt = L"cpp" + 1;
// config.nMaxFile = sizeof(path) / sizeof(path[0]);
// config.Flags = OFN_FILEMUSTEXIST;
// config.Flags |= OFN_NOCHANGEDIR;//To prevent GetOpenFileName() from changing the working directory

// if (GetOpenFileNameW(&config)) {
//     // `path` contains the file
// }
// return path;



static bool Platform_LoadEntireFile_wideChar(void *filename_wideChar_, void **data, size_t *data_size) {

    LPWSTR filename_wideChar = (LPWSTR)filename_wideChar_;

     bool read_successful = 0;
    
    *data = 0;
    *data_size = 0;
    
    HANDLE file = {0};
    
    {
        DWORD desired_access = GENERIC_READ | GENERIC_WRITE;
        DWORD share_mode = 0;
        SECURITY_ATTRIBUTES security_attributes = {
            (DWORD)sizeof(SECURITY_ATTRIBUTES),
            0,
            0,
        };
        DWORD creation_disposition = OPEN_EXISTING;
        DWORD flags_and_attributes = 0;
        HANDLE template_file = 0;
        
        if((file = CreateFileW(filename_wideChar, desired_access, share_mode, &security_attributes, creation_disposition, flags_and_attributes, template_file)) != INVALID_HANDLE_VALUE)
        {
            DWORD read_bytes = GetFileSize(file, 0);
            if(read_bytes)
            {
                void *read_data = Win32HeapAlloc(read_bytes+1, false);
                DWORD bytes_read = 0;
                OVERLAPPED overlapped = {0};
                
                ReadFile(file, read_data, read_bytes, &bytes_read, &overlapped);
                
                ((u8 *)read_data)[read_bytes] = 0;
                
                *data = read_data;
                *data_size = (u64)bytes_read;
                
                read_successful = 1;
            }
            CloseHandle(file);
        }
    }
    
    return read_successful;
}

static void *Platform_loadTextureToGPU(void *data, u32 texWidth, u32 texHeight, u32 bytesPerPixel) {
    // Create Texture
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width              = texWidth;
    textureDesc.Height             = texHeight;
    textureDesc.MipLevels          = 1;
    textureDesc.ArraySize          = 1;
    textureDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    textureDesc.SampleDesc.Count   = 1;
    textureDesc.Usage              = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA textureSubresourceData = {};
    textureSubresourceData.pSysMem = data;
    textureSubresourceData.SysMemPitch = bytesPerPixel*texWidth;

    ID3D11Texture2D* texture;
    global_d3d11Device->CreateTexture2D(&textureDesc, &textureSubresourceData, &texture);

    ID3D11ShaderResourceView* textureView;
    global_d3d11Device->CreateShaderResourceView(texture, nullptr, &textureView);

    return textureView;
}

static bool Platform_LoadEntireFile_utf8(char *filename_utf8, void **data, size_t *data_size) {
    LPWSTR filename_wideChar;
    {
        //NOTE: turning utf8 to windows wide char
        int characterCount = MultiByteToWideChar(CP_UTF8, 0, filename_utf8, -1, 0, 0);

        u32 sizeInBytes = (characterCount + 1)*sizeof(u16); //NOTE: Plus one for null terminator

        filename_wideChar = (LPWSTR)Win32HeapAlloc(sizeInBytes, false);

        int sizeCheck = MultiByteToWideChar(CP_UTF8, 0, filename_utf8, -1, filename_wideChar, characterCount);

        assert(sizeCheck != sizeInBytes);

    }

    bool result = Platform_LoadEntireFile_wideChar(filename_wideChar, data, data_size);

    Win32HeapFree(filename_wideChar);

    return result;

}

static void
Win32FreeFileData(void *data)
{
    Win32HeapFree(data);
}


#include "3DMaths.h"
#include "render.c"


#include "d3d_render.cpp"

#include "main.cpp"


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    // Open a window
    HWND hwnd;
    {	
    	//First register the type of window we are going to create
        WNDCLASSEXW winClass = {};
        winClass.cbSize = sizeof(WNDCLASSEXW);
        winClass.style = CS_HREDRAW | CS_VREDRAW;
        winClass.lpfnWndProc = &WndProc;
        winClass.hInstance = hInstance;
        winClass.hIcon = LoadIconW(0, IDI_APPLICATION);
        winClass.hCursor = LoadCursorW(0, IDC_ARROW);
        winClass.lpszClassName = L"MyWindowClass";
        winClass.hIconSm = LoadIconW(0, IDI_APPLICATION);

        if(!RegisterClassExW(&winClass)) {
            MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }

        //Now create the actual window
        RECT initialRect = { 0, 0, 960, 540 };
        AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
        LONG initialWidth = initialRect.right - initialRect.left;
        LONG initialHeight = initialRect.bottom - initialRect.top;

        hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
                                winClass.lpszClassName,
                                L"Woodland Editor",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                initialWidth, 
                                initialHeight,
                                0, 0, hInstance, 0);

        if(!hwnd) {
            MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }
    }

    global_wndHandle = hwnd;


    //TODO: Change to using memory arena? 
    BackendRenderer *backendRenderer = (BackendRenderer *)Win32HeapAlloc(sizeof(BackendRenderer), true); 
    backendRender_init(backendRenderer, hwnd);

    global_platformInput.dpi_for_window = GetDpiForWindow(hwnd);

    char buffer[256];
    sprintf(buffer, "%d\n", global_platformInput.dpi_for_window);
    OutputDebugStringA(buffer);

    global_d3d11Device = backendRenderer->d3d11Device;

    //NOTE: Create a input buffer to store text input across frames.
    #define MAX_INPUT_BUFFER_SIZE 128
    int textBuffer_count = 0;
    uint8_t textBuffer[MAX_INPUT_BUFFER_SIZE] = {};

    int cursorAt = 0;  //NOTE: Where our cursor position is
    //////////////////////////////////////////////////////////////


    //NOTE: Allocate stuff
    global_platform.permanent_storage_size = PERMANENT_STORAGE_SIZE;
    global_platform.scratch_storage_size = SCRATCH_STORAGE_SIZE;

    global_platform.permanent_storage = VirtualAlloc(0, global_platform.permanent_storage_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    global_platform.scratch_storage = VirtualAlloc(0, global_platform.scratch_storage_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    /////////////////////

    // Timing
    LONGLONG startPerfCount = 0;
    LONGLONG perfCounterFrequency = 0;
    {
        //NOTE: Get the current performance counter at this moment to act as our reference
        LARGE_INTEGER perfCount;
        QueryPerformanceCounter(&perfCount);
        startPerfCount = perfCount.QuadPart;

        //NOTE: Get the Frequency of the performance counter to be able to convert from counts to seconds
        LARGE_INTEGER perfFreq;
        QueryPerformanceFrequency(&perfFreq);
        perfCounterFrequency = perfFreq.QuadPart;

    }

    //NOTE: To store the last time in
    double currentTimeInSeconds = 0.0;
    

    bool running = true;
    while(running) {

        //NOTE: Inside game loop
        float dt;
        {
            double previousTimeInSeconds = currentTimeInSeconds;
            LARGE_INTEGER perfCount;
            QueryPerformanceCounter(&perfCount);

            currentTimeInSeconds = (double)(perfCount.QuadPart - startPerfCount) / (double)perfCounterFrequency;
            dt = (float)(currentTimeInSeconds - previousTimeInSeconds);
        }

        //NOTE: Clear the input text buffer to empty
        global_platformInput.textInput_bytesUsed = 0;
        global_platformInput.textInput_utf8[0] = '\0';

        //NOTE: Clear our input command buffer
        global_platformInput.keyInputCommand_count = 0;

        
        //NOTE: Clear the key pressed and released count before processing our messages
        for(int i = 0; i < PLATFORM_KEY_TOTAL_COUNT; ++i) {
            global_platformInput.keyStates[i].pressedCount = 0;
            global_platformInput.keyStates[i].releasedCount = 0;
        }

    	MSG msg = {};
        while(PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if(msg.message == WM_QUIT) {
                running = false;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }


        //NOTE: Get mouse position
        {  
            POINT mouse;
            GetCursorPos(&mouse);
            ScreenToClient(hwnd, &mouse);
            global_platformInput.mouseX = (float)(mouse.x);
            global_platformInput.mouseY = (float)(mouse.y);
        }

        //NOTE: Find the smallest size we can add to the buffer without overflowing it
        int bytesToMoveAboveCursor = global_platformInput.textInput_bytesUsed;
        int spaceLeftInBuffer = (MAX_INPUT_BUFFER_SIZE - textBuffer_count - 1); //minus one to put a null terminating character in
        if(bytesToMoveAboveCursor > spaceLeftInBuffer) {
            bytesToMoveAboveCursor = spaceLeftInBuffer;
        }

        //NOTE: Get all characters above cursor and save them in a buffer
        char tempBuffer[MAX_INPUT_BUFFER_SIZE] = {};
        int tempBufferCount = 0;
        for(int i = cursorAt; i < textBuffer_count; i++) {
            tempBuffer[tempBufferCount++] = textBuffer[i];
        }

        //NOTE: Copy new string into the buffer
        for(int i = 0; i < bytesToMoveAboveCursor; ++i) {
            textBuffer[cursorAt + i] = global_platformInput.textInput_utf8[i];
        }
        
        //NOTE: Advance the cursor and the buffer count
        textBuffer_count += bytesToMoveAboveCursor;
        cursorAt += bytesToMoveAboveCursor;

        //NOTE: Replace characters above the cursor that we would have written over
        for(int i = 0; i < tempBufferCount; ++i) {
            textBuffer[cursorAt + i] = tempBuffer[i]; 
        }

        //NOTE: Should cache these values 
        RECT winRect;
        GetClientRect(hwnd, &winRect);

        EditorState *editorState = updateEditor(dt, (float)(winRect.right - winRect.left), (float)(winRect.bottom - winRect.top));


        // //NOTE: Process our command buffer
        // for(int i = 0; i < global_platformInput.keyInputCommand_count; ++i) {
        //     PlatformKeyType command = global_platformInput.keyInputCommandBuffer[i];
        //     if(command == PLATFORM_KEY_BACKSPACE) {
                
        //         //NOTE: can't backspace a character if cursor is in front of text
        //         if(cursorAt > 0 && textBuffer_count > 0) {
        //             //NOTE: Move all characters in front of cursor down
        //             int charactersToMoveCount = textBuffer_count - cursorAt;
        //             for(int i = 0; i < charactersToMoveCount; ++i) {
        //                 int indexInFront = cursorAt + i;
        //                 assert(indexInFront < textBuffer_count); //make sure not buffer overflow
        //                 textBuffer[cursorAt + i - 1] = textBuffer[indexInFront]; //get the one in front 
        //             }

        //             cursorAt--;
        //             textBuffer_count--;
        //         }
                
        //     }

        //     if(command == PLATFORM_KEY_LEFT) {
        //         //NOTE: Move cursor left 
        //         if(cursorAt > 0) {
        //             cursorAt--;
        //         }
        //     }

        //     if(command == PLATFORM_KEY_RIGHT) {
        //         //NOTE: Move cursor right 
        //         if(cursorAt < textBuffer_count) {
        //             cursorAt++;
        //         }
        //     }       
        // }  

        // //NOTE: put in a null terminating character at the end
        // assert(textBuffer_count < MAX_INPUT_BUFFER_SIZE);
        // textBuffer[textBuffer_count] = '\0';  



        // FLOAT backgroundColor[4] = { DEFINES_BACKGROUND_COLOR };
        // d3d11DeviceContext->ClearRenderTargetView(default_d3d11FrameBufferView, backgroundColor);


        
        backendRender_processCommandBuffer(&editorState->renderer, backendRenderer);

        backendRender_presentFrame(backendRenderer);
    


    }
    
    return 0;

}