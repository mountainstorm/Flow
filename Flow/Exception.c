//
//  Exception.c
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
#include <stdlib.h>

#include "Log.h"
#include "Exception.h"




/*
 * Exported function implementations
 */
void Exception_create(Exception* self) {
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		self->task = TASK_NULL;
		self->thread = THREAD_NULL;
		self->state = NULL;
		self->type = 0;
		self->count = 0;
		self->maxCount = 0;
		self->data = NULL;
	}
}


void Exception_release(Exception* self) {
	if (self->data) {
		(void) free(self->data);
		self->data = NULL;
	}
}


kern_return_t Exception_assign(Exception* self, 
							   mach_port_t task,
							   mach_port_t thread,
							   thread_state_t state,
							   exception_type_t type,
							   mach_exception_data_t code,
							   mach_msg_type_number_t codeCnt) {
	kern_return_t retVal = KERN_FAILURE;
	
	if (	(self == NULL)
		 || (task == TASK_NULL)
		 || (thread == THREAD_NULL)
		 || (state == NULL)) { 
		Log_invalidArgument("self: %p, task: %p, thread: %p, state: %p", 
							self,
							(void*) task,
							(void*) thread,
							state);
	
	} else {
		self->task = task;
		self->thread = thread;
		self->state = state;
		self->type = type;
		if (codeCnt > self->maxCount) {
			// resize data
			self->data = reallocf(self->data, codeCnt * sizeof(mach_exception_data_t));
			if (self->data) {
				self->maxCount = codeCnt;
			}
		}
		
		if (self->data == NULL) {
			Log_error("unable to allcoate memory");
			
		} else {
			self->count = codeCnt;
			(void) memcpy(self->data, code, self->count);
			retVal = KERN_SUCCESS;
		}
	}
	return retVal;
}


int Exception_softwareSignal(Exception* self) {
	int retVal = 0;
	
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
							
	} else {
		if (self->type == EXC_SOFTWARE && self->count == 2 && self->data[0] == EXC_SOFT_SIGNAL) {
			retVal = (int) self->data[1];
		}
	}
	return retVal;
}


const char* Exception_name(Exception* self) {
	const char* retVal = "<unknown>";
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		switch(self->type) {
			case EXC_BAD_ACCESS:        retVal = "EXC_BAD_ACCESS";
			case EXC_BAD_INSTRUCTION:   retVal = "EXC_BAD_INSTRUCTION";
			case EXC_ARITHMETIC:        retVal = "EXC_ARITHMETIC";
			case EXC_EMULATION:         retVal = "EXC_EMULATION";
			case EXC_SOFTWARE:          retVal = "EXC_SOFTWARE";
			case EXC_BREAKPOINT:        retVal = "EXC_BREAKPOINT";
			case EXC_SYSCALL:           retVal = "EXC_SYSCALL";
			case EXC_MACH_SYSCALL:      retVal = "EXC_MACH_SYSCALL";
			case EXC_RPC_ALERT:         retVal = "EXC_RPC_ALERT";
			case EXC_CRASH:             retVal = "EXC_CRASH";
			default:
				break;
		}
	}
	return retVal;
}
