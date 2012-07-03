//
//  Task.c
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <stdbool.h>
#include <errno.h>
#include <mach/vm_map.h>

#include "Task.h"
#include "Log.h"

#include "TaskArch_x86.h"
#include "TaskArch_x86_64.h"




/*
 * Static function predefinitions
 */
static cpu_type_t getCPUTypeForTask(pid_t pid);
static uint64_t getWordSize(pid_t pid);




/*
 * Exported function implementations
 */
kern_return_t Task_createWithTask(Task* self, task_t task) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL || task == TASK_NULL) {
		Log_invalidArgument("self: %p, task: %p", self, (void*) task);
		
	} else {
		pid_t pid = -1;
		(void) pid_for_task(task, &pid);	

		cpu_type_t cpuType = getCPUTypeForTask(pid);
		if (cpuType == CPU_TYPE_ARM) {
			printf("ARM; NOT IMPLEMENTED\n");
			//self->arch = TaskArch_ARM_create();
			
		} else if (cpuType == CPU_TYPE_I386 || cpuType == CPU_TYPE_X86) {
			self->arch = TaskArch_x86_create();
			
		} else if (cpuType == CPU_TYPE_X86_64) {
			self->arch = TaskArch_x86_64_create();
			
		} else {
			Log_error("Unsupported process architecture, %d", cpuType);
			retVal = KERN_FAILURE;
		}
		
		if (self->arch) {
			self->task = task;
			self->pid = pid;
			self->cpuType = cpuType;
			self->wordSize = getWordSize(pid);
			retVal = KERN_SUCCESS;
		}
	}
	return retVal;
}


cpu_type_t Task_getCpuType(Task* self) {
	cpu_type_t retVal = CPU_TYPE_ANY;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->cpuType;
	}
	return retVal;
}


pid_t Task_getPid(Task* self) {
	pid_t retVal = -1;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->pid;
	}
	return retVal;
}


uint64_t Task_getWordSize(Task* self) {
	uint64_t retVal = 0;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->wordSize;
	}
	return retVal;
}


kern_return_t Task_getDyldAllImageInfosAddr(Task* self, task_dyld_info_data_t* info) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL || info == NULL) {
		Log_invalidArgument("self: %p, info: %p", self, info);
		
	} else {
		mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
		retVal = task_info(self->task, TASK_DYLD_INFO, (task_info_t) info, &count);
		if (retVal != KERN_SUCCESS) {
			Log_errorMach(retVal, "retieving dyld info");
		}
	}
	return retVal;
}


kern_return_t Task_readMemory(Task* self, VMAddr addr, void* data, vm_size_t length) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL || data == NULL) {
		Log_invalidArgument("self: %p, data: %p", self, data);
		
	} else {
		vm_size_t count = length;
		retVal = vm_read_overwrite(self->task, addr, length, (vm_address_t) data, &count);
		if (retVal != KERN_SUCCESS || length != count) {
			Log_errorMach(retVal, "vm_read_overwrite");
			
		}
	}
	return retVal;
}


kern_return_t Task_readString(Task* self, VMAddr addr, char* path, uint64_t size) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL || path == NULL) {
		Log_invalidArgument("self: %p, path: %p", self, path);
		
	} else {
		uint64_t i = 0;
		
		// Note: this is terifyingly inefficient; but works
		for (i = 0; i < size; i++) {
			retVal = Task_readMemory(self, addr+i, (void*) &path[i], 1);
			if (retVal != KERN_SUCCESS) {
				break;
			}
			
			if (path[i] == '\0') {
				break; // done
			}
		}
		
		if (i == size) {
			path[size-1] = '\0';
		}
	}
	return retVal;
}


void Task_release(Task* self) {
	if (self && self->arch) {
		self->arch->release(self);
	}
}




void Thread_initialize(Thread* self, Task* task, thread_t thread, thread_state_t state) {
	if (self == NULL || task == NULL || thread == THREAD_NULL || state == NULL) {
		Log_invalidArgument("self: %p, task: %p, thread: %p, state: %p", 
							self, 
							task, 
							(void*) thread, 
							state);
		
	} else {
		self->task = task;
		self->thread = thread;
		self->state = state;
	}
}


VMAddr Thread_getPC(Thread* self) {
	VMAddr retVal = 0;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->task->arch->getPC(self);
	}
	return retVal;	
}


kern_return_t Thread_setSingleStep(Thread* self, bool enable) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->task->arch->setSingleStep(self, enable);
	}
	return retVal;
}


kern_return_t Thread_getSingleStep(Thread* self, bool* enable) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->task->arch->getSingleStep(self, enable);
	}
	return retVal;
}


kern_return_t Thread_setBreakpoint(Thread* self, VMAddr pc) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->task->arch->setBreakpoint(self, pc);
	}
	return retVal;	
}


kern_return_t Thread_clearBreakpoint(Thread* self) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->task->arch->clearBreakpoint(self);
	}
	return retVal;
}


kern_return_t Thread_findNextBranch(Thread* self, Block* block) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL) {
		Log_invalidArgument("self: %p", self);
		
	} else {
		retVal = self->task->arch->findNextBranch(self, block);
	}
	return retVal;
}



void FunctionArgs_initialize(FunctionArgs* self, Thread* thread, bool stackCookie) {
	if (self == NULL || thread == NULL) {
		Log_invalidArgument("self: %p, thread: %p", self, thread);
		
	} else {
		thread->task->arch->argsInitialize(self, thread, stackCookie);
	}
}


kern_return_t FunctionArgs_get(FunctionArgs* self, uint64_t size, void* value) {
	kern_return_t retVal = KERN_INVALID_ARGUMENT;
	if (self == NULL || value == NULL) {
		Log_invalidArgument("self: %p, value: %p", self, value);
		
	} else {
		retVal = self->thread->task->arch->argsGet(self, size, value);
	}
	return retVal;
}




/*
 * Static function implementations
 */
static cpu_type_t getCPUTypeForTask(pid_t pid) {
	cpu_type_t retVal = 0;
	
    int mib[CTL_MAXNAME] = {0};
    size_t length = CTL_MAXNAME;
    if (sysctlnametomib("sysctl.proc_cputype", mib, &length) != 0) { 
		Log_errorPosix(errno, "sysctlnameotmib");
		
	} else {
		mib[length] = pid;
		length++;
		
		cpu_type_t cpuType = 0;
		size_t cpuLength = sizeof(cpuType);
		if (sysctl(mib, (uint) length, &cpuType, &cpuLength, 0, 0) == 0) {
			retVal = cpuType;
		}
	}
    return retVal;
}


static uint64_t getWordSize(pid_t pid) {
	uint64_t retVal = 0;
	
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
	struct kinfo_proc processInfo = {0};
	size_t bufsize = sizeof(processInfo);
	if (	(sysctl(mib, 
					(unsigned) (sizeof(mib)/sizeof(int)), 
					&processInfo, 
					&bufsize, 
					NULL, 
					0) == -1)
		|| (bufsize == 0)) {
		Log_errorPosix(errno, "sysctl");
		
	} else {
		retVal = sizeof(uint32_t);
		if (processInfo.kp_proc.p_flag & P_LP64) {
			retVal = sizeof(uint64_t);
		}
	}
	return retVal;
}
