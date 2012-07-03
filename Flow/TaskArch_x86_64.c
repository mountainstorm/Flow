//
//  TaskArch_x86_64.c
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

#include <stdio.h>
#include <sys/types.h>
#include <mach/mach.h>

#include <distorm.h>

#include "TaskArch_x86_64.h"
#include "Log.h"




/*
 * Defines
 */
#define kTraceBit			(0x100u)
#define kBranchReadAhead	(4096)




/*
 * Structure definition
 */
typedef struct sTaskArch_x86 {
	TaskArch			arch;
	x86_debug_state64_t	debug;
} TaskArch_x86_64;




/*
 * Static function predefinitions
 */
static void release(Task* self);

static VMAddr getPC(Thread* self);

static kern_return_t setSingleStep(Thread* self, bool enable);
static kern_return_t getSingleStep(Thread* self, bool* enable);
static kern_return_t setBreakpoint(Thread* self, VMAddr pc);
static kern_return_t clearBreakpoint(Thread* self);

static kern_return_t findNextBranch(Thread* self, Block* block);


static void argsInitialize(FunctionArgs* self, Thread* thread, bool stackCookie);
static kern_return_t argsGet(FunctionArgs* self, uint64_t size, void* value);





/*
 * Exported function implementations
 */
TaskArch* TaskArch_x86_64_create(void) {
	static TaskArch_x86_64 singleton = {0};
	singleton.arch.release = release;
	
	singleton.arch.getPC = getPC;
	
	singleton.arch.setSingleStep = setSingleStep;
	singleton.arch.getSingleStep = getSingleStep;
	singleton.arch.setBreakpoint = setBreakpoint;
	singleton.arch.clearBreakpoint = clearBreakpoint;
	
	singleton.arch.findNextBranch = findNextBranch;
	
	singleton.arch.argsInitialize = argsInitialize;
	singleton.arch.argsGet = argsGet;
	
	(void) memset(&singleton.debug, 0x00, sizeof(singleton.debug));
	return (TaskArch*) &singleton;
}




/*
 * Static function implementations
 */
static void release(Task* self) {
	// do nothing; we returned a singleton with no state
}


static VMAddr getPC(Thread* self) {
	return ((x86_thread_state_t*) self->state)->uts.ts64.__rip;		
}


static kern_return_t setSingleStep(Thread* self, bool enable) {
	if (enable) {
		((x86_thread_state_t*) self->state)->uts.ts64.__rflags |= kTraceBit;
	} else {
		((x86_thread_state_t*) self->state)->uts.ts64.__rflags &= ~kTraceBit;
	}
	return KERN_SUCCESS;
}


static kern_return_t getSingleStep(Thread* self, bool* enable) {
	*enable = ((x86_thread_state_t*) self->state)->uts.ts64.__rflags & kTraceBit;
	return KERN_SUCCESS;
}


static kern_return_t setBreakpoint(Thread* self, VMAddr pc) {
	kern_return_t retVal = KERN_FAILURE;
	x86_debug_state64_t* debug = &((TaskArch_x86_64*) self->task->arch)->debug;
	debug->__dr0 = pc;
	debug->__dr7 |= 0x1; // enable bp 1
	retVal = thread_set_state(self->thread, 
							  x86_DEBUG_STATE64, 
							  (thread_state_t) debug, 
							  X86_DEBUG_STATE64_COUNT);
	if (retVal != KERN_SUCCESS) {
		Log_errorMach(retVal, "thread_set_state");
		
	}
	return retVal;
}


static kern_return_t clearBreakpoint(Thread* self) {
	kern_return_t retVal = KERN_FAILURE;
	x86_debug_state64_t* debug = &((TaskArch_x86_64*) self->task->arch)->debug;
	debug->__dr0 = 0;
	debug->__dr7 &= ~0x1; // disable bp 1
	retVal = thread_set_state(self->thread, 
							  x86_DEBUG_STATE64, 
							  (thread_state_t) debug, 
							  X86_DEBUG_STATE64_COUNT);
	if (retVal != KERN_SUCCESS) {
		Log_errorMach(retVal, "thread_set_state");
		
	}
	return retVal;
}


static kern_return_t findNextBranch(Thread* self, Block* block) {
	kern_return_t retVal = KERN_FAILURE;
	
	// search forward from pc until we find next branch
	char data[kBranchReadAhead] = {0};
	VMAddr pc = getPC(self);
	if (Task_readMemory(self->task, pc, &data, sizeof(data)) == KERN_SUCCESS) {
		_DInst result = {0};
		unsigned int ic = 0;
		
		_CodeInfo ci = {0};
		ci.code = (uint8_t*) data;
		ci.codeLen = sizeof(data);
		ci.dt = Decode64Bits;
		ci.codeOffset = pc;
		while (1) {
			distorm_decompose64(&ci, &result, 1, &ic);
			if (result.flags == FLAG_NOT_DECODABLE) {
				Log_error("Unable to decode instruction at: %llx, offset: %llx", pc, ci.codeOffset-pc);
				break;
				
			} else {
				int fc = META_GET_FC(result.meta);
				if (fc != FC_NONE){
					block->entry = pc;
					block->branch = ci.codeOffset;
					switch (fc) {
						case FC_CALL:	block->type = eBranchType_call;		break;
						case FC_RET:	block->type = eBranchType_ret;		break;
						case FC_SYS:	block->type = eBranchType_sys;		break;								
						default:		block->type = eBranchType_other;	break;
					}
					retVal = KERN_SUCCESS;
					break;
				}
			}
			ci.code += result.size;
			ci.codeLen -= result.size;
			ci.codeOffset += result.size;
		}
	}
	return retVal;
}



static void argsInitialize(FunctionArgs* self, Thread* thread, bool stackCookie) {
	self->thread = thread;
	*((x86_thread_state_t*)&self->state) = *((x86_thread_state_t*) thread->state);
	
	x86_thread_state_t* state = (x86_thread_state_t*) &self->state;
	state->uts.ts64.__rax = 0; // we'll store the argument idx in rax
	state->uts.ts64.__rsp += sizeof(uint64_t); // skip the return address
	if (stackCookie) {
		state->uts.ts64.__rsp += sizeof(uint64_t); // skip the cookie
	}
}


static kern_return_t argsGet(FunctionArgs* self, uint64_t size, void* value) {
	kern_return_t retVal = KERN_FAILURE;
	
	// TODO: make a more robust version of this i.e. complete it
	x86_thread_state_t* state = (x86_thread_state_t*) &self->state;
	switch (state->uts.ts64.__rax) {
		case 0: 
			(void) memcpy(value, &state->uts.ts64.__rdi, size);
			retVal = KERN_SUCCESS;
			break;
			
		case 1: 
			(void) memcpy(value, &state->uts.ts64.__rsi, size);
			retVal = KERN_SUCCESS;
			break;
			
		case 2: 
			(void) memcpy(value, &state->uts.ts64.__rdx, size);
			retVal = KERN_SUCCESS;
			break;
	}
	
	if (retVal == KERN_SUCCESS) {
		state->uts.ts64.__rax++;
	}
	return retVal;
}

