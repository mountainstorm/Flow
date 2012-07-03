//
//  ExceptionPort.h
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

#ifndef Flow_ExceptionPort_h
#define Flow_ExceptionPort_h


#include <mach/mach.h>

#include "Exception.h"




/*
 * Structre/Type definitions
 */
typedef struct sOriginalExceptionPort {
	exception_mask_t		mask[EXC_TYPES_COUNT];
	mach_port_t				port[EXC_TYPES_COUNT];
	exception_behavior_t	behavior[EXC_TYPES_COUNT];
	thread_state_flavor_t	flavor[EXC_TYPES_COUNT];
	mach_msg_type_number_t	count;
} OriginalExceptionPort;


typedef ExceptionAction (ExceptionPort_onException)(void* ptr, Exception* exception);


typedef struct sExceptionPort {
	pid_t						pid;
	task_t						task;
	mach_port_t					exceptionPort;
	
	Exception					currentException;
	ExceptionPort_onException*	onException;
	void*						onExceptionCtx;
	
	OriginalExceptionPort		originalExceptionPort;
} ExceptionPort;




/*
 * Exported function definitions
 */
kern_return_t ExceptionPort_attachToTask(ExceptionPort* self, 
										 task_t task, 
										 ExceptionPort_onException* onException, 
										 void* ctx);
kern_return_t ExceptionPort_detachFromTask(ExceptionPort* self);

kern_return_t ExceptionPort_process(ExceptionPort* self);


#endif
