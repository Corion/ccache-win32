/*
   Copyright (C) Andrew Tridgell 2002
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "ccache.h"

#ifdef _WIN32
static char *argvtos(char **argv)
{
       int i, len;
       char *ptr, *str;

       for (i = 0, len = 0; argv[i]; i++) {
               len += strlen(argv[i]) + 3;
       }

       str = ptr = (char *)malloc(len + 1);
       if (str == NULL)
               return NULL;

       for (i = 0; argv[i]; i++) {
               len = strlen(argv[i]);
               *ptr++ = '"';
               memcpy(ptr, argv[i], len);
               ptr += len;
               *ptr++ = '"';
               *ptr++ = ' ';
       }
       *ptr = 0;

       return str;
}
#endif

/*
  execute a compiler backend, capturing all output to the given paths
  the full path to the compiler to run is in argv[0]
*/
int execute(char **argv, 
	    const char *path_stdout,
	    const char *path_stderr)
{
#ifdef _WIN32
	PROCESS_INFORMATION pinfo; 
	STARTUPINFO sinfo;
	BOOL ret; 
	DWORD exitcode;

	int   status = -2;
	int   fd, std_od = -1, std_ed = -1;
	
	cc_log("Executing...\n");
	
	unlink(path_stdout);
	std_od = _dup(1);
	fd = _open(path_stdout, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_BINARY, 0666);
	if (fd == -1) {
		status = STATUS_NOCACHE;
		cc_log("stdout error: failed to open %s\n", path_stdout);
		goto out;
	}
	_dup2(fd, 1);
	_close(fd);

	unlink(path_stderr);
	fd = _open(path_stderr, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_BINARY, 0666);
	std_ed = _dup(2);
	if (fd == -1) {
		status = STATUS_NOCACHE;
		cc_log("stderr error: failed to open %s\n", path_stderr);
		goto out;
	}
	_dup2(fd, 2);
	_close(fd);

        int argc;
        
        for (argc = 0; argv[argc]; argc++) {
		// printf("%d: Found argument %s\n", argc, argv[argc]);
        };
        
        cc_log("Quoting %d arguments\n", argc);
        ARGS* quoted_args = args_init_q(argc, argv);
        char** quoted_argv = quoted_args->argv;

	/* Spawn process (_exec* familly doesn't return) */
	status = _spawnv(_P_WAIT, quoted_argv[0], quoted_argv);

out:
	cc_log("%s:\n  stdout -> %s\n  stderr -> %s\n  process status=%i\n",
	        quoted_argv[0], path_stdout, path_stderr, status);
	if (status == -1) cc_log("Error %i: %s\n", errno, strerror(errno));

	/* Restore descriptors */
	if (std_od != -1) _dup2(std_od, 1);
	if (std_ed != -1) _dup2(std_ed, 2);
	_flushall();
	return (status>0);
	
#else
	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1) fatal("Failed to fork");
	
	if (pid == 0) {
		int fd;

		unlink(path_stdout);
		fd = open(path_stdout, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_BINARY, 0666);
		if (fd == -1) {
			exit(STATUS_NOCACHE);
		}
		dup2(fd, 1);
		close(fd);

		unlink(path_stderr);
		fd = open(path_stderr, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_BINARY, 0666);
		if (fd == -1) {
			exit(STATUS_NOCACHE);
		}
		dup2(fd, 2);
		close(fd);

		exit(execv(argv[0], argv));
	}

	if (waitpid(pid, &status, 0) != pid) {
		fatal("waitpid failed");
	}

	if (WEXITSTATUS(status) == 0 && WIFSIGNALED(status)) {
		return -1;
	}

	return WEXITSTATUS(status);
#endif
}


/*
  find an executable by name in $PATH. Exclude any that are links to exclude_name 
*/
char *find_executable(const char *name, const char *exclude_name)
{
#if _WIN32
	DWORD ret;
	char namebuf[MAX_PATH];

        ret = SearchPathA(getenv("CCACHE_PATH"), name, ".exe",
                          sizeof(namebuf), namebuf, NULL);
        if (ret != 0) {
        	return x_strdup(namebuf);
        }

        return NULL;
#else
	char *path;
	char *tok;
	struct stat st1, st2;

	if (*name == '/') {
		return x_strdup(name);
	}

	path = getenv("CCACHE_PATH");
	if (!path) {
		path = getenv("PATH");
	}
	if (!path) {
		cc_log("no PATH variable!?\n");
		return NULL;
	}

	path = x_strdup(path);
	
	/* search the path looking for the first compiler of the right name
	   that isn't us */
	for (tok=strtok(path,":"); tok; tok = strtok(NULL, ":")) {
		char *fname;
		x_asprintf(&fname, "%s"PATH_SEP"%s", tok, name);
		/* look for a normal executable file */
		if (access(fname, X_OK) == 0 &&
		    lstat(fname, &st1) == 0 &&
		    stat(fname, &st2) == 0 &&
		    S_ISREG(st2.st_mode)) {
			/* if its a symlink then ensure it doesn't
                           point at something called exclude_name */
			if (S_ISLNK(st1.st_mode)) {
				char *buf = x_realpath(fname);
				if (buf) {
					char *p = str_basename(buf);
					if (strcmp(p, exclude_name) == 0) {
						/* its a link to "ccache" ! */
						free(p);
						free(buf);
						continue;
					}
					free(buf);
					free(p);
				}
			}

			/* found it! */
			free(path);
			return fname;
		}
		free(fname);
	}

	return NULL;
#endif
}
