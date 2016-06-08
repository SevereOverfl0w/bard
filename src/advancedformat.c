//Copyright (C) 2015 Jesper Jensen
//    This file is part of bard.
//
//    bard is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    bard is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with bard.  If not, see <http://www.gnu.org/licenses/>.
#define _GNU_SOURCE
#include "advancedformat.h"
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <wait.h>
#include <wordexp.h>
#include "formatter.h"
#include "myerror.h"
#include "logger.h"

int advformat_execute(jmp_buf jmpBuf, char* format, Pvoid_t compiledEnv, const struct FormatArray* fmtArrays[], size_t fmtLen, char** out) {
	char* null = NULL;
	size_t cmdlen = 0;
	char* ch = format;
	char** w;
	//Get length of initial command

	wordexp_t p;
	wordexp(format, &p, 0);
	w = p.we_wordv;

	int pipefd[2];
	pipe(pipefd);

	log_write(LEVEL_INFO, "Executing: %s", w[0]);

	//Start the child
	int pid = fork();
	if(pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);

		char index[12] = "\0"; //Max ENV len
		Vector** vec;
		JSLF(vec, compiledEnv, index);
		while(vec != NULL) {
			char* str;
			formatter_format(jmpBuf, *vec, fmtArrays, fmtLen, &str);
			log_write(LEVEL_INFO, "Env %s -> %s", index, str);
			setenv(index, str, true);
			JSLN(vec, compiledEnv, index);
		}

		execvp(w[0], w);
		exit(0);
	}

	close(pipefd[1]);
	FILE* output = fdopen(pipefd[0], "r");

	wordfree(&p);

	Vector buff;
	vector_init(jmpBuf, &buff, sizeof(char), 1024);
	ssize_t readLen;

	/* Read output */
	char chunk[1024];
	while((readLen = fread(chunk, 1, 1024, output)) > 0){
		log_write(LEVEL_INFO, "Read: %d", readLen);
		vector_putListBack(jmpBuf, &buff, chunk, readLen);
	}
	if(ferror(output)) {
		longjmp(jmpBuf, errno);
	}

	//Remove trailing newline
	if(buff.size > 0 && buff.data[buff.size-1] == '\n')
		buff.data[buff.size-1] = '\0';
	else
		vector_putListBack(jmpBuf, &buff, "\0", 1);

	int status;
	waitpid(pid, &status, 0);
	int exitCode = WEXITSTATUS(status);
	close(pipefd[0]);
	log_write(LEVEL_INFO, "Done: %d -> %s", exitCode, buff.data);
	*out = vector_detach(&buff);
	return exitCode;
}
