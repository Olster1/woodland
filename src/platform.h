#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

float max(float a, float b) {
    float result = (a < b) ? b : a;
    return result;
}

float min(float a, float b) {
    float result = (a > b) ? b : a;
    return result;
}

#include <stdint.h>

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef intptr_t intprt;

#if !defined arrayCount
#define arrayCount(array1) (sizeof(array1) / sizeof(array1[0]))
#endif 

#define ENUM(value) value,
#define STRING(value) #value,

typedef struct {
	size_t permanent_storage_size;
	size_t scratch_storage_size;

	void *permanent_storage;
	void *scratch_storage;
} PlatformLayer; 

struct Platform_File_Handle {
    void *data;
    bool has_errors;
};

enum PlatformKeyType {
    PLATFORM_KEY_NULL,
    PLATFORM_KEY_UP,
    PLATFORM_KEY_DOWN,
    PLATFORM_KEY_RIGHT,
    PLATFORM_KEY_LEFT,
    PLATFORM_KEY_X,
    PLATFORM_KEY_Z,
    PLATFORM_KEY_Y,
    PLATFORM_KEY_P,
    PLATFORM_KEY_V,
    PLATFORM_KEY_C,
    PLATFORM_KEY_S,
    PLATFORM_KEY_B,
    PLATFORM_KEY_F5,

    PLATFORM_KEY_ENTER,
    PLATFORM_KEY_ESCAPE,

    PLATFORM_KEY_HOME,
    PLATFORM_KEY_END,

    PLATFORM_KEY_CTRL,
    PLATFORM_KEY_COMMA,
    PLATFORM_KEY_FULL_STOP,
    PLATFORM_KEY_FULL_FORWARD_SLASH,

    PLATFORM_KEY_SHIFT,

    PLATFORM_KEY_O,

    PLATFORM_KEY_MINUS,
    PLATFORM_KEY_PLUS,

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

    char *drop_file_name_wide_char_need_to_free; //NOTE: Not null if there is a file that got dropped
};

static PlatformInputState global_platformInput;