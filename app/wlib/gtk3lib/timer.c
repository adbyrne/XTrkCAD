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
//static struct timeval startTime;

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
 * Pause for <count> milliseconds. The function waits for a condition that
 * is never met. So it blocks until it runs into a timeout.
 *
 * \param duration IN  duration of pause in milliseconds
 */

void wPause(long duration)		/* milliseconds */
{
	while (gtk_events_pending())
	    gtk_main_iteration();			//Allow GTK to finish before pausing

    gdk_display_sync(gdk_display_get_default());

    GMutex mutex;
    GCond cond;
    gint64 end_time = g_get_monotonic_time() + duration * G_TIME_SPAN_MILLISECOND;

    g_cond_init(&cond);
    g_mutex_init(&mutex);

    g_mutex_lock(&mutex);

    g_cond_wait_until(&cond, &mutex, end_time);

    g_mutex_unlock(&mutex);

    g_mutex_clear(&mutex);
    g_cond_clear(&cond);
}

/**
 * Get time expired since start???
 * \todo Check where start time is initialized!!!
 *
 * \returns time in seconds
 */

unsigned long wGetTimer(void)
{
    printf("Not implemented wGetTimer: %s %d\n", __FILE__, __LINE__);
    return(0L);
    //struct timeval tv;
    //struct timezone tz;
    //
    //gettimeofday(&tv, &tz);
    //return (tv.tv_sec-startTime.tv_sec+1) * 1000 + tv.tv_usec /1000;
}
