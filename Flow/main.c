//
//  main.c
//  Flow
//
//  Created by R J Cooper on 03/06/2012.
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
#include <errno.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <Security/Authorization.h>

#include "Log.h"
#include "Launch.h"
#include "ExceptionPort.h"
#include "Task.h"
#include "Flow.h"




/*
 * Structure definitions
 */
typedef enum eLaunchStyle {
	eLaunchStyle_posixSpawn,
	eLaunchStyle_springboard,
	eLaunchStyle_attach	
} eLaunchStyle;


typedef struct sOptions {
	pid_t			pid;			// pid to attach too if eLaunchStyle_attach
	cpu_type_t		cpuType;		// fat binary img to run; CPU_TYPE_ANY means default
	char*			traceFilename;	// if not NULL, the output trace log filename
	eLaunchStyle	launchStyle;
} Options;




/*
 * Static function predefinitions
 */
static bool processTaskExceptions(pid_t pid, task_t task, const char* traceFilename, Options* options);

static int parseOptions(Options* options, int argc, char* argv[]);
static cpu_type_t parseCpuType(char* cpuTypeStr);
static void usage(void);
static bool acquireTaskportRight(void);




/*
 * TODO:
 *  1. ARM support
 *  2. Springboard launch
 */
int main (int argc, char* argv[])
{
	Options options = {0};
	int optarg = parseOptions(&options, argc, argv);
	
	// create or attach to the process
	pid_t pid = -1;
	if (acquireTaskportRight()) {
		if (options.launchStyle == eLaunchStyle_attach) {
			pid = options.pid;
			
		} else {
			if (options.launchStyle == eLaunchStyle_posixSpawn) {
				pid = Launch_posixSpawnSuspended(options.cpuType, argv[optarg], &argv[optarg]);
				
			} else {
				pid = Launch_springboardSuspended(argv[optarg], &argv[optarg]);
				
			}
		}
		
		if (pid != -1) {
			bool attached = false;
			printf("pid: %d\n", pid);

			// get the task for this pid and suspend it (in case we attached and it was running)
			task_t task = 0;
			kern_return_t ret = task_for_pid(mach_task_self(), pid, &task);	
			if (ret != KERN_SUCCESS) {
				Log_errorMach(ret, "task_for_pid");
				
			} else {
				ret = task_suspend(task);
				if (ret != KERN_SUCCESS) {
					Log_errorMach(ret, "task_suspend");
					
				} else {
					char defaultTraceFilename[PATH_MAX] = {0};
					(void) snprintf(defaultTraceFilename, 
									sizeof(defaultTraceFilename), 
									"Flow_%u.log", 
									pid);

					char* traceFilename = defaultTraceFilename;
					if (options.traceFilename) {
						traceFilename = options.traceFilename;
					}
					attached = processTaskExceptions(pid, task, traceFilename, &options);
					
				}
				(void) task_resume(task); // ensure when we detach it will resume
			}
			if (attached == false) {
				(void) ptrace(PT_ATTACHEXC, pid, 0, 0);
			}
			(void) ptrace(PT_KILL, pid, 0, 0); // kill the process we attached to
			(void) ptrace(PT_DETACH, pid, 0, 0);
			
		}
	}
    return 0;
}


