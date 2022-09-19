/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/

#if !defined(__MQTT_LINUX_)
#define __MQTT_LINUX_

#if defined(WIN32_DLL) || defined(WIN64_DLL)
  #define DLLImport __declspec(dllimport)
  #define DLLExport __declspec(dllexport)
#elif defined(LINUX_SO)
  #define DLLImport extern
  #define DLLExport  __attribute__ ((visibility ("default")))
#else
  #define DLLImport
  #define DLLExport
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "log_c.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#define LOG_DOMAIN_MQTT  0x9527

#ifndef NAPI_MQTT_LOG_TAG
#define NAPI_MQTT_LOG_TAG "MQTT"
#endif // !NAPI_MQTT_LOG_TAG
// #define MQTT_RELEASE_BUILD 1
#ifndef MQTT_RELEASE_BUILD
#define LogDebug(...) ((void)HiLogPrint(LOG_APP, LOG_DEBUG, LOG_DOMAIN_MQTT, NAPI_MQTT_LOG_TAG, __VA_ARGS__))
#define LogInfo(...) ((void)HiLogPrint(LOG_APP, LOG_INFO, LOG_DOMAIN_MQTT, NAPI_MQTT_LOG_TAG, __VA_ARGS__))
#define LogWarn(...) ((void)HiLogPrint(LOG_APP, LOG_WARN, LOG_DOMAIN_MQTT, NAPI_MQTT_LOG_TAG, __VA_ARGS__))
#define LogError(...) ((void)HiLogPrint(LOG_APP, LOG_ERROR, LOG_DOMAIN_MQTT, NAPI_MQTT_LOG_TAG, __VA_ARGS__))
#define LogFatal(...) ((void)HiLogPrint(LOG_APP, LOG_FATAL, LOG_DOMAIN_MQTT, NAPI_MQTT_LOG_TAG, __VA_ARGS__))
#else // !MQTT_RELEASE_BUILD
#define LogDebug(...)
#define LogInfo(...)
#define LogWarn(...)
#define LogError(...) ((void)HiLogPrint(LOG_APP, LOG_ERROR, LOG_DOMAIN_MQTT, NAPI_MQTT_LOG_TAG, __VA_ARGS__))
#define LogFatal(...) ((void)HiLogPrint(LOG_APP, LOG_FATAL, LOG_DOMAIN_MQTT, NAPI_MQTT_LOG_TAG, __VA_ARGS__))
#endif
typedef struct Timer
{
    struct timeval end_time;
} Timer;

void TimerInit(Timer*);
char TimerIsExpired(Timer*);
void TimerCountdownMS(Timer*, unsigned int);
void TimerCountdown(Timer*, unsigned int);
int TimerLeftMS(Timer*);
void TimerAddSecond(Timer* timer, unsigned int time);

typedef struct Network
{
    int my_socket;
    int (*mqttread) (struct Network*, unsigned char*, int, int);
    int (*mqttwrite) (struct Network*, unsigned char*, int, int);
} Network;

int linux_read(Network*, unsigned char*, int, int);
int linux_write(Network*, unsigned char*, int, int);

DLLExport void NetworkInit(Network*);
DLLExport int NetworkConnect(Network*, char*, int);
DLLExport void NetworkDisconnect(Network*);

#endif
