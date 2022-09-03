#define NOMINMAX
#define UNICODE
#include <assert.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <windows.h>
#include <shobjidl.h>
#include <Shlobj.h>
#include <shlwapi.h>
#include <wchar.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../libs/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION 
#include "../../libs/stb_truetype.h"


#include <stdint.h> //for the type uint8_t for our text input buffer
#include <stdio.h>

#define Megabytes(value) value*1000*1000
#define Kilobytes(value) value*1000

#define DEFAULT_WINDOW_WIDTH             1280
#define DEFAULT_WINDOW_HEIGHT             720
#define PERMANENT_STORAGE_SIZE  Megabytes(32)

#include "../platform.h"

#define EASY_STRING_IMPLEMENTATION 1
#include "../easy_string_utf8.h"


#include "../debug_stats.h"

// #include "./win32/win32_threads.cpp"

static DEBUG_stats global_debug_stats = {};


static PlatformLayer global_platform;
static HWND global_wndHandle;
static ID3D11Device1 *global_d3d11Device;
static bool global_windowDidResize = false;

static bool w32_got_system_info = false;
SYSTEM_INFO w32_system_info = {}; 

//TODO:  From docs: Because the system cannot compact a private heap, it can become fragmented.
//TODO:  This means we don't want to use heap alloc, we would rather use a memory arena
static void *
platform_alloc_memory(size_t size, bool zeroOut)
{

    void *result = HeapAlloc(GetProcessHeap(), 0, size);

    if(zeroOut) {
        memset(result, 0, size);
    }

    #if DEBUG_BUILD
        DEBUG_add_memory_block_size(&global_debug_stats, result, size);
    #endif

    return result;
}

//NOTE: Used by the game layer
static void platform_free_memory(void *data)
{
#if DEBUG_BUILD
    DEUBG_remove_memory_block_size(&global_debug_stats, data);
#endif

    HeapFree(GetProcessHeap(), 0, data);

}


static u64 platform_get_memory_page_size() {
    if(w32_got_system_info == false)
    {
        w32_got_system_info = true;
        GetSystemInfo(&w32_system_info);
    }
    return w32_system_info.dwPageSize;
}

static bool win32_isValidText(u16 wparam) {
    bool result = true;
    if(wparam < 9 || (14 <= wparam && wparam <= 27)) {
        
        result = false;
    }

     return result;
}


//NOTE: Used by the game layer
static void *platform_alloc_memory_pages(size_t size) {


    u64 page_size = platform_get_memory_page_size();

    size_t size_to_alloc = size + (page_size - 1);

    size_to_alloc -= size_to_alloc % page_size; 

#if DEBUG_BUILD
    global_debug_stats.total_virtual_alloc += size_to_alloc;
#endif


    //NOTE: According to the docs this just gets zeroed out
    return VirtualAlloc(0, size_to_alloc, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); 

}

