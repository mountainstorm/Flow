//
//  Launch.c
//  Flow
//
//  Created by R J Cooper on 05/06/2012.
//  Copyright (c) 2012 Mountainstorm
//  
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

#include <stdio.h>
#include <spawn.h>
#include <signal.h>

#include "Launch.h"
#include "Log.h"




/*
 * Defines
 */
#ifndef _POSIX_SPAWN_DISABLE_ASLR
#  define _POSIX_SPAWN_DISABLE_ASLR       (0x0100)
#endif




/*
 * Static function implementations
 */
pid_t Launch_posixSpawnSuspended(cpu_type_t cpuType, const char *path, char** argv) {
	pid_t retVal = -1;
	
	if (path == NULL || argv == NULL) {
		Log_invalidArgument("path: %p, argv: %p", path, argv);
		
	} else {
		posix_spawnattr_t attr = 0;
		int ret = posix_spawnattr_init(&attr);
		if (ret != 0) {
			Log_errorPosix(ret, "posix_spawnattr_init");
			
		} else {
			sigset_t no_signals = 0;
			sigset_t all_signals = 0;
			sigemptyset(&no_signals);
			sigfillset(&all_signals);
			posix_spawnattr_setsigmask(&attr, &no_signals);
			posix_spawnattr_setsigdefault(&attr, &all_signals);

			if (cpuType != CPU_TYPE_ANY) {
				size_t ocount = 0;
				// if specified choose the arch from the fat binary to run
				ret = posix_spawnattr_setbinpref_np(&attr, 1, &cpuType, &ocount);
				if (ret != 0) {
					Log_errorPosix(ret, "posix_spawnattr_setbinpref_np");
				}
			}
			
			if (ret == 0) {
				ret = posix_spawnattr_setflags(&attr, 
											     POSIX_SPAWN_START_SUSPENDED 
											   | _POSIX_SPAWN_DISABLE_ASLR 
											   | POSIX_SPAWN_SETSIGDEF 
											   | POSIX_SPAWN_SETSIGMASK);
				if (ret != 0) {
					Log_errorPosix(ret, "posix_spawnattr_setflags");
					
				} else {
					pid_t pid = -1;
					ret = posix_spawnp(&pid, 
									   path, 
									   NULL, 
									   &attr, 
									   (char * const*)argv, 
									   (char * const*)NULL);
					if (ret != 0) {
						Log_errorPosix(ret, "posix_spawnp");
						
					} else {
						retVal = pid;
					}
				}
			}
			posix_spawnattr_destroy(&attr);
		}
	}
	return retVal;
}


pid_t Launch_springboardSuspended(const char *path, char** argv) {
	pid_t retVal = -1;
	
	if (path == NULL || argv == NULL) {
		Log_invalidArgument("path: %p, argv: %p", path, argv);
		
	} else {
		// TODO: springboard launch
	}
	return retVal;
}