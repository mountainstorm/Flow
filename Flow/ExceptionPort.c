//
//  ExceptionPort.c
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
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ptrace.h>

#include "ExceptionPort.h"
#include "Exception.h"

#include "Log.h"




/*
 * Defines 
 */
#define kExceptionMsgSize	(4096)



/*
 * Imports 
 */
// you need to create mach_excServer.c which implements this - see here for the best documentation around
// http://stackoverflow.com/questions/2824105/handling-mach-exceptions-in-64bit-os-x-application
boolean_t mach_exc_server(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);




/*
 * Static function predefinitions
 */
static kern_return_t exceptionPort_attach(ExceptionPort* self, 
										  task_t task, 
										  ExceptionPort_onException* onException, 
										  void* ctx);
static kern_return_t exceptionPort_detach(ExceptionPort* self);
static kern_return_t exceptionPort_process(ExceptionPort* self);

// these 3 functions are required by mach_exc_server; which one is called depends on the params
// passed to task_set_exception_ports's behaviour parameter.  As a result they can't be static
kern_return_t catch_mach_exception_raise(mach_port_t exception_port, 
										 mach_port_t thread,
										 mach_port_t task,
										 exception_type_t exception,
										 mach_exception_data_t code,
										 mach_msg_type_number_t codeCnt);

kern_return_t catch_mach_exception_raise_state(mach_port_t exception_port,
											   exception_type_t exception,
											   const mach_exception_data_t code,
											   mach_msg_type_number_t codeCnt,
											   int* flavor,
											   const thread_state_t old_state,
											   mach_msg_type_number_t old_stateCnt,
											   thread_state_t new_state,
											   mach_msg_type_number_t* new_stateCnt);

kern_return_t catch_mach_exception_raise_state_identity(mach_port_t exception_port,
														mach_port_t thread,
														mach_port_t task,
														exception_type_t exception,
														mach_exception_data_t code,
														mach_msg_type_number_t codeCnt,
														int* flavor,
														thread_state_t old_state,
														mach_msg_type_number_t old_stateCnt,
														thread_state_t new_state,
														mach_msg_type_number_t* new_stateCnt);




/*
 * Global variables
 */
// this should be a list if we want to handle multiple tasks
ExceptionPort* gExceptionPort = NULL;




/*
 * Exported function implementations
 */
kern_return_t ExceptionPort_attachToTask(ExceptionPort* self, 
										 task_t task, 
										 ExceptionPort_onException* onException, 
										 void* ctx) {
	kern_return_t retVal = KERN_INVALID_VALUE;
	if (gExceptionPort != NULL || self == NULL || task == TASK_NULL) {
		Log_invalidArgument("gExceptionPort: %p, self: %p, task: %p", 
							gExceptionPort, 
							self, 
							(void*) task);
		
	} else {
		retVal = exceptionPort_attach(self, task, onException, ctx);
		if (retVal == KERN_SUCCESS) {
			// add this port into the list to process
			gExceptionPort = self;
		}
	}
	return retVal;
}


kern_return_t ExceptionPort_detachFromTask(ExceptionPort* self) {
	kern_return_t retVal = KERN_INVALID_VALUE;
	if (self == NULL || self != gExceptionPort) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = exceptionPort_detach(self);
	}
	return retVal;
}


kern_return_t ExceptionPort_process(ExceptionPort* self) {
	kern_return_t retVal = KERN_INVALID_VALUE;
	if (self == NULL || self != gExceptionPort) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = exceptionPort_process(gExceptionPort);
	}
	return retVal;
}




/*
 * Static function implementations
 */
