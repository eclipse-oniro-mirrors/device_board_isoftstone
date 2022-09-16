/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp.
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
 *    Ian Craggs - return codes from linux_read
 *******************************************************************************/

#include "MQTTLinux.h"

void TimerInit(Timer* timer)
{
    timer->end_time = (struct timeval){0, 0};
}

char TimerIsExpired(Timer* timer)
{
    struct timeval now, res;
    gettimeofday(&now, NULL);
    timersub(&timer->end_time, &now, &res);
    return res.tv_sec < 0 || (res.tv_sec == 0 && res.tv_usec <= 0);
}


void TimerCountdownMS(Timer* timer, unsigned int timeout)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    struct timeval interval = {timeout / 1000, (timeout % 1000) * 1000};
    timeradd(&now, &interval, &timer->end_time);
}


void TimerAddSecond(Timer* timer, unsigned int time)
{
    struct timeval interval = {time, 0};
    timeradd(&timer->end_time, &interval, &timer->end_time);
}


void TimerCountdown(Timer* timer, unsigned int timeout)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    struct timeval interval = {timeout, 0};
    timeradd(&now, &interval, &timer->end_time);
}


int TimerLeftMS(Timer* timer)
{
    struct timeval now, res;
    gettimeofday(&now, NULL);
    timersub(&timer->end_time, &now, &res);
    //printf("left %d ms\n", (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000);
    return (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000;
}


int linux_read(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
    struct timeval interval = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (interval.tv_sec < 0 || (interval.tv_sec == 0 && interval.tv_usec <= 0))
    {
        interval.tv_sec = 0;
        interval.tv_usec = 100;
    }
    setsockopt(n->my_socket, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int));
    setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&interval, sizeof(struct timeval));
    Timer timer;
    TimerInit(&timer);
    TimerCountdownMS(&timer, timeout_ms);
    int bytes = 0;
    int rc = 0;
    while (bytes < len)
    {
        rc = recv(n->my_socket, &buffer[bytes], (size_t)(len - bytes), 0);
        if (rc == -1)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK){
                bytes = -1;
                break;
            }
            if (TimerIsExpired(&timer)){
                // LogDebug("linux_read timeout...");
                break;
            }
            continue;
        }
        else if (rc == 0)
        {
            bytes = 0;
            break;
        }
        else{
            TimerCountdownMS(&timer, timeout_ms);
            bytes += rc;
        }
    }
    return bytes;
}


int linux_write(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
    struct timeval interval;
    Timer timer;
    TimerInit(&timer);
    TimerCountdownMS(&timer, timeout_ms);
    int    rc = 0;
    setsockopt(n->my_socket, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int));
    setsockopt(n->my_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&interval,sizeof(struct timeval));
    while (rc < len && !TimerIsExpired(&timer)) {
        interval.tv_sec = 0;  /* 30 Secs Timeout */
        interval.tv_usec = timeout_ms * 1000;  // Not init'ing this can cause strange errors
        rc = write(n->my_socket, buffer, len);
        if (rc == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                if (errno != EPIPE) {
                    LogDebug("fatal error: write buffer error,errorno = %{public}d", errno);
                    break;
                }
            }
            continue;
        }
    }
    return rc;
}


void NetworkInit(Network* n)
{
    n->my_socket = 0;
    n->mqttread = linux_read;
    n->mqttwrite = linux_write;
}


int NetworkConnect(Network* n, char* addr, int port)
{
    int type = SOCK_STREAM;
    struct sockaddr_in address;
    int rc = -1;
    sa_family_t family = AF_INET;
    struct addrinfo *result = NULL;
    struct addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};

    if ((rc = getaddrinfo(addr, NULL, &hints, &result)) == 0)
    {
        struct addrinfo* res = result;

        /* prefer ip4 addresses */
        while (res)
        {
            if (res->ai_family == AF_INET)
            {
                result = res;
                break;
            }
            res = res->ai_next;
        }

        if (result->ai_family == AF_INET)
        {
            address.sin_port = htons(port);
            address.sin_family = family = AF_INET;
            address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
        }
        else
            rc = -1;

        freeaddrinfo(result);
    }

    if (rc == 0)
    {
        n->my_socket = socket(family, type, 0);
        if (n->my_socket != -1) {
          fcntl(n->my_socket, F_SETFL, fcntl(n->my_socket, F_GETFL) | O_NONBLOCK);
          rc = connect(n->my_socket, (struct sockaddr*)&address, sizeof(address));
          if (rc == 0 || (rc != 0 && errno == EINPROGRESS)) {
             rc = 0;
             int retval;
             fd_set set;
             FD_ZERO(&set);
             FD_SET(n->my_socket, &set);
             struct timeval timeo = { 2, 0};
             retval = select(n->my_socket + 1, NULL, &set, NULL, &timeo);
             socklen_t len = sizeof(timeo);
             if (retval == -1) {
                     return -1;
             } else if(retval == 0) {
                     LogDebug("connect timeout");
                     return -1;
             }
             return 0;
          }
     }
        else
            rc = -1;
    }

    return rc;
}


void NetworkDisconnect(Network* n)
{
    close(n->my_socket);
}
