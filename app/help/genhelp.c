
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

typedef enum { BALLOONHELP, BALLOONHELPI18N } mode_e;

struct messageData {
    char* line;
    char* message;
};

int
CompareMessages(const struct messageData *msg1, const struct messageData *msg2)
{
    return(strcmp(msg1->line, msg2->line));
}

int process(mode_e mode, char * json, FILE * outFile)
{
    const cJSON *messages = NULL;
    const cJSON *messageLine = NULL;
    int cntMessages;
    int i = 0;
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

    fputs("/*\n * DO NOT EDIT! This file has been automatically created by genhelp.\n * Changes to this file will be overwritten. Edit this: genhelp.json\n */\n",
          outFile);
    fprintf(outFile, "#include <stdio.h>\n");
    fprintf(outFile, "#include \"wlib.h\"\n");
    if (mode == BALLOONHELPI18N) {
        fprintf(outFile, "#include \"" I18NHEADERFILE "\"\n");
    }

    fprintf(outFile, "wBalloonHelp_t balloonHelp[] = {\n");


    messages = cJSON_GetObjectItemCaseSensitive(message_json, "messages");
    cntMessages = cJSON_GetArraySize(messages);
    messageList = malloc(sizeof(struct messageData) * cntMessages);
    if (!messageList) {
        fprintf(stderr, "Could not allocate storage for message list\n");
        return(1);
    }

    cJSON_ArrayForEach(messageLine, messages) {
        cJSON* line = cJSON_GetObjectItemCaseSensitive(messageLine, "line");
        cJSON* contents = cJSON_GetObjectItemCaseSensitive(messageLine, "contents");

        if (!cJSON_IsString(line) || !cJSON_IsString(contents)) {
            status = 0;
            goto end;
        }

        messageList[i].line = line->valuestring;
        messageList[i].message = contents->valuestring;
        i++;
    }

    qsort(messageList, cntMessages, sizeof(struct messageData),  CompareMessages);

    for(int i=0; i < cntMessages; i++ ) {

        if (messageList[i].message != NULL) {
            if (mode == BALLOONHELP) {
                fprintf(outFile, "\t{ \"%s\", \"%s\" },\n", messageList[i].line,
                    messageList[i].message);
            } else {
                if (messageList[i].message[0]) {
                    fprintf(outFile, "\t{ \"%s\", N_(\"%s\") },\n", messageList[i].line,
                        messageList[i].message);
                } else {
                     fprintf(outFile, "\t{ \"%s\", \"\" },\n", messageList[i].line);
                }
            }
        } else {
            fprintf(outFile, "\t{ \"%s\", \"No help\" },\n", messageList[i].line);
            fprintf(stderr, "INFO: %s has an empty help text\n", messageList[i].line);
        }
    }

    fprintf(outFile, "\t{ NULL, NULL } };\n");
end:
    cJSON_Delete(message_json);
    return status;
}


int main(int argc, char * argv[])
{
    FILE * inFile, * outFile;
    char *jsonData;

    mode_e mode;
    if (argc != 4) {
        fprintf(stderr, "Usage: %s (-bh|-bhi) JSONFILE OUTFILE\n", argv[0]);
        exit(1);
    }

    if (strcmp(argv[1], "-bh") == 0) {
        mode = BALLOONHELP;
    } else if (strcmp(argv[1], "-bhi") == 0) {
        mode = BALLOONHELPI18N;
    } else {
        fprintf(stderr, "Bad mode: %s\n", argv[1]);
        exit(1);
    }

    inFile = fopen(argv[2], "r");
    if (inFile == NULL) {
        perror(argv[2]);
        exit(1);
    }

    if (inFile) {
        unsigned int length;
        fseek(inFile, 0, SEEK_END);
        length = ftell(inFile);
        fseek(inFile, 0, SEEK_SET);
        jsonData = malloc(length + 1);
        if (jsonData) {
            fread(jsonData, 1, length, inFile);
            jsonData[length] = '\0';
        }
        fclose(inFile);
    }

    outFile = fopen(argv[3], "w");
    if (outFile == NULL) {
        perror(argv[3]);
        exit(1);
    }

    int ret = process(mode, jsonData, outFile);
    exit(ret);
}