static u8 *platform_realloc_memory(void *src, u32 bytesToMove, size_t sizeToAlloc) {
    u8 *result = (u8 *)platform_alloc_memory(sizeToAlloc, true);

    memmove(result, src, bytesToMove);

    platform_free_memory(src);

    return result;

}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    //quit our program
    if(msg == WM_CLOSE || msg == WM_DESTROY) {
        PostQuitMessage(0);
        //NOTE: quit program handled in our loop

    } else if(msg == WM_SIZE) {
        global_windowDidResize = true;
    } else if(msg == WM_DPICHANGED) {
        global_platformInput.dpi_for_window = GetDpiForWindow(hwnd);
    } else if(msg == WM_SETFOCUS) {
        //NOTE: Clear out vunerable keys since we only track the down and we can miss the up message if we open a dialog window or player changes screen while holding the key then changes the screen
        
        {
            // Docs: if the high-order bit is 1, the key is down; otherwise, it is up.

            SHORT ctrl_state = GetKeyState(VK_CONTROL);
            SHORT shift_state = GetKeyState(VK_SHIFT);

            global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown = ctrl_state >> 15;
            global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown = shift_state >> 15;

        }

    } else if(msg == WM_DROPFILES) {
        //NOTE: Drop files 
        HDROP drop_data = (HDROP)wparam;

        UINT buffer_size = DragQueryFileW(drop_data, 0, 0, 0);

        char *buffer_memory = (char *)platform_alloc_memory((buffer_size + 1)*sizeof(WCHAR), false);

        DragQueryFileW(drop_data, 0, (LPWSTR)buffer_memory, buffer_size + 1);

        if(global_platformInput.drop_file_name_wide_char_need_to_free) {
            platform_free_memory(global_platformInput.drop_file_name_wide_char_need_to_free);
        }

        global_platformInput.drop_file_name_wide_char_need_to_free = buffer_memory;

        DragFinish(drop_data);

    } else if(msg == WM_CHAR) {
        
        //NOTE: Dont add backspace to the buffer
        //TODO
        if(win32_isValidText((u16)wparam)) {

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
            addToCommandBuffer = keyIsDown;
        } else if(vk_code == VK_DOWN) {
            keyType = PLATFORM_KEY_DOWN;
            addToCommandBuffer = keyIsDown;
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
        } else if(vk_code == VK_OEM_2) {
            keyType = PLATFORM_KEY_FULL_FORWARD_SLASH;
        } else if(vk_code == VK_OEM_PERIOD) {
            keyType = PLATFORM_KEY_FULL_STOP;
        } else if(vk_code == VK_OEM_COMMA) {
            keyType = PLATFORM_KEY_COMMA;
        } else if(vk_code == VK_RETURN) {
            keyType = PLATFORM_KEY_ENTER;
         } else if(vk_code == VK_ESCAPE) {
            keyType = PLATFORM_KEY_ESCAPE;
        } else if(vk_code == VK_END) {
            keyType = PLATFORM_KEY_END;
            //NOTE: Also add the message to our command buffer if it was a KEYDOWN message
            addToCommandBuffer = keyIsDown;
        } else if(vk_code == VK_HOME) {
            keyType = PLATFORM_KEY_HOME;
            //NOTE: Also add the message to our command buffer if it was a KEYDOWN message
            addToCommandBuffer = keyIsDown;
        } else if(vk_code == 'C') {
            keyType = PLATFORM_KEY_C;
        } else if(vk_code == 'S') {
            keyType = PLATFORM_KEY_S;
        } else if(vk_code == 'Y') {
            keyType = PLATFORM_KEY_Y;
        } else if(vk_code == 'B') {
            keyType = PLATFORM_KEY_B;
        } else if(vk_code == 'F') {
            keyType = PLATFORM_KEY_F;
        } else if(vk_code == VK_SHIFT) {
            keyType = PLATFORM_KEY_SHIFT;
        } else if(vk_code == VK_F5) {
            keyType = PLATFORM_KEY_F5;
        } else if(vk_code == 'X') {
            keyType = PLATFORM_KEY_X;
        } else if(vk_code == 'P') {
            keyType = PLATFORM_KEY_P;
        } else if(vk_code == 'O') {
            keyType = PLATFORM_KEY_O;
        } else if(vk_code == 'V') {
            keyType = PLATFORM_KEY_V;
        } else if(vk_code == VK_OEM_MINUS) {
            keyType = PLATFORM_KEY_MINUS;
        } else if(vk_code == VK_OEM_PLUS) {
            keyType = PLATFORM_KEY_PLUS;
        } else if(vk_code == VK_CONTROL) {
            keyType = PLATFORM_KEY_CTRL;
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

#include "../memory_arena.cpp"

//TODO: I don't know if this is meant to be WCHAR or can do straight utf8

static void platform_copy_text_utf8_to_clipboard(char *text, size_t str_size_in_bytes) {
    if(str_size_in_bytes == 0) { return; }
    if (!OpenClipboard(global_wndHandle))  {
        //NOTE: Error
        MessageBoxA(0, "Couldn't open clipboard", "Clip Board Error", MB_OK);
        return;
    } else {

        //NOTE: this frees any memory that has been previously allocated
        EmptyClipboard(); 
         
        assert(sizeof(WCHAR) == 2);

        // size_t str_size_in_bytes = easyString_getSizeInBytes_utf8(text);

        // Allocate a global memory object for the text. 
        HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (str_size_in_bytes + 1)*sizeof(TCHAR));
             
        if (hglbCopy == NULL) { 
            MessageBoxA(0, "Couldn't allocate memory", "Clip Board Error", MB_OK);
        } else { 
         
            // Lock the handle and copy the text to the buffer. 

            LPTSTR  lptstrCopy = (LPTSTR)GlobalLock(hglbCopy); 

            if(lptstrCopy) {
                memcpy(lptstrCopy, text, str_size_in_bytes); 
                u8 *temp = (u8 *)lptstrCopy;

                temp[str_size_in_bytes] = '\0';    // null character 

                GlobalUnlock(hglbCopy); 

                // Place the handle on the clipboard. 
                SetClipboardData(CF_TEXT, hglbCopy); 
            } else {
                MessageBoxA(0, "Couldn't lock memory", "Clip Board Error", MB_OK);
            }
        }

        CloseClipboard();
    }
     
}


static char *platform_get_text_utf8_from_clipboard(Memory_Arena *arena) {
    // Try opening the clipboard
     if (! OpenClipboard(nullptr)) {
        //NOTE: Error
        MessageBoxA(0, "Couldn't open clipboard", "Clip Board Error", MB_OK);
        return 0;
     }

    HANDLE hData = GetClipboardData(CF_TEXT); //CF_TEXT for ansi text //CF_UNICODETEXT for unicode text
    if (hData == nullptr) {
      MessageBoxA(0, "Couldn't get clipboard data", "Clip Board Error", MB_OK);
      return 0;
    } 

    // Lock the handle to get the actual text pointer
    char * text_from_clipboard = (char *)GlobalLock(hData);
    if (text_from_clipboard == nullptr) {
      MessageBoxA(0, "No data in clipboard", "Clip Board Error", MB_OK);
      return 0;
    }

    char *result = 0;
    //NOTE: Put in an arena if the user passes us one, otherwise allocate on the heap
    if(arena) {
        result = nullTerminateArena(text_from_clipboard, easyString_getSizeInBytes_utf8(text_from_clipboard), arena);
    } else {
        result = nullTerminate(text_from_clipboard, easyString_getSizeInBytes_utf8(text_from_clipboard));    
    }
    

    // Release the lock
    GlobalUnlock(hData);

    // Release the clipboard
    CloseClipboard();
        
       
     return result;
}

static void *Platform_OpenFile_withDialog_wideChar(Memory_Arena *arena) {
    OPENFILENAME config = {};
    config.lStructSize = sizeof(OPENFILENAME);
    config.hwndOwner = global_wndHandle; // Put the owner window handle here.
    config.lpstrFilter = L"\0\0"; // Put the file extension here.

    wchar_t *path = (wchar_t *)pushSize(arena, MAX_PATH*sizeof(wchar_t));
    path[0] = 0;
    
    config.lpstrFile = path;
    // config.lpstrDefExt = L"";
    config.nMaxFile = MAX_PATH;
    config.Flags = OFN_FILEMUSTEXIST;
    config.Flags |= OFN_NOCHANGEDIR;//To prevent GetOpenFileName() from changing the working directory

    if (GetOpenFileNameW(&config)) {
        //NOTE: Success
    } 

    return path;
}

static void *Platform_SaveFile_withDialog_wideChar(Memory_Arena *arena) {

    OPENFILENAME config = {};
    config.lStructSize = sizeof(OPENFILENAME);
    config.hwndOwner = global_wndHandle; // Put the owner window handle here.
    config.lpstrFilter = L"\0\0"; // Put the file extension here.

    wchar_t *path = (wchar_t *)pushSize(arena, MAX_PATH*sizeof(wchar_t));
    path[0] = 0;
    
    config.lpstrFile = path;
    // config.lpstrDefExt = L"cpp";
    config.nMaxFile = MAX_PATH;
    config.Flags = OFN_OVERWRITEPROMPT;
    config.Flags |= OFN_NOCHANGEDIR;//To prevent GetOpenFileName() from changing the working directory

    if (GetSaveFileNameW(&config)) {
        //NOTE: Success
    }  else {
        //NOTE: Null the path
        path = NULL;
    }
    return path;
}

static void *platform_wide_char_to_utf8_allocates_on_heap(void *win32_wideString_utf16) {

    u8 *result = 0;

    int bufferSize_inBytes = WideCharToMultiByte(
      CP_UTF8,
      0,
      (LPCWCH)win32_wideString_utf16,
      -1,
      (LPSTR)result, 
      0,
      0, 
      0
    );


    result = (u8 *)platform_alloc_memory(bufferSize_inBytes, false);

    u32 bytesWritten = WideCharToMultiByte(
      CP_UTF8,
      0,
      (LPCWCH)win32_wideString_utf16,
      -1,
      (LPSTR)result, 
      bufferSize_inBytes,
      0, 
      0
    );


    return result;
}

static u16 *platform_utf8_to_wide_char(char *string_utf8, Memory_Arena *arena) {

    WCHAR *result = 0;

    int characterCount = MultiByteToWideChar(CP_UTF8, 0, string_utf8, -1, 0, 0);

    size_t bufferSize_inBytes = (characterCount + 1)*sizeof(u16);

    result = (WCHAR *)pushSize(&globalPerFrameArena, bufferSize_inBytes);

    size_t bytesWritten = MultiByteToWideChar(CP_UTF8, 0, string_utf8, -1, (LPWSTR)result, characterCount);

    return (u16 *)result;
}



static Platform_File_Handle platform_begin_file_write_utf8_file_path (char *path_utf8) {

    Platform_File_Handle Result = {};

    DWORD desired_access = GENERIC_READ | GENERIC_WRITE;
    DWORD share_mode = 0;
    SECURITY_ATTRIBUTES security_attributes = { (DWORD)sizeof(SECURITY_ATTRIBUTES) };
    DWORD creation_disposition = CREATE_ALWAYS;
    DWORD flags_and_attributes = 0;
    HANDLE template_file = 0;
    
    WCHAR *path16 = (WCHAR *)platform_utf8_to_wide_char(path_utf8, &globalPerFrameArena);

    HANDLE FileHandle = CreateFileW((WCHAR*)path16,
                           desired_access,
                           share_mode,
                           &security_attributes,
                           creation_disposition,
                           flags_and_attributes,
                           template_file);
    
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        Result.data = FileHandle;
    }
    else
    {
        DWORD Error = GetLastError();
        Result.has_errors = true;
    }
    
    return Result;
}

