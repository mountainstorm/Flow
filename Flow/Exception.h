//
//  Exception.h
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

#ifndef Flow_Exception_h
#define Flow_Exception_h


#include <mach/mach.h>
#include <stdint.h>




/*
 * Structure/Type/Enum definitions
 */
typedef enum eExceptionAction {
	eExceptionAction_abortTask,
	eExceptionAction_continue
} ExceptionAction;


typedef struct sException {
	task_t					task;
	thread_t				thread;
	thread_state_t			state;
	exception_type_t		type;
	
	mach_msg_type_number_t	maxCount;
	mach_msg_type_number_t	count;
	mach_exception_data_t	data;
} Exception;




/*
 * Exported function definitions
 */
void Exception_create(Exception* self);
void Exception_release(Exception* self);
inline kern_return_t Exception_assign(Exception* self, 
									  mach_port_t task,
									  mach_port_t thread,
									  thread_state_t state,									  
									  exception_type_t type,
									  mach_exception_data_t code,
									  mach_msg_type_number_t codeCnt);
inline int Exception_softwareSignal(Exception* self);
const char* Exception_name(Exception* self);


#endif
