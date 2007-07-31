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

#ifdef __cplusplus
#ifndef memguard_h
#define memguard_h
#include <new.h>
#define MEMGUARD
void* operator new(size_t, const char*, long) throw (std::bad_alloc);
void* operator new[](size_t, const char*, long) throw (std::bad_alloc);
#define new new(__FILE__,__LINE__)
int memguardLocation(const char*, long);
#define delete if (memguardLocation(__FILE__,__LINE__)) delete
extern "C" void memguardReport();
#endif
#endif
