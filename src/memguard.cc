/***************************************************************
* memguard                                                     *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* Include memguard.h and link memguard.o to get some memory    *
* guardiance. Allocated memory (using new) is tracked with     *
* source file and line of the new operator. Deleted memory is  *
* checked for duplicate deletion.                              *
* Call memguardReport() to see all allocated memory.           *
* Calling memguardReport() shows currently allocated memory.   *
*                                                              *
* DISCLAIMER: If this software breaks something or harms       *
* someone, it's your problem.                                  *
*                                                              *
***************************************************************/

#undef new
#undef delete

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct memheader
{
    static memheader* first;
    memheader* next;
    size_t size;
    long line;
    const char* file;
    long fill1;
    long fill2;
    unsigned long magic;
};

extern "C" void memguardReport();

class memguard
{
public:
    memguard() {atexit(memguardReport);}
    void report();
};

memheader* memheader::first = NULL;

static memguard guard;

extern "C" void memguardReport()
{
    guard.report();
}

void* operator new(size_t size, const char* file, long line) throw (std::bad_alloc)
{
    memheader* header;
    memheader** prev;

    header = (memheader*) malloc(size + sizeof(memheader));
    if (!header)
    {
        fprintf (stderr, "out of memory\n");
        if (file) fprintf (stderr, "in %s line %ld\n", file, line);
        guard.report();
        abort();
    }
    header->magic = 0xA110caed;
    for (prev = &memheader::first; *prev; prev = &(*prev)->next);
    *prev = header;
    header->next = NULL;
    header->size = size;
    header->line = line;
    header->file = file;
//     fprintf(stderr, "allocating %p: %d bytes for %s line %ld\n",
//         header+1, size, file, line);
    return header+1;
}


void* operator new[](size_t size, const char* file, long line) throw (std::bad_alloc)
{
    return operator new(size, file, line);
}

void* operator new(size_t size) throw (std::bad_alloc)
{
    return operator new(size, NULL, 0);
}

void* operator new[](size_t size) throw (std::bad_alloc)
{
    return operator new(size, NULL, 0);
}

static const char* deleteFile = NULL;
static long deleteLine = 0;

int memguardLocation(const char* file, long line)
{
    deleteFile = file;
    deleteLine = line;
    return 1;
}

void operator delete (void* memory) throw ()
{
    memheader* header;
    memheader** prev;

    if (!memory)
    {
        deleteFile = NULL;
        deleteLine = 0;
        return;
    }
    header = ((memheader*)memory)-1;
    if (header->magic == 0xDeadBeef)
    {
        fprintf (stderr, "memory at %p deleted twice\n", memory);
        if (header->file)
            fprintf (stderr, "allocated by %s line %ld\n",
            header->file, header->line);
        guard.report();
        abort();
    }
    for (prev = &memheader::first; *prev; prev = &(*prev)->next)
    {
        if (*prev == header)
        {
            if (header->magic != 0xA110caed)
            {
                fprintf (stderr, "deleted memory at %p corrupt\n", memory);
                if (header->file)
                    fprintf (stderr, "allocated by %s line %ld\n",
                    header->file, header->line);
                guard.report();
                abort();
            }
            *prev = header->next;
            header->magic = 0xDeadBeef;
//            fprintf(stderr, "deleting %p ", memory);
//            if (header->file) fprintf(stderr, "allocated in %s line %ld\n",
//                header->file, header->line);
//            if (deleteFile) fprintf(stderr, "probably deleted in %s line %ld\n",
//                deleteFile, deleteLine);
            deleteFile = NULL;
            deleteLine = 0;
            free(header);
//            fprintf(stderr, "done\n");
            return;
        }
    }
    if (deleteFile)
    {
        fprintf (stderr, "deleted memory at %p was never allocated\n", memory);
        fprintf (stderr, "delete was probably called in %s line %ld\n",
            deleteFile, deleteLine);
        abort();
    }
}

void operator delete[](void* memory) throw ()
{
    operator delete(memory);
}

void memguard::report()
{
    memheader* header;
    unsigned long sum = 0;
    unsigned long chunks = 0;

    if (memheader::first)
    {
        fprintf(stderr, "allocated memory:\n");
        for (header = memheader::first; header; header = header->next)
        {
            chunks++;
            sum += header->size;
            if (header->file)
            {
                fprintf (stderr, "%p: %d bytes by %s line %ld\n",
                    header+1, header->size, header->file, header->line);
            }
            else
            {
                fprintf (stderr, "%p: %d bytes by unknown\n",
                    header+1, header->size);
            }
            if (header->magic != 0xA110caed)
            {
                fprintf (stderr, "memory list is corrupt\n");
                abort();
            }
        }
        fprintf (stderr, "%lu bytes in %lu chunks\n", sum, chunks);
    }
    else
    {
        fprintf(stderr, "no memory allocated\n");
    }
}