static kern_return_t exceptionPort_attach(ExceptionPort* self, 
										  task_t task, 
										  ExceptionPort_onException* onException, 
										  void* ctx) {
	kern_return_t retVal = mach_port_allocate(mach_task_self(), 
											  MACH_PORT_RIGHT_RECEIVE, 
											  &self->exceptionPort);
	if (retVal != KERN_SUCCESS) {
		Log_errorMach(retVal, "mach_port_allocate");
		
	} else {
		retVal = mach_port_insert_right(mach_task_self(), 
										self->exceptionPort, 
										self->exceptionPort, 
										MACH_MSG_TYPE_MAKE_SEND);
		if (retVal != KERN_SUCCESS) {
			Log_errorMach(retVal, "mach_port_insert_right");
			
		} else {
			exception_mask_t mask =   EXC_MASK_SOFTWARE
									| EXC_MASK_BREAKPOINT;
			// get the old exception port to allow us to restore it
			self->originalExceptionPort.count = (  sizeof(self->originalExceptionPort.port)
												 / sizeof(self->originalExceptionPort.port[0]));
			retVal = task_get_exception_ports(task,
											  mask,
											  self->originalExceptionPort.mask,
											  &self->originalExceptionPort.count,
											  self->originalExceptionPort.port,
											  self->originalExceptionPort.behavior,
											  self->originalExceptionPort.flavor);
			if (retVal != KERN_SUCCESS) {
				Log_errorMach(retVal, "task_get_exception_ports");
				
			} else {
				retVal = task_set_exception_ports(task, 
												  mask, 
												  self->exceptionPort, 
													EXCEPTION_STATE_IDENTITY 
												  | MACH_EXCEPTION_CODES, 
												  MACHINE_THREAD_STATE); 
				if (retVal != KERN_SUCCESS) {
					Log_errorMach(retVal, "task_set_exception_ports");
					
				} else {
					Exception_create(&self->currentException);
					
					self->task = task; 
					self->onException = onException;
					self->onExceptionCtx = ctx;
					pid_for_task(self->task, &self->pid);
				}
			}
		}
		
		if (retVal != KERN_SUCCESS) {
			exceptionPort_detach(self);
		}
	}
	return retVal;
}


static kern_return_t exceptionPort_detach(ExceptionPort* self) {
	kern_return_t retVal = KERN_SUCCESS;

	if (self->exceptionPort == MACH_PORT_NULL) {
		retVal = mach_port_deallocate(mach_task_self(), self->exceptionPort);
		self->exceptionPort = MACH_PORT_NULL;
	}
	
	// restore the original report
	for (uint32_t i = 0; i > self->originalExceptionPort.count; i++) {
		retVal = task_set_exception_ports(self->task, 
										  self->originalExceptionPort.mask[i], 
										  self->originalExceptionPort.port[i], 
										  self->originalExceptionPort.behavior[i], 
										  self->originalExceptionPort.flavor[i]);
		if (retVal != KERN_SUCCESS) {
			Log_errorMach(retVal, "task_set_exception_ports(%u)", i);
			break;
		}
	}
	
	Exception_release(&self->currentException);
	return retVal;
}


static kern_return_t exceptionPort_process(ExceptionPort* self) {
	kern_return_t retVal = KERN_INVALID_VALUE;
	
	while (1) {	
		char data[kExceptionMsgSize + sizeof(mach_msg_header_t)] = {0};
		mach_msg_header_t* hdr = (mach_msg_header_t*) data;
			
		retVal = mach_msg(hdr, 
						  MACH_RCV_MSG | MACH_RCV_LARGE, 
						  0, 
						  sizeof(data), 
						  self->exceptionPort, 
						  MACH_MSG_TIMEOUT_NONE, 
						  MACH_PORT_NULL);
		if (retVal != KERN_SUCCESS) {
			Log_errorMach(retVal, "mach_msg");
			break;
			
		}

		char replyData[kExceptionMsgSize + sizeof(mach_msg_header_t)] = {0};
		mach_msg_header_t* replyHdr = (mach_msg_header_t*) replyData;
		if (!mach_exc_server(hdr, replyHdr)) {
			Log_error("exc_server\n");
			retVal = KERN_FAILURE;
			break;
			
		}
		
		// send a reply to prod
		retVal = mach_msg(replyHdr,
						  MACH_SEND_MSG | MACH_RCV_LARGE,
						  replyHdr->msgh_size,
						  0,
						  MACH_PORT_NULL,
						  MACH_MSG_TIMEOUT_NONE,
						  MACH_PORT_NULL);
		if (retVal != KERN_SUCCESS) {
			Log_errorMach(retVal, "mach_send");
			break;
			
		}
	}	
	return retVal;
}




/*
 * Some stats from my Core i7 27" iMac.
 *
 * using catch_mach_exception_raise gives about 250,000 instructions per second
 *  but, if you reenter the kernel by calling thread_get_state(MACHINE_THREAD_STATE)
 *  (which we need to do to get the pc/eip value), it plummets and we only get around
 *  80,000 instructions per second.
 *
 * using catch_mach_exception_raise_state_identity we get supplied the MACHINE_THREAD_STATE
 *  obviously this prevents the need to reenter the kernel each instruction.  As such we're 
 *  rewarded with around 170,000 instructions per second; slow but actually usable.
 */
kern_return_t catch_mach_exception_raise(mach_port_t exception_port, 
										 mach_port_t thread,
										 mach_port_t task,
										 exception_type_t exception,
										 mach_exception_data_t code,
										 mach_msg_type_number_t codeCnt) {
	assert(0); // should never be called
    return KERN_FAILURE;
}


kern_return_t catch_mach_exception_raise_state(mach_port_t exception_port,
											   exception_type_t exception,
											   const mach_exception_data_t code,
											   mach_msg_type_number_t codeCnt,
											   int* flavor,
											   const thread_state_t old_state,
											   mach_msg_type_number_t old_stateCnt,
											   thread_state_t new_state,
											   mach_msg_type_number_t* new_stateCnt) {
	assert(0); // should never be called
    return KERN_FAILURE;
}


kern_return_t catch_mach_exception_raise_state_identity(mach_port_t exception_port,
														mach_port_t thread,
														mach_port_t task,
														exception_type_t exception,
														mach_exception_data_t code,
														mach_msg_type_number_t codeCnt,
														int* flavor,
														thread_state_t old_state,
														mach_msg_type_number_t old_stateCnt,
														thread_state_t new_state,
														mach_msg_type_number_t* new_stateCnt) {
	kern_return_t retVal = KERN_SUCCESS;
	if (task == gExceptionPort->task) {
		retVal = Exception_assign(&gExceptionPort->currentException, 
								  task, 
								  thread, 
								  old_state, 
								  exception, 
								  code, 
								  codeCnt);

		*new_stateCnt = old_stateCnt;

		// we dont need to suspend/resume the task as we're happy with only the excepting thread being suspended
		ExceptionAction action = gExceptionPort->onException(gExceptionPort->onExceptionCtx, 
															 &gExceptionPort->currentException);
#if defined(__arm)
		// TODO:
#else
		*((x86_thread_state_t*)new_state) = *((x86_thread_state_t*)old_state);
#endif		

		if (action == eExceptionAction_abortTask) {
			ptrace(PT_KILL, gExceptionPort->pid, 0, 0);
			printf("action abort\n"); fflush(stdout);
			retVal = KERN_FAILURE;
		}

		// TODO: debug server does this, do I need to?
		int softwareSignal = Exception_softwareSignal(&gExceptionPort->currentException);
		if (softwareSignal) {
			int err = ptrace(PT_THUPDATE, 
							 gExceptionPort->pid, 
							 (caddr_t) gExceptionPort->currentException.thread, 
							 softwareSignal);
			if (err == -1) {
				printf("PT_THUPDATE\n"); fflush(stdout);
				Log_errorPosix(err, "ptrace PT_THUPDATE");
				retVal = KERN_FAILURE;
			}
		}
 }
	return retVal;
}
