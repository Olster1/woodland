
#define THREAD_WORK_FUNCTION(name) void name(void *Data)
typedef THREAD_WORK_FUNCTION(thread_work_function);

struct thread_work
{
    thread_work_function *FunctionPtr;
    void *Data;
    b32 Finished;
};

struct thread_info
{
#if SDL_PORT
    SDL_sem *Semaphore;
    SDL_Window *WindowHandle;
    thread_work WorkQueue[256];
    SDL_atomic_t IndexToTakeFrom;
    SDL_atomic_t IndexToAddTo;
    SDL_GLContext ContextForThread;
#else 
    HANDLE Semaphore;
    LPVOID WindowHandle;
    thread_work WorkQueue[256];
    volatile u32 IndexToTakeFrom;
    volatile u32 IndexToAddTo;
    HGLRC ContextForThread;
    HDC WindowDC;
#endif
};

#define PLATFORM_PUSH_WORK_ONTO_QUEUE(name) void name(thread_info *Info, thread_work_function *WorkFunction, void *Data)
typedef PLATFORM_PUSH_WORK_ONTO_QUEUE(platform_push_work_onto_queue);



//TODO(ollie): Make safe for threads other than the main thread to add stuff
PLATFORM_PUSH_WORK_ONTO_QUEUE(Win32PushWorkOntoQueue) {
    for(;;)
    {
        u32 OnePastTheHead = (Info->IndexToAddTo + 1) % ArrayCount(Info->WorkQueue);
        if(Info->WorkQueue[Info->IndexToAddTo].Finished && OnePastTheHead != Info->IndexToTakeFrom)
        {
            thread_work *Work = Info->WorkQueue + Info->IndexToAddTo; 
            Work->FunctionPtr = WorkFunction;
            Work->Data = Data;
            Work->Finished = false;
            
            MemoryBarrier();
            _ReadWriteBarrier();
            
            ++Info->IndexToAddTo %= ArrayCount(Info->WorkQueue);
            
            MemoryBarrier();
            _ReadWriteBarrier();
            
            ReleaseSemaphore(Info->Semaphore, 1, 0);
            
            break;
        }
        else
        {
            //NOTE(ollie): Queue is full
        }
    }
    
}

static thread_work *
GetWorkOffQueue(thread_info *Info, thread_work **WorkRetrieved)
{
    *WorkRetrieved = 0;
    
    u32 OldValue = Info->IndexToTakeFrom;
    if(OldValue != Info->IndexToAddTo)
    {
        u32 NewValue = (OldValue + 1) % ArrayCount(Info->WorkQueue);
        if(InterlockedCompareExchange(&Info->IndexToTakeFrom, NewValue, OldValue) == OldValue)
        {
            *WorkRetrieved = Info->WorkQueue + OldValue;
        }
    }    
    return *WorkRetrieved;
}

static void
Win32DoThreadWork(thread_info *Info)
{
    thread_work *Work;
    while(GetWorkOffQueue(Info, &Work))
    {
        Work->FunctionPtr(Work->Data);
        Assert(!Work->Finished);
        
        MemoryBarrier();
        _ReadWriteBarrier();
        
        Work->Finished = true;
        
    }
}

static DWORD
Win32ThreadEntryPoint(LPVOID Info_)
{
    thread_info *Info = (thread_info *)Info_;
    
    for(;;)
    {
        Win32DoThreadWork(Info);
        
        WaitForSingleObject(Info->Semaphore, INFINITE);
    }
    
}

inline b32
IsWorkFinished(thread_info *Info)
{
    b32 Result = true;
    for(u32 WorkIndex = 0;
        WorkIndex < ArrayCount(Info->WorkQueue);
        ++WorkIndex)
    {
        Result &= Info->WorkQueue[WorkIndex].Finished;
        if(!Result) { break; }
    }
    
    return Result;
}

static void
WaitForWorkToFinish(thread_info *Info)
{
    while(!IsWorkFinished(Info))
    {
        Win32DoThreadWork(Info);        
    }
}

THREAD_WORK_FUNCTION(PrintTest)
{
    char *String = (char *)Data;
    OutputDebugString(String);
}

THREAD_WORK_FUNCTION(MessageLoop)
{
    HWND WindowHandle = (HWND)(Data);
    MSG Message;
    for(;;)
    {
        while(PeekMessage(&Message, WindowHandle, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }
}


static void win32_init_threads() {
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    
    u32 NumberOfProcessors = SystemInfo.dwNumberOfProcessors;
    
    u32 NumberOfUnusedProcessors = (NumberOfProcessors - 1); //NOTE(oliver): minus one to account for the one we are on
    
    thread_info ThreadInfo = {};
    ThreadInfo.Semaphore = CreateSemaphore(0, 0, NumberOfUnusedProcessors, 0);
    ThreadInfo.IndexToTakeFrom = ThreadInfo.IndexToAddTo = 0;
    ThreadInfo.WindowHandle = WindowHandle;
    ThreadInfo.WindowDC = WindowDC;
    
    for(u32 WorkIndex = 0;
        WorkIndex < ArrayCount(ThreadInfo.WorkQueue);
        ++WorkIndex)
    {
        ThreadInfo.WorkQueue[WorkIndex].Finished = true;
    }
    
    HANDLE Threads[12];
    u32 ThreadCount = 0;
    
    s32 CoreCount = Min(NumberOfUnusedProcessors, ArrayCount(Threads));
    
    for(u32 CoreIndex = 0;
        CoreIndex < (u32)CoreCount;
        ++CoreIndex)
    {
        HGLRC ContextForThisThread =  Global_wglCreateContextAttribsARB(WindowDC, MainRenderContext, OpenGLAttribs);
        
        ThreadInfo.ContextForThread = ContextForThisThread;
        Assert(ThreadCount < ArrayCount(Threads));
        Threads[ThreadCount++] = CreateThread(0, 0, Win32ThreadEntryPoint, &ThreadInfo, 0, 0);
    }
    
}