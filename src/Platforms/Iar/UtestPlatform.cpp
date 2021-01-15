/*
 * Copyright (c) 2007, Michael Feathers, James Grenning and Bas Vodde
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE EARLIER MENTIONED AUTHORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "CppUTest/TestHarness.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef strndup
#include "CppUTest/PlatformSpecificFunctions.h"

#define DEMCR_TRCENA    0x01000000
#define DEMCR           (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL        (*(volatile uint32_t *)0xE0001000)
#define CYCCNTENA       (1<<0)
#define DWT_CYCCNT      ((volatile uint32_t *)0xE0001004)

static int initialized = 0;
static uint32_t overflow_cnt = 0;
static uint32_t start;
extern uint32_t SystemCoreClock;

static jmp_buf test_exit_jmp_buf[10];
static int jmp_buf_index = 0;

TestOutput::WorkingEnvironment PlatformSpecificGetWorkingEnvironment()
{
  return TestOutput::eclipse;
}

static void DummyPlatformSpecificRunTestInASeperateProcess(UtestShell* shell, TestPlugin*, TestResult* result)
{
  result->addFailure(TestFailure(shell, "-p doesn't work on this platform, as it is lacking fork.\b"));
}

static int DummyPlatformSpecificFork(void)
{
  return 0;
}

static int DummyPlatformSpecificWaitPid(int, int*, int)
{
  return 0;
}

void (*PlatformSpecificRunTestInASeperateProcess)(UtestShell* shell, TestPlugin* plugin, TestResult* result) =
  DummyPlatformSpecificRunTestInASeperateProcess;
int (*PlatformSpecificFork)(void) = DummyPlatformSpecificFork;
int (*PlatformSpecificWaitPid)(int, int*, int) = DummyPlatformSpecificWaitPid;

extern "C" {

  static int PlatformSpecificSetJmpImplementation(void (*function) (void* data), void* data)
  {
    if (0 == setjmp(test_exit_jmp_buf[jmp_buf_index])) {
      jmp_buf_index++;
      function(data);
      jmp_buf_index--;
      return 1;
    }
    return 0;
  }

  static void PlatformSpecificLongJmpImplementation()
  {
    jmp_buf_index--;
    longjmp(test_exit_jmp_buf[jmp_buf_index], 1);
  }

  static void PlatformSpecificRestoreJumpBufferImplementation()
  {
    jmp_buf_index--;
  }

  void (*PlatformSpecificLongJmp)() = PlatformSpecificLongJmpImplementation;
  int (*PlatformSpecificSetJmp)(void (*)(void*), void*) = PlatformSpecificSetJmpImplementation;
  void (*PlatformSpecificRestoreJumpBuffer)() = PlatformSpecificRestoreJumpBufferImplementation;

  long TimeInMillisImplementation()
  {
	if (initialized == 0) {
      DEMCR |= DEMCR_TRCENA;
      *DWT_CYCCNT = 0;
      DWT_CTRL |= CYCCNTENA;
      start = *DWT_CYCCNT;
      initialized = 1;
	}
    double ticks;
    if (*DWT_CYCCNT < start) {
      // overflow
      overflow_cnt++;
      ticks = UINT32_MAX*overflow_cnt + *DWT_CYCCNT;
    } else {
      ticks = UINT32_MAX*overflow_cnt + *DWT_CYCCNT - start;
    }
	return long(ticks/SystemCoreClock * 1000); // ms
  }

  static const char* TimeStringImplementation()
  {
    time_t tm = time(NULL);
    char* pTimeStr = ctime(&tm);
    char* newlineChar = strchr(pTimeStr, '\n');   // Find the terminating newline character.
    if(newlineChar != NULL) *newlineChar = '\0';   //If newline is found replace it with the string terminator.
    return (pTimeStr);
  }

  long (*GetPlatformSpecificTimeInMillis)() = TimeInMillisImplementation;
  const char* (*GetPlatformSpecificTimeString)() = TimeStringImplementation;

  int (*PlatformSpecificVSNprintf)(char *str, size_t size, const char* format, va_list args) = vsnprintf;

  static PlatformSpecificFile PlatformSpecificFOpenImplementation(const char* filename, const char* flag)
  {
    (void)filename;
    (void)flag;
    return 0;
  }

  static void PlatformSpecificFPutsImplementation(const char* str, PlatformSpecificFile file)
  {
    (void)str;
    (void)file;
  }

  static void PlatformSpecificFCloseImplementation(PlatformSpecificFile file)
  {
    (void)file;
  }

  static void PlatformSpecificFlushImplementation()
  {
  }

  PlatformSpecificFile (*PlatformSpecificFOpen)(const char*, const char*) = PlatformSpecificFOpenImplementation;
  void (*PlatformSpecificFPuts)(const char*, PlatformSpecificFile) = PlatformSpecificFPutsImplementation;
  void (*PlatformSpecificFClose)(PlatformSpecificFile) = PlatformSpecificFCloseImplementation;

  int (*PlatformSpecificPutchar)(int) = putchar;
  void (*PlatformSpecificFlush)() = PlatformSpecificFlushImplementation;

  void* (*PlatformSpecificMalloc)(size_t size) = malloc;
  void* (*PlatformSpecificRealloc)(void*, size_t) = realloc;
  void (*PlatformSpecificFree)(void* memory) = free;
  void* (*PlatformSpecificMemCpy)(void*, const void*, size_t) = memcpy;
  void* (*PlatformSpecificMemset)(void*, int, size_t) = memset;

  static int IsNanImplementation(double d)
  {
    return isnan(d);
  }

  static int IsInfImplementation(double d)
  {
    return isinf(d);
  }

  double (*PlatformSpecificFabs)(double) = fabs;
  int (*PlatformSpecificIsNan)(double) = IsNanImplementation;
  int (*PlatformSpecificIsInf)(double) = IsInfImplementation;
  int (*PlatformSpecificAtExit)(void(*func)(void)) = atexit;  /// this was undefined before

  static PlatformSpecificMutex DummyMutexCreate(void)
  {
    return 0;
  }

  static void DummyMutexLock(PlatformSpecificMutex)
  {
  }

  static void DummyMutexUnlock(PlatformSpecificMutex)
  {
  }

  static void DummyMutexDestroy(PlatformSpecificMutex)
  {
  }

  PlatformSpecificMutex (*PlatformSpecificMutexCreate)(void) = DummyMutexCreate;
  void (*PlatformSpecificMutexLock)(PlatformSpecificMutex) = DummyMutexLock;
  void (*PlatformSpecificMutexUnlock)(PlatformSpecificMutex) = DummyMutexUnlock;
  void (*PlatformSpecificMutexDestroy)(PlatformSpecificMutex) = DummyMutexDestroy;

}
