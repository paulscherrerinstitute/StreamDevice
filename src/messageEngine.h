#ifndef messageEngine_h
#define messageEngine_h

#include <string>
#include <time.h>
#include <pthread.h>

struct errorData_t 
{
    bool waitingTimeout;
    time_t lastPrintTime;
    int numberOfCalls;
    char lastErrorMessage[500];
    pthread_mutex_t mutex;
} ;

// Polling time in seconds
#define ENGINE_POLLING_TIME 1

// Errors are grouped by categories. TOTAL_CATEGORIES does not count CAT_NONE.
#define TOTAL_CATEGORIES 6
enum ErrorCategory
{
    CAT_NONE = -1,
    CAT_ASYN_CONNECTION,
    CAT_ASYN_READ,
    CAT_ASYN_WRITE,
    CAT_READ_TIMEOUT,
    CAT_IO_TIMEOUT,
    CAT_PROTO_FORMAT
};

class StreamErrorEngine 
{
private:
    struct errorData_t categories[TOTAL_CATEGORIES];
    int timeout; // in seconds
    pthread_t thread_id;
    bool stopThread;
public:
    StreamErrorEngine();
    ~StreamErrorEngine();
    void setTimeout(int tmout);
    int getTimeout();
    errorData_t* errorDataFrom(ErrorCategory category); 
    void callError(ErrorCategory category, char* message);
    bool mustStop();
};

// Wrappers for C functions because we don't have C++11
StreamErrorEngine* createEngine(pthread_t thread_id);
void *engineJob(void* engineObj);

#endif