static bool processTaskExceptions(pid_t pid, task_t task, const char* traceFilename, Options* options) {
	bool retVal = false;
	
	Flow flow = {0};
	if (Flow_create(&flow, task, traceFilename) == KERN_SUCCESS) {
		ExceptionPort exceptionPort = {0};
		kern_return_t ret = ExceptionPort_attachToTask(&exceptionPort, 
													   task,
													   (ExceptionPort_onException*) Flow_onException,
													   &flow);
		if (ret == KERN_SUCCESS) {
			/*
			 * we must have attached to the tasks exception port before ptrace attaching, 
			 * otherwise when we start the process the SIGCRASH it sends isn't handled by the 
			 * mach exception mechanism and just gets translated into a BSD signal.  If this
			 * happens, waitpid returns SIGCRASH and we can no longer fix it ... thus the
			 * target process dies :(
			 */
			int err = ptrace(PT_ATTACHEXC, pid, 0, 0);
			if (err != 0) {
				Log_errorPosix(err, "ptrace(ATTACHEXC)");
				
			} else {
				retVal = true;
				pthread_t exceptionThread = NULL;
				int err = pthread_create(&exceptionThread, 
										 NULL, 
										 (void*(*)(void*))ExceptionPort_process, 
										 &exceptionPort);
				if (err != 0) {
					Log_errorPosix(errno, "pthread_create");
					
				} else {
					// resume task and process BSD signals
					ret = task_resume(task);
					if (ret != KERN_SUCCESS) {
						Log_errorMach(ret, "task_resume");
						
					} else {
						int status = 0;					
						while (1) {
							int p = waitpid(pid, &status, 0);
							if (p < 0) {
								break;
							} else {
								if (!WIFSTOPPED(status)) {
									break;
								}
							}
						}
					}
					(void) pthread_kill(exceptionThread, SIGUSR1); // cancel the thread
					(void) pthread_join(exceptionThread, NULL);
				}
				
			}
			(void) ExceptionPort_detachFromTask(&exceptionPort);
			Flow_release(&flow);
		}
	}
	return retVal;
}


static int parseOptions(Options* options, int argc, char* argv[]) {
	options->cpuType = CPU_TYPE_ANY;
	options->pid = -1;
	options->launchStyle = eLaunchStyle_posixSpawn;
	
	int c = -1;
	while ((c = getopt(argc, argv, "sc:a:o:")) != -1) {
		switch (c) {
			case 's':
				options->launchStyle = eLaunchStyle_springboard;
				break;
				
			case 'c':
				options->cpuType = parseCpuType(optarg);
				break;
				
			case 'a':
				options->launchStyle = eLaunchStyle_attach;
				options->pid = atol(optarg);
				break;
				
			case 'o':
				options->traceFilename = optarg;
				break;
				
			default:
				usage();
				break;
		}
	}
		
	// check for invalid options
	if (	(options->launchStyle == eLaunchStyle_attach)
		 && (	 (options->pid == -1)
			  || (options->cpuType != CPU_TYPE_ANY))) {
		usage();
	}
	return optind;
}


static cpu_type_t parseCpuType(char* cpuTypeStr) {
	cpu_type_t retVal = CPU_TYPE_ANY;
	if (strcmp(cpuTypeStr, "i386") == 0) {
		retVal = CPU_TYPE_I386;		
	} else if (strcmp(cpuTypeStr, "x86_64") == 0) {
		retVal = CPU_TYPE_X86_64;
	}
	return retVal;
}


static void usage(void) {
	printf("Usage: flow [-o tracefile] -a pid | [-se] [-c i386|x86_64] prog args\n");
	printf("    -o: the name of the tracefile\n"); 
	printf("    -a: attach to pid\n");
	printf("    -s: launch using springboard\n");
	printf("    -e: log from entrypoint; rather than process start (in dyld)\n");
	printf("    -c: launch this arch from a fat binary\n");
	exit(-1);
}


// code taken from: http://os-tres.net/blog/2010/02/17/mac-os-x-and-task-for-pid-mach-call/
// note you'll need to create your cert like this: 
// https://llvm.org/svn/llvm-project/lldb/trunk/docs/code-signing.txt
static bool acquireTaskportRight(void) {
	bool retVal = false;
	
	AuthorizationFlags auth_flags =   kAuthorizationFlagExtendRights 
									| kAuthorizationFlagPreAuthorize
									| kAuthorizationFlagInteractionAllowed 
									| ( 1 << 5); // ???

	AuthorizationRef author = NULL;
	OSStatus stat = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, auth_flags, &author);
	if (stat != errAuthorizationSuccess) {
		Log_error("unable create authorization object: %u", stat);
		
	} else {
		AuthorizationItem taskport_item[] = {{"system.privilege.taskport"}};
		AuthorizationRights rights = {1, taskport_item};
		AuthorizationRights* out_rights = NULL;
		
		stat = AuthorizationCopyRights(author, 
									   &rights, 
									   kAuthorizationEmptyEnvironment, 
									   auth_flags, 
									   &out_rights);
		if (stat != errAuthorizationSuccess) {
			Log_error("unable to aquire system.privilege.taskport: %u", stat);

		} else {
			retVal = true;
		}
	}
	return retVal;
}

