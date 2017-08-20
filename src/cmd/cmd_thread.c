/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#include <err.h>
#include <stdlib.h>
#include <pthread.h>

#include "dreamcast.h"
#include "cmd_tcp_link.h"

#include "cmd_thread.h"

static char const *banner =
    "WashingtonDC Copyright (C) 2016, 2017 snickerbockers\n"
    "This program comes with ABSOLUTELY NO WARRANTY;\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under the terms of the GNU GPL version 3;\n";

static pthread_mutex_t cmd_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cmd_thread_create_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cmd_thread_cond = PTHREAD_COND_INITIALIZER;

static pthread_t cmd_thread;

static void cmd_thread_lock(void);
static void cmd_thread_unlock(void);
static void cmd_thread_wait(void);
static void cmd_thread_signal(void);

static void *cmd_thread_main(void *arg);

void cmd_thread_launch(void) {
    int err_code;

    cmd_thread_lock();

    if ((err_code = pthread_create(&cmd_thread, NULL, cmd_thread_main, NULL)) != 0)
        err(1, "unable to launch CMD thread");

    if (pthread_cond_wait(&cmd_thread_create_cond, &cmd_thread_mutex) != 0)
        abort(); // TODO: error handling

    cmd_thread_unlock();
}

void cmd_thread_join(void) {
    pthread_join(cmd_thread, NULL);
}

static void *cmd_thread_main(void *arg) {
    cmd_thread_lock();

    if (pthread_cond_signal(&cmd_thread_create_cond) < 0)
        abort(); // TODO: error handling

    cmd_thread_print(banner);
    while (dc_is_running()) {
        cmd_tcp_link_run_once();
        cmd_thread_wait();
    }

    cmd_thread_unlock();

    pthread_exit(NULL);
    return NULL; // this line never executes
}

void cmd_thread_kick(void) {
    cmd_thread_lock();
    cmd_thread_signal();
    cmd_thread_unlock();
}

void cmd_thread_put_char(char c) {
    // TODO: buffer the incoming text and parse out commands
    if (c)
        printf("CMD_THREAD: \"%02x\" received\n", (unsigned)c);
}

void cmd_thread_print(char const *txt) {
    cmd_tcp_link_put_text(txt);
}

static void cmd_thread_lock(void) {
    if (pthread_mutex_lock(&cmd_thread_mutex) < 0)
        abort(); // TODO: error handling
}

static void cmd_thread_unlock(void) {
    if (pthread_mutex_unlock(&cmd_thread_mutex) < 0)
        abort(); // TODO: error handling
}

static void cmd_thread_wait(void) {
    if (pthread_cond_wait(&cmd_thread_cond, &cmd_thread_mutex) < 0)
        abort(); // TODO: error handling
}

static void cmd_thread_signal(void) {
    if (pthread_cond_signal(&cmd_thread_cond) < 0)
        abort(); // TODO: error handling
}
