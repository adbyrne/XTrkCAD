/** \file osxhelp.c
 * use OSX Help system
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2015 Martin Fischer
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "gtkint.h"
#include "i18n.h"

#define HELPCOMMANDPIPE "/tmp/helppipe"
#define EXITCOMMAND "##exit##"
#define HELPERPROGRAM "helphelper"

static pid_t pidOfChild;
static int handleOfPipe;
extern char *wExecutableName;

/**
 * Create the fully qualified filename for the help helper
 *
 * \param parent IN path and filename of parent process
 * \return filename for the child process
 */

static
char *ChildProgramFile(char *parentProgram)
{
    char *startOfFilename;
    char *childProgram;

    childProgram = malloc(strlen(parentProgram)+ sizeof(HELPERPROGRAM) + 1);
    strcpy(childProgram, parentProgram);
    startOfFilename = strrchr(childProgram, '/');
    strcpy(startOfFilename + 1, HELPERPROGRAM);

    return (childProgram);
}


/**
 * Invoke the help system to display help for <topic>.
 *
 * \param topic IN topic string
 */

void wHelp(const char * topic)
{
    pid_t newPid;
    int len;
    int status;
    const char html[] = ".html";
    char *page;
    
    // check whether child already exists
    if (pidOfChild != 0) {
        if (waitpid(pidOfChild, &status, WNOHANG) > 0) {
            // child exited -> clean up
            close(handleOfPipe);
            unlink(HELPCOMMANDPIPE);
            handleOfPipe = 0;
            pidOfChild = 0;		// child exited
        }
    }

    // (re)start child
    if (pidOfChild == 0) {
        mkfifo(HELPCOMMANDPIPE, 0666);
        newPid = fork();  /* New process starts here */

        if (newPid > 0) {
            pidOfChild = newPid;
        } else if (newPid == 0) {
			char *child = ChildProgramFile(wExecutableName);
            
            if (execlp(child, child, NULL) < 0) {   /* never normally returns */
                exit(8);
            }
            
            free(child);
        } else { /* -1 signifies fork failure */
            pidOfChild = 0;
            return;
        }
    }

    if (!handleOfPipe) {
        handleOfPipe = open(HELPCOMMANDPIPE, O_WRONLY);

        if (handleOfPipe < 0) {
            kill(pidOfChild, SIGKILL);  /* tidy up on next call */
        }

    }

    page = malloc(strlen(topic) + strlen(html) + 1);

    if (!page) {
        return;
    }

    strcpy(page, topic);
    strcat(page, html);
    len = strlen(page);

    write(handleOfPipe, &len, sizeof(int));
    write(handleOfPipe, page, strlen(page)+1);

    free(page);
}