static void platform_close_file(Platform_File_Handle handle)
{
    HANDLE FileHandle = (HANDLE)handle.data;
    if(FileHandle)
    {
        CloseHandle(FileHandle);
    }
}

static void platform_write_file_data(Platform_File_Handle handle, void *memory, size_t size_to_write, size_t offset)
{
    if(!handle.has_errors) {
        HANDLE FileHandle = (HANDLE)handle.data;
        if(FileHandle)
        {
            if(SetFilePointer(FileHandle, offset, 0, FILE_BEGIN) !=  INVALID_SET_FILE_POINTER)
            {
                DWORD BytesWritten;
                if(WriteFile(FileHandle, memory, (DWORD)size_to_write, &BytesWritten, 0))
                {
                    if(BytesWritten == size_to_write) {
                    } else {
                        assert(!"soemthing went wrong");
                    } 
                }
                else
                {
                    assert(!"Read file did not succeed");
                }
            }
        }
    } else {
        assert(!"File handle not correct");
    }
}

static char *platform_get_save_file_location_utf8(Memory_Arena *arena) {
    char *result = 0;

    PWSTR  win32_wideString_utf16 = 0; 

    //NOTE(ollie): Get the folder name
    if(SHGetKnownFolderPath(
      FOLDERID_LocalAppData,
      KF_FLAG_CREATE,
      0,
      (PWSTR *)&win32_wideString_utf16
    ) != S_OK) {
        assert(false);
        return result;
    }

    WCHAR *append_str = L"\\Woodland_Editor\\";

    assert(easyString_getSizeInBytes_utf16((u16 *)win32_wideString_utf16) == wcslen(win32_wideString_utf16)*2);

    size_t str_size_in_bytes = easyString_getSizeInBytes_utf16((u16 *)win32_wideString_utf16) + easyString_getSizeInBytes_utf16((u16 *)append_str);

    //TODO: Remove max path - get actual string length and put it in a tmep arena
    WCHAR *buffer = (WCHAR *)pushSize(&globalPerFrameArena, str_size_in_bytes + sizeof(u16));

    memcpy(buffer, win32_wideString_utf16, easyString_getSizeInBytes_utf16((u16 *)win32_wideString_utf16)); 


    PathAppendW(buffer, append_str);

    int bufferSize_inBytes = WideCharToMultiByte(
      CP_UTF8,
      0,
      (LPCWCH)buffer,
      -1,
      (LPSTR)result, 
      0,
      0, 
      0
    );

    result = (char *)pushSize(arena, max(bufferSize_inBytes, MAX_PATH*sizeof(u8)));

    u32 bytesWritten = WideCharToMultiByte(
      CP_UTF8,
      0,
      (LPCWCH)buffer,
      -1,
      (LPSTR)result, 
      bufferSize_inBytes,
      0, 
      0
    );


    

    if(CreateDirectoryW((WCHAR *)buffer, 0))
    {

    }

    //NOTE(ollie): Free the string
    CoTaskMemFree(win32_wideString_utf16);

    return result;
}

