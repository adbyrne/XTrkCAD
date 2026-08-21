
/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2018 Martin Fischer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

#define I18NHEADERFILE "i18n.h"

/* Reject a ".." path-traversal component -- the actual CodeQL-recommended
 * defense against cpp/path-injection (a comment doesn't break the taint
 * chain a real dataflow scan follows; this does). Real invocations always
 * come from CMake as absolute paths under the project's own source/build
 * tree (${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_CURRENT_BINARY_DIR}) and never
 * need "..", so this can't reject a legitimate build. */
static int
PathHasTraversal(const char *path)
{
	size_t i, len = strlen(path);

	for (i = 0; i < len; i++) {
		int atStart = (i == 0) || (path[i - 1] == '/') || (path[i - 1] == '\\');

		if (atStart && path[i] == '.' && path[i + 1] == '.' &&
		    (path[i + 2] == '\0' || path[i + 2] == '/' || path[i + 2] == '\\')) {
			return 1;
		}
	}

	return 0;
}

typedef enum { TOOLTIPS, TOOLTIPS_I18N } mode_e;

struct messageData {
	char* line;
	char* message;
};

int
CompareMessages(const void *msg1, const void *msg2)
{
	return(strcmp(((const struct messageData *)msg1)->line,
	              ((const struct messageData *)msg2)->line));
}

int process(mode_e mode, char * json, FILE * outFile)
{
	const cJSON *messages = NULL;
	const cJSON *messageLine = NULL;
	int cntMessages;
	int currentLine = 0;
	int status = 0;
	struct messageData *messageList = NULL;

	cJSON *message_json = cJSON_Parse(json);
	if (message_json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) {
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		status = 0;
		goto end;
	}

	fputs("/*\n * DO NOT EDIT! This file has been automatically created by genhelp.\n"
	      " * Changes to this file will be overwritten. Edit this: genhelp.json\n */\n",
	      outFile);
	;
	fprintf(outFile, "#include \"wlib.h\"\n");

	if (mode == TOOLTIPS_I18N) {
		fprintf(outFile, "#include \"" I18NHEADERFILE "\"\n");
	}

	messages = cJSON_GetObjectItemCaseSensitive(message_json, "messages");
	cntMessages = cJSON_GetArraySize(messages);
	messageList = malloc(sizeof(struct messageData) * cntMessages);
	if (!messageList) {
		fprintf(stderr, "Could not allocate storage for message list\n");
		return(1);
	}

	fprintf(outFile, "wTooltip_t tooltipTexts[] = {\n");

	cJSON_ArrayForEach(messageLine, messages) {
		cJSON* line = cJSON_GetObjectItemCaseSensitive(messageLine, "line");
		cJSON* contents = cJSON_GetObjectItemCaseSensitive(messageLine, "contents");

		if (!cJSON_IsString(line) || !cJSON_IsString(contents)) {
			status = 0;
			goto end;
		}

		messageList[currentLine].line = line->valuestring;
		if (contents->valuestring[0]) {
			messageList[currentLine].message = contents->valuestring;
		} else {
			messageList[currentLine].message = NULL;
		}
		currentLine++;
	}

	qsort(messageList, cntMessages, sizeof(struct messageData),  CompareMessages);

	for(int i=0; i < cntMessages; i++ ) {

		if (messageList[i].message != NULL) {
			if (mode == TOOLTIPS) {
				fprintf(outFile, "\t{ \"%s\", \"%s\" },\n", messageList[i].line,
				        messageList[i].message);
			} else {
				fprintf(outFile, "\t{ \"%s\", N_(\"%s\") },\n", messageList[i].line,
				        messageList[i].message);

			}
		} else {
			fprintf(outFile, "\t{ \"%s\", NULL },\n", messageList[i].line);
			fprintf(stderr, "INFO: %s has an empty help text\n", messageList[i].line);
		}
	}

	fprintf(outFile, "\t{ NULL, NULL } };\n");

	fprintf(outFile, "\n\nunsigned TooltipsGetCount(void)\n{\n\treturn(%d);\n}\n",
	        cntMessages);

end:
	cJSON_Delete(message_json);
	free(messageList);
	return status;
}


int main(int argc, const char * argv[])
{
	FILE * inFile, * outFile;
	char *jsonData;

	mode_e mode;
	if (argc != 4) {
		fprintf(stderr, "Usage: %s (-bh|-bhi) JSONFILE OUTFILE\n", argv[0]);
		exit(1);
	}

	if (strcmp(argv[1], "-bh") == 0) {
		mode = TOOLTIPS;
	} else if (strcmp(argv[1], "-bhi") == 0) {
		mode = TOOLTIPS_I18N;
	} else {
		fprintf(stderr, "Bad mode: %s\n", argv[1]);
		exit(1);
	}

	if (PathHasTraversal(argv[2])) {
		fprintf(stderr, "Rejected: %s contains a '..' path component\n", argv[2]);
		exit(1);
	}

	inFile = fopen(argv[2], "r");
	if (inFile == NULL) {
		perror(argv[2]);
		exit(1);
	}

	unsigned int length;
	fseek(inFile, 0, SEEK_END);
	length = ftell(inFile);
	fseek(inFile, 0, SEEK_SET);
	jsonData = malloc(length + 1);
	if (jsonData) {
		jsonData[ fread(jsonData, 1, length, inFile) ] = '\0';
	}
	fclose(inFile);

	if (PathHasTraversal(argv[3])) {
		fprintf(stderr, "Rejected: %s contains a '..' path component\n", argv[3]);
		exit(1);
	}

	outFile = fopen(argv[3], "w");
	if (outFile == NULL) {
		perror(argv[3]);
		exit(1);
	}

	int ret = process(mode, jsonData, outFile);
	exit(ret);
}
