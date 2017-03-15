/** \file timer.c
 * Timer Functions
 *
 * Copyright 2016 Martin Fischer <m_fischer@sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
//#include "i18n.h"

static wBool_t gtkPaused = FALSE;
static int alarmTimer = 0;
static struct timeval startTime;

static wControl_p triggerControl = NULL;
static setTriggerCallback_p triggerFunc = NULL;

/**
 * Signal handler for alarm timer - executes the defined function when
 * timer expires
 *
 * \param data IN callback function
 * \returns alwys 0
 */

static gint doAlarm(
    gpointer data)
{
    wAlarmCallBack_p func = (wAlarmCallBack_p)data;

    func();

    alarmTimer = 0;
    return FALSE;
}

/**
 * Alarm for <count> milliseconds.
 *
 * \param count IN time to wait
 * \param func IN function called when timer expires
 */

void wAlarm(
    long count,
    wAlarmCallBack_p func)		/* milliseconds */
{
    gtkPaused = TRUE;

    if (alarmTimer) {
        g_source_remove(alarmTimer);
    }

    alarmTimer = g_timeout_add(count, doAlarm, (void *)(GSourceFunc)func);
}

static void doTrigger(void)
{
    if (triggerControl && triggerFunc) {
        triggerFunc(triggerControl);
        triggerFunc = NULL;
        triggerControl = NULL;
    }
}

void wlibSetTrigger(
    wControl_p b,
    setTriggerCallback_p trigger)
{
    triggerControl = b;
    triggerFunc = trigger;
    wAlarm(500, doTrigger);
}

/**
 * Pause for <count> milliseconds.
 *
 * \param count IN
 */

void wPause(
    long count)		/* milliseconds */
{
    struct timeval timeout;
    sigset_t signal_mask;
    sigset_t oldsignal_mask;
    gdk_display_sync(gdk_display_get_default());
    timeout.tv_sec = count/1000;
    timeout.tv_usec = (count%1000)*1000;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGIO);
    sigaddset(&signal_mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &signal_mask, &oldsignal_mask);

    if (select(0, NULL, NULL, NULL, &timeout) == -1) {
        perror("wPause:select");
    }

    sigprocmask(SIG_BLOCK, &oldsignal_mask, NULL);
}

/**
 * Get time expired since start???
 * \todo Check where start time is initialized!!!
 *
 * \returns time in seconds
 */

unsigned long wGetTimer(void)
{
    struct timeval tv;
    struct timezone tz;
    
    gettimeofday(&tv, &tz);
    return (tv.tv_sec-startTime.tv_sec+1) * 1000 + tv.tv_usec /1000;
}
