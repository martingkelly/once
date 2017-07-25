/*
This is a simple program used to implement a shell singleton. It is useful
when you want to run a program if and only if the program isn't currently
running.  An example is autostarted programs in desktop environments. If you
configure a program to run whenever your DE starts and it doesn't depend on
the GUI, then if you logout and login again you will now have two instances
of that program running. By wrapping the program invocation with this script,
that issue won't happen.

Copyright 2015 Martin Kelly <martin@martingkelly.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

void usage(void) {
    printf(
"solo implements a shell singleton. By wrapping acommand invocation with solo\n"
"solo, the program is guaranteed to be the only one among others wrapped with\n"
"solo that is running at a given time.\n"
"\n"
"Usage: solo COMMAND\n"
"       solo [-l|--lockfile] LOCKFILE COMMAND\n");
}

const char *get_lockdir(void) {
    /* Test possible temporary directory locations. */
    static const char *paths[] = {"/var/lock", "/tmp"};
    for (size_t i = 0; i < ARRAY_SIZE(paths); i++) {
        struct stat s;
        const char *path = paths[i];
        int result = stat(path, &s);
        if (result != 0) {
            perror("stat");
            return NULL;
        }

        if (S_ISDIR(s.st_mode)) {
            return path;
        }
    }

    /* If all else fails, this is our fallback. */
    return ".";
}

const char *lockfile = NULL;
int lockfile_fd = -1;
void graceful_exit(int status) {
    /*
     * Cleanup. It's important that we remove the file before closing it, as
     * closing it drops the lock. If we close the file first, it's possible someone
     * else grabs the lock, which we then remove. Since doing an flock on a nonexistent
     * file always succeeds, this would be bad.
     */
    if (lockfile != NULL) {
        unlink(lockfile);
    }
    if (lockfile_fd != -1) {
        close(lockfile_fd);
    }
    exit(status);
}

void signal_handler(int signal __attribute__((unused))) {
    graceful_exit(1);
}

int install_signal_handlers(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    int signals[] = { SIGHUP, SIGINT, SIGTERM };
    for (size_t i = 0; i < ARRAY_SIZE(signals); i++) {
        int result = sigaction(signals[i], &action, NULL);
        if (result == -1) {
            perror("sigaction");
            return errno;
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    /*
     * Check if the lockfile is specified as an option. Importantly, we must
     * not read farther than the first argument, as -l/--lockfile may be a
     * legitimate argument to the command itself.
     */
    int cmd_start;
    if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--lockfile") == 0) {
        if (argc < 3) {
            usage();
            return 1;
        }
        lockfile = argv[2];
        cmd_start = 3;
    }
    else {
        cmd_start = 1;
        const char *lockdir = get_lockdir();
        if (lockdir == NULL) {
            fprintf(stderr, "Cannot find a temporary lock directory!\n");
            return 1;
        }

        const char *program_name = basename(argv[cmd_start]);
        if (*program_name == '\0') {
            fprintf(stderr, "Invalid program name %s\n", argv[cmd_start]);
            return 1;
        }

        char filepath[PATH_MAX];
        int result = snprintf(filepath,
                              sizeof(filepath),
                              "%s/solo-lock-%s",
                              lockdir,
                              program_name);
        if (result >= (int) sizeof(filepath)) {
            fprintf(stderr, "Command or lockdir too large; this is likely a "
                            "bug. Please report it!\n");
        }
        lockfile = filepath;
    }

    /*
     * Install signal handlers so we can cleanup if something goes wrong after
     * creating the lockfile.
     */
    int result = install_signal_handlers();
    if (result != 0) {
        return result;
    }

    lockfile_fd = creat(lockfile, S_IWUSR);
    if (lockfile_fd == -1) {
        perror("lock file creation failed");
        graceful_exit(errno);
    }

    result = lockf(lockfile_fd, F_TLOCK, 0);
    if (result == -1) {
        if (errno == EACCES || errno == EAGAIN) {
            fprintf(stderr, "Another instance is already running\n");
        }
        else {
            perror("cannot take lock");
        }
        graceful_exit(errno);
    }

    pid_t pid = fork();
    if (pid == 0) {
        /* Child */
        close(lockfile_fd);
        result = execvp(argv[cmd_start], &argv[cmd_start]);
        if (result == -1) {
            perror("failed to exec program");
            /* Don't try to cleanup; let the parent handle that */
            return errno;
        }
    }
    else if (pid > 0) {
        /* Parent */
        result = waitpid(pid, NULL, 0);
        if (result == -1) {
            perror("waitpid");
            graceful_exit(errno);
        }
    }
    else {
      perror("fork");
      graceful_exit(errno);
    }

    graceful_exit(0);
}