static bool platform_does_file_exist(u16 *wide_file_name) {
    return PathFileExistsW((LPCWSTR)wide_file_name);
}

// function void
// OS_DeleteFile(String8 path)
// {
//     M_Temp scratch = GetScratch(0, 0);
//     String16 path16 = Str16From8(scratch.arena, path);
//     DeleteFileW((WCHAR*)path16.str);
//     ReleaseScratch(scratch);
// }

// function B32
// OS_MakeDirectory(String8 path)
// {
//     M_Temp scratch = GetScratch(0, 0);
//     String16 path16 = Str16From8(scratch.arena, path);
//     B32 result = 1;
//     if(!CreateDirectoryW((WCHAR *)path16.str, 0))
//     {
//         result = 0;
//     }
//     ReleaseScratch(scratch);
//     return result;
// }



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
                void *read_data = platform_alloc_memory(read_bytes+1, false);
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

        filename_wideChar = (LPWSTR)platform_alloc_memory(sizeInBytes, false);

        int sizeCheck = MultiByteToWideChar(CP_UTF8, 0, filename_utf8, -1, filename_wideChar, characterCount);

        assert(sizeCheck != sizeInBytes);

    }

    bool result = Platform_LoadEntireFile_wideChar(filename_wideChar, data, data_size);

    platform_free_memory(filename_wideChar);

    return result;

}

