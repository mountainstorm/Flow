//
//  Task.h
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

#ifndef Flow_Task_h
#define Flow_Task_h


#include <unistd.h>
#include <mach/mach.h>
#include <stdbool.h>




/*
 * Structure/Type/Enum defines
 */
typedef uint64_t				VMAddr;

typedef struct sFunctionArgs	FunctionArgs;
typedef struct sTask			Task;
typedef struct sThread			Thread;
typedef struct sBlock			Block;
typedef enum eBranchType		BranchType;


typedef void (TaskArch_release)(Task* task);

typedef VMAddr (TaskArch_getPC)(Thread* thread);

typedef kern_return_t (TaskArch_setSingleStep)(Thread* thread, bool enable);
typedef kern_return_t (TaskArch_getSingleStep)(Thread* thread, bool* enable);
typedef kern_return_t (TaskArch_setBreakpoint)(Thread* thread, VMAddr pc);
typedef kern_return_t (TaskArch_clearBreakpoint)(Thread* thread);

typedef kern_return_t (TaskArch_findNextBranch)(Thread* thread, Block* block);


typedef void (TaskArch_argsInitialize)(FunctionArgs* self, Thread* thread, bool stackCookie);
typedef kern_return_t (TaskArch_argsGet)(FunctionArgs* self, uint64_t size, void* value);




typedef struct sTaskArch {
	TaskArch_release*				release;
	
	TaskArch_getPC*					getPC;

	TaskArch_setSingleStep*			setSingleStep;
	TaskArch_getSingleStep*			getSingleStep;
	TaskArch_setBreakpoint*			setBreakpoint;
	TaskArch_clearBreakpoint*		clearBreakpoint;
	
	TaskArch_findNextBranch*		findNextBranch;
	
	TaskArch_argsInitialize*		argsInitialize;
	TaskArch_argsGet*				argsGet;
	
	uint8_t							data[0];
} TaskArch;


struct sTask {
	task_t			task;
	pid_t			pid;
	cpu_type_t		cpuType;
	uint64_t		wordSize;

	TaskArch*		arch;
};


struct sThread {
	Task*			task;
	thread_t		thread;
	thread_state_t	state;
};


enum eBranchType {
	eBranchType_call,	// entry to a function
	eBranchType_ret,	// exit from a function
	eBranchType_sys,	// a syscall, interrupt etc
	eBranchType_other	// jmp, jc, jz etc
};


struct sBlock {
	VMAddr		entry;
	VMAddr		branch;
	BranchType	type;
};


struct sFunctionArgs {
	Thread*				thread;
	thread_state_data_t	state;
};



/*
 * Exported function definitions
 */
kern_return_t Task_createWithTask(Task* self, task_t task);

inline cpu_type_t Task_getCpuType(Task* self);
inline pid_t Task_getPid(Task* self);
inline uint64_t Task_getWordSize(Task* self);

kern_return_t Task_getDyldAllImageInfosAddr(Task* self, struct task_dyld_info* info);
kern_return_t Task_readMemory(Task* self, VMAddr addr, void* data, vm_size_t length);
kern_return_t Task_readString(Task* self, VMAddr addr, char* path, uint64_t size);

void Task_release(Task* self);


void Thread_initialize(Thread* self, Task* task, thread_t thread, thread_state_t state);

inline VMAddr Thread_getPC(Thread* self);

inline kern_return_t Thread_setSingleStep(Thread* self, bool enable);
inline kern_return_t Thread_getSingleStep(Thread* self, bool* enable);
inline kern_return_t Thread_setBreakpoint(Thread* self, VMAddr pc);
inline kern_return_t Thread_clearBreakpoint(Thread* self);

kern_return_t Thread_findNextBranch(Thread* self, Block* block);


inline void FunctionArgs_initialize(FunctionArgs* self, Thread* thread, bool stackCookie);
inline kern_return_t FunctionArgs_get(FunctionArgs* self, uint64_t size, void* value);


#endif
