/*
 * Author: daddinuz
 * email:  daddinuz@gmail.com
 *
 * Copyright (c) 2018 Davide Di Carlo
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <time.h>
#include <stdlib.h>
#include <unistd.h>  // just for sleep() and getpid()
#include <process.h>
#include <panic/panic.h>

#define bufferSize  256
char buffer[bufferSize + 1] = "";

size_t readLine(FILE *const stream, char *const buffer, const size_t size) {
    size_t i;
    for (i = 0; feof(stream) && i < size; i++) {
        const char c = fgetc(stream);
        if ('\n' == c) {
            break;
        }
        buffer[i] = c;
    }
    return i;
}

void doSomething(void) {
    const pid_t pid = getpid();

    srand((unsigned) pid);
    sleep(10 - (unsigned) rand() % 5);
    const size_t bytesRead = readLine(stdin, buffer, bufferSize);
    printf("%s:%d:%.*s:%lu", __func__, pid, (int) bytesRead, buffer, time(NULL));
}

int main() {
    struct Process_ExitInfo info;
    struct Process processList[5];
    const struct Process *processListEnd = &processList[sizeof(processList) / sizeof(processList[0])];

    for (struct Process *process = processList; process < processListEnd; process++) {
        if (Process_spawn(process, doSomething) != Ok) {
            Panic_terminate("Unable to fork");
        }
        printf("Spawned: %d\n", Process_id(process));
    }

    printf("Canceling: %d\n", Process_id(&processList[2]));
    Process_cancel(&processList[2]);
    printf("Canceled: %d\n", Process_id(&processList[2]));

    for (struct Process *process = processList; process < processListEnd; process++) {
        if (Process_isAlive(process)) {
            const int size = snprintf(buffer, bufferSize, "%ld\n", time(NULL));
            if (size <= 0) {
                Panic_terminate("Unexpected error while writing to buffer");
            }
            if (Process_writeInputStream(process, buffer, (size_t) size) < 0) {
                Panic_terminate("Unexpected error while writing to input stream of process: %d", Process_id(process));
            }
        }
    }

    for (struct Process *process = processList; process < processListEnd; process++) {
        const Error e = Process_isAlive(process) ? Process_wait(process, &info) : Process_exitInfo(process, &info);
        if (e != Ok) {
            Panic_terminate("%s", Error_explain(e));
        }
        const long bytesRead = Process_readOutputStream(process, buffer, bufferSize);
        if (bytesRead < 0) {
            Panic_terminate("Unexpected error while reading from output stream of process: %d", Process_id(process));
        }
        printf("Process: %d normallyExited: %d exitValue: %2d output: %.*s\n",
               Process_id(process), info.normallyExited, info.exitValue, (int) bytesRead, buffer);
        Process_teardown(process);
    }

    return 0;
}
