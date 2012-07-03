//
//  Flow.h
//  Flow
//
//  Created by R J Cooper on 06/06/2012.
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

#ifndef Flow_Flow_h
#define Flow_Flow_h


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <mach/mach.h>
#include <stdbool.h>
#include <sys/time.h>

#include "Task.h"
#include "Exception.h"
#include "TraceLog.h"




/*
 * Structure/Type definitions
 */
typedef struct sFlow Flow;


typedef void (GetAllImageInfos)(Flow* self, VMAddr* dyldImageLoadAddress);


struct sFlow {
	Task						task;
	
	TraceLog					traceLog;

	VMAddr						dyldNotificationFunc;
	bool						dyldAddrLogged;
	
	task_dyld_info_data_t		dyldInfoData;
	GetAllImageInfos*			getAllImageInfos;

	struct timeval				start;
};




/*
 * Exported function definitions
 */
kern_return_t Flow_create(Flow* self, task_t task, const char* traceFilename);
void Flow_release(Flow* self);
ExceptionAction Flow_onException(Flow* self, Exception* exception);


#endif