#include "../3DMaths.h"

static float2 platform_get_window_xy_pos() {
    float2 result = make_float2(0, 0);
    RECT dim = {};
    if(GetWindowRect(global_wndHandle, &dim)) {
        result.x = dim.left;
        result.y = dim.top;
    }
    return result;
}


#include "../render.c"


#include "../render_backend/d3d_render.cpp"

#include "../main.cpp"


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    Settings_To_Save settings_to_save = {};
    char *save_file_location_utf8 = 0;


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

        LONG window_width = 960;
        LONG window_height = 540;

        //NOTE: Allocate stuff
        global_platform.permanent_storage_size = PERMANENT_STORAGE_SIZE;
        global_platform.permanent_storage = platform_alloc_memory_pages(global_platform.permanent_storage_size);
        

        global_long_term_arena = initMemoryArena_withMemory(((u8 *)global_platform.permanent_storage) + sizeof(EditorState), global_platform.permanent_storage_size - sizeof(EditorState));

        globalPerFrameArena = initMemoryArena(Kilobytes(100));
        global_perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);

        int window_xAt = CW_USEDEFAULT;
        int window_yAt = CW_USEDEFAULT;

        //NOTE: Get the settings file we need
        {
            save_file_location_utf8 = platform_get_save_file_location_utf8(&global_long_term_arena);

            char *settings_file_path = concatInArena(save_file_location_utf8, "user.settings", &globalPerFrameArena);
            Settings_To_Save settings_to_save = load_settings(settings_file_path);

            if(settings_to_save.is_valid) {
                window_width = (LONG)settings_to_save.window_width;
                window_height = (LONG)settings_to_save.window_height;

                window_xAt = (LONG)settings_to_save.window_xAt;
                window_yAt = (LONG)settings_to_save.window_yAt;
            }
        }


        //Now create the actual window
        RECT initialRect = { 0, 0, window_width, window_height };
        AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
        LONG initialWidth = initialRect.right - initialRect.left;
        LONG initialHeight = initialRect.bottom - initialRect.top;

        hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
                                winClass.lpszClassName,
                                L"Woodland Editor",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                window_xAt, window_yAt,
                                initialWidth, 
                                initialHeight,
                                0, 0, hInstance, 0);

        if(!hwnd) {
            MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }
    }

    global_wndHandle = hwnd;

    //NOTE: We want our app to be able to accept dragging files on to. This allows the WM_DROPFILES message to be sent to us
    DragAcceptFiles(hwnd, true);


    //TODO: Change to using memory arena? 
    BackendRenderer *backendRenderer = (BackendRenderer *)platform_alloc_memory(sizeof(BackendRenderer), true); 
    backendRender_init(backendRenderer, hwnd);

    global_platformInput.dpi_for_window = GetDpiForWindow(hwnd);

    global_d3d11Device = backendRenderer->d3d11Device;

    bool first_frame = true;

  
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

        global_platformInput.mouseScrollX = 0;
        global_platformInput.mouseScrollY = 0;
        //NOTE: Clear the key pressed and released count before processing our messages
        for(int i = 0; i < PLATFORM_KEY_TOTAL_COUNT; ++i) {
            global_platformInput.keyStates[i].pressedCount = 0;
            global_platformInput.keyStates[i].releasedCount = 0;
        }

        {
            //Docs: if the high-order bit is 1, the key is down; otherwise, it is up.

            SHORT ctrl_state = GetKeyState(VK_CONTROL);
            SHORT shift_state = GetKeyState(VK_SHIFT);

            global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown = ctrl_state >> 15;
            global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown = shift_state >> 15;

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
        // int bytesToMoveAboveCursor = global_platformInput.textInput_bytesUsed;
        // int spaceLeftInBuffer = (MAX_INPUT_BUFFER_SIZE - textBuffer_count - 1); //minus one to put a null terminating character in
        // if(bytesToMoveAboveCursor > spaceLeftInBuffer) {
        //     bytesToMoveAboveCursor = spaceLeftInBuffer;
        // }

        // //NOTE: Get all characters above cursor and save them in a buffer
        // char tempBuffer[MAX_INPUT_BUFFER_SIZE] = {};
        // int tempBufferCount = 0;
        // for(int i = cursorAt; i < textBuffer_count; i++) {
        //     tempBuffer[tempBufferCount++] = textBuffer[i];
        // }

        // //NOTE: Copy new string into the buffer
        // for(int i = 0; i < bytesToMoveAboveCursor; ++i) {
        //     textBuffer[cursorAt + i] = global_platformInput.textInput_utf8[i];
        // }
        
        // //NOTE: Advance the cursor and the buffer count
        // textBuffer_count += bytesToMoveAboveCursor;
        // cursorAt += bytesToMoveAboveCursor;

        // //NOTE: Replace characters above the cursor that we would have written over
        // for(int i = 0; i < tempBufferCount; ++i) {
        //     textBuffer[cursorAt + i] = tempBuffer[i]; 
        // }

        bool resized_window = false;
        if(global_windowDidResize)
        {
            d3d_release_and_resize_default_frame_buffer(backendRenderer);

            //NOTE: Make new ortho matrix here

            resized_window = true;

            global_windowDidResize = false;
        }

        //NOTE: Should cache these values 
        RECT winRect;
        GetClientRect(hwnd, &winRect);

        EditorState *editorState = updateEditor(dt, (float)(winRect.right - winRect.left), (float)(winRect.bottom - winRect.top), resized_window && !first_frame, save_file_location_utf8, settings_to_save);


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
        
        first_frame = false;


    }
    
    return 0;

}