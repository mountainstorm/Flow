//
//  Flow.c
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

#include "Flow.h"
#include "Log.h"
#include "TraceLog.h"

#include <mach-o/dyld_images.h>




/*
 * Static function predefinitions
 */
static void getAllImageInfos32(Flow* self, VMAddr* dyldImageLoadAddress);
static void getAllImageInfos64(Flow* self, VMAddr* dyldImageLoadAddress);




/*
 * Exported function implementations
 */
kern_return_t Flow_create(Flow* self, task_t task, const char* traceFilename) {
	kern_return_t retVal = Task_createWithTask(&self->task, task);
	if (retVal == KERN_SUCCESS) {
		self->dyldNotificationFunc = 0;
		self->dyldAddrLogged = false;

		bzero(&self->dyldInfoData, sizeof(self->dyldInfoData));
		retVal = Task_getDyldAllImageInfosAddr(&self->task, &self->dyldInfoData);
		if (retVal == KERN_SUCCESS) {
			if (self->dyldInfoData.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_32) {
				self->getAllImageInfos = getAllImageInfos32;
				
			} else if (self->dyldInfoData.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_64) {
				self->getAllImageInfos = getAllImageInfos64;
				
			}
			
			gettimeofday(&self->start, NULL);
			
			// create a trace log for this task
			if (TraceLog_open(&self->traceLog, &self->task, traceFilename) == false) {
				retVal = KERN_FAILURE;
			}
			
			// TODO: set single step on all threads so that on first exception (on attach) we look for next branch
		}
	}
	return retVal;
}


void Flow_release(Flow* self) {
	if (self) {
		Task_release(&self->task);	
		TraceLog_close(&self->traceLog);
	}
}


ExceptionAction Flow_onException(Flow* self, Exception* exception) {
	ExceptionAction retVal = eExceptionAction_abortTask;
	/*
	printf("Exception: %s(%d)\n", Exception_name(exception), exception->type);
	for (int i = 0; i < exception->count; i++) {
		printf("%x ", exception->data[i]);
	}
	printf("\n");
	*/
	
	// if the dyld notification function hasn't been set yet; check again
	if (self->dyldNotificationFunc == 0) {
		// we've not hooked the notification handler yet; so check if its been set yet
		VMAddr dyldImageLoadAddress = 0;
		self->getAllImageInfos(self, &dyldImageLoadAddress);

		// the first time we get a valid dyldImageLoadAddress; log it
		if (self->dyldAddrLogged == false && dyldImageLoadAddress != 0) {
			(void) TraceLog_dyldLoadAddress(&self->traceLog, dyldImageLoadAddress);
			self->dyldAddrLogged = true;
		}
	}
	
	Thread thread = {0};
	Thread_initialize(&thread, &self->task, exception->thread, exception->state);

	VMAddr pc = Thread_getPC(&thread);
	if (pc == self->dyldNotificationFunc) {

			// we've got the params, now log out the content
		if (TraceLog_libraryNotification(&self->traceLog, &thread) == false) {
			return eExceptionAction_abortTask; // bad error
		}
			
		// calculate our performance
		struct timeval end = {0};
		gettimeofday(&end, NULL);
		double x_ms = (double)self->start.tv_sec + (double)self->start.tv_usec/1000000;
		double y_ms = (double)end.tv_sec + (double)end.tv_usec/1000000;
		double diff = (double)y_ms - (double)x_ms;
		printf("t %f\n", diff);
		self->start.tv_sec = end.tv_sec;
		self->start.tv_usec = end.tv_usec;
	}

	/*
	 * This deals with branch stepping.  
	 *
	 * If we're in single step mode we search forward from pc until we find a branch instruction.  
	 * We then disable single step mode, setup a hw breakpoint on the branch instruction and log 
	 * out the block between pc and the branch.  The process will then run until the branch.
	 *
	 * If we're not in single step mode we've just hit a branch instruction.  Thus, we set single
	 * step and disable the hw breakpoint.  We then perform a step (over the branch) and repeat
	 * (running the signle step logic above).
	 *
	 * What this means is that after a single step pc rests at the first instruction of a new block, 
	 * we search forward until we find a branch (which ends the block) and log it.  We run the the 
	 * block at native speed and when its complete single step to find where the branch took us.
	 */
	bool singleStep = false;
	if (Thread_getSingleStep(&thread, &singleStep) == KERN_SUCCESS) {
		if (singleStep) {
			Block block = {0};
			if (	(Thread_findNextBranch(&thread, &block) == KERN_SUCCESS)
				 && (Thread_setSingleStep(&thread, false) == KERN_SUCCESS)
				 && (Thread_setBreakpoint(&thread, block.branch) == KERN_SUCCESS)) {
				
				// log block record
				if (TraceLog_block(&self->traceLog, &block)) {
					retVal = eExceptionAction_continue;
				}
			}
		} else {
			if (	(Thread_setSingleStep(&thread, true) == KERN_SUCCESS)
				 && (Thread_clearBreakpoint(&thread) == KERN_SUCCESS)) {
				retVal = eExceptionAction_continue;		
				
			}
		}
	}
	return retVal;
}




/*
 * Static function implementations
 */


/*
 * architecture independent versions of struct dyld_all_image_infos
 * (basically pointers are uitn32_t or uint64_t) we only include 
 * the fields we need as we're lazy
 */
typedef struct sDyldAllImageInfos32 {
	uint32_t	version;			/* 1 in Mac OS X 10.4 and 10.5 */
	uint32_t	infoArrayCount;
	uint32_t	infoArray;
	uint32_t	notification;
	bool		processDetachedFromSharedRegion;
	/* the following fields are only in version 2 (Mac OS X 10.6, iPhoneOS 2.0) and later */
	bool		libSystemInitialized;
	uint32_t	dyldImageLoadAddress;
} DyldAllImageInfos32;


typedef struct sDyldAllImageInfos64 {
	uint32_t	version;			/* 1 in Mac OS X 10.4 and 10.5 */
	uint32_t	infoArrayCount;
	uint64_t	infoArray;
	uint64_t	notification;	
	bool		processDetachedFromSharedRegion;
	/* the following fields are only in version 2 (Mac OS X 10.6, iPhoneOS 2.0) and later */
	bool		libSystemInitialized;
	uint64_t	dyldImageLoadAddress;
} DyldAllImageInfos64;




static void getAllImageInfos32(Flow* self, VMAddr* dyldImageLoadAddress) {
	DyldAllImageInfos32 allInfos32 = {0};
	kern_return_t ret = Task_readMemory(&self->task, 
										self->dyldInfoData.all_image_info_addr, 
										&allInfos32, 
										sizeof(DyldAllImageInfos32));
	if (ret == KERN_SUCCESS) {
		self->dyldNotificationFunc = allInfos32.notification;
		*dyldImageLoadAddress = allInfos32.dyldImageLoadAddress;
	}
}


static void getAllImageInfos64(Flow* self, VMAddr* dyldImageLoadAddress) {
	DyldAllImageInfos64 allInfos64 = {0};
	kern_return_t ret = Task_readMemory(&self->task, 
										self->dyldInfoData.all_image_info_addr, 
										&allInfos64, 
										sizeof(DyldAllImageInfos64));
	if (ret == KERN_SUCCESS) {
		self->dyldNotificationFunc = allInfos64.notification;
		*dyldImageLoadAddress = allInfos64.dyldImageLoadAddress;
	}
}

