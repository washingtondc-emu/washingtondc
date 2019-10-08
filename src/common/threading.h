/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#ifndef WASHDC_THREADING_H_
#define WASHDC_THREADING_H_

typedef void(*washdc_thread_main)(void*);

#ifdef _WIN32

#ifndef WINVER
#define WINVER 0x0600
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <synchapi.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    void *argp;
    washdc_thread_main entry;
    HANDLE td;
} washdc_thread;
typedef SRWLOCK washdc_mutex;
typedef CONDITION_VARIABLE washdc_cvar;

#define WASHDC_MUTEX_STATIC_INIT SRWLOCK_INIT
#define WASHDC_CVAR_STATIC_INIT CONDITION_VARIABLE_INIT

inline static void washdc_mutex_init(washdc_mutex *mtx) {
    InitializeSRWLock(mtx);
}

inline static void washdc_mutex_cleanup(washdc_mutex *mtx) {
}

inline static void washdc_mutex_lock(washdc_mutex *mtx) {
    AcquireSRWLockExclusive(mtx);
}

inline static void washdc_mutex_unlock(washdc_mutex *mtx) {
    ReleaseSRWLockExclusive(mtx);
}

static void washdc_cvar_init(washdc_cvar *cvar) {
    InitializeConditionVariable(cvar);
}

static void washdc_cvar_cleanup(washdc_cvar *cvar) {
}

static void washdc_cvar_wait(washdc_cvar *cvar, washdc_mutex *mtx) {
    if (!SleepConditionVariableSRW(cvar, mtx, INFINITE, 0)) {
        fprintf(stderr, "Failure to acquire condition variable - %08X!\n",
                (unsigned)GetLastError());
    }
}

static void washdc_cvar_signal(washdc_cvar *cvar) {
    WakeAllConditionVariable(cvar);
}

static DWORD washdc_thread_entry_proxy_win32(_In_ LPVOID lpParameter) {
    washdc_thread *td = (washdc_thread*)lpParameter;
    td->entry(td->argp);
    return 0;
}

static void washdc_thread_create(washdc_thread *td, washdc_thread_main entry, void *argp) {
    td->argp = argp;
    td->entry = entry;
    HANDLE newtd = CreateThread(NULL, 0, washdc_thread_entry_proxy_win32,
                                td, 0, NULL);
    if (!newtd)
        fprintf(stderr, "ERROR: unable to launch thread\n");
    td->td = newtd;
}

static void washdc_thread_join(washdc_thread *td) {
    if (WaitForSingleObject(td->td, INFINITE) == WAIT_FAILED)
        fprintf(stderr, "unable to join thread\n");
}

#else

#include <pthread.h>
#include <stdio.h>

typedef struct {
    void *argp;
    washdc_thread_main entry;
    pthread_t td;
} washdc_thread;
typedef pthread_mutex_t washdc_mutex;
typedef pthread_cond_t washdc_cvar;

#define WASHDC_MUTEX_STATIC_INIT PTHREAD_MUTEX_INITIALIZER
#define WASHDC_CVAR_STATIC_INIT PTHREAD_COND_INITIALIZER

inline static void washdc_mutex_init(washdc_mutex *mtx) {
    pthread_mutex_init(mtx, NULL);
}

inline static void washdc_mutex_cleanup(washdc_mutex *mtx) {
    pthread_mutex_destroy(mtx);
}

inline static void washdc_mutex_lock(washdc_mutex *mtx) {
    pthread_mutex_lock(mtx);
}

inline static void washdc_mutex_unlock(washdc_mutex *mtx) {
    pthread_mutex_unlock(mtx);
}

static void washdc_cvar_init(washdc_cvar *cvar) {
    pthread_cond_init(cvar, NULL);
}

static void washdc_cvar_cleanup(washdc_cvar *cvar) {
    pthread_cond_destroy(cvar);
}

static void washdc_cvar_wait(washdc_cvar *cvar, washdc_mutex *mtx) {
    if (pthread_cond_wait(cvar, mtx) != 0)
        fprintf(stderr, "Failure to acquire condition variable\n");
}

static void washdc_cvar_signal(washdc_cvar *cvar) {
    pthread_cond_signal(cvar);
}

static void *washdc_thread_entry_proxy_unix(void *argp) {
    washdc_thread *td = (washdc_thread*)argp;
    td->entry(td->argp);
    return NULL;
}

static void washdc_thread_create(washdc_thread *td, washdc_thread_main entry, void *argp) {
    td->argp = argp;
    td->entry = entry;
    if (pthread_create(&td->td, NULL, washdc_thread_entry_proxy_unix, td) != 0)
        fprintf(stderr, "ERROR: unable to launch thread\n");
}

static void washdc_thread_join(washdc_thread *td) {
    pthread_join(td->td, NULL);
}

#endif

#endif
