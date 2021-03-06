#ifndef __X_TIME_UTILS_
#define __X_TIME_UTILS_

/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "PlatformDefs.h"

VOID GetLocalTime(LPSYSTEMTIME);

DWORD GetTickCount(void);

BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);

void WINAPI Sleep(DWORD dwMilliSeconds);

BOOL   FileTimeToLocalFileTime(const FILETIME* lpFileTime, LPFILETIME lpLocalFileTime);
BOOL   SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime,  LPFILETIME lpFileTime);
LONG   CompareFileTime(const FILETIME* lpFileTime1, const FILETIME* lpFileTime2);
BOOL   FileTimeToSystemTime( const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime);
BOOL   LocalFileTimeToFileTime( const FILETIME* lpLocalFileTime, LPFILETIME lpFileTime);
VOID   GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);

BOOL  FileTimeToTimeT(const FILETIME* lpLocalFileTime, time_t *pTimeT);
BOOL  TimeTToFileTime(time_t timeT, FILETIME* lpLocalFileTime);

// Time
DWORD timeGetTime(VOID);


#endif

