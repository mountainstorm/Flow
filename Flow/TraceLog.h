//
//  TraceLog.h
//  Flow
//
//  Created by R J Cooper on 01/07/2012.
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

#ifndef Flow_TraceLog_h
#define Flow_TraceLog_h

#include <stdio.h>

#include "Task.h"




/*
 * Struct/Enum definitions
 */
typedef struct sTraceLog {
	FILE*	log;
	Task*	task;
} TraceLog;


typedef enum eTraceLogRecord {
	eTraceLogRecord_block = 0,
	eTraceLogRecord_dyldLoadAddress = 0x80,
	eTraceLogRecord_libraryNotification = 0x81
} TraceLogRecord;




/*
 * Exported function definitions
 */
bool TraceLog_open(TraceLog* self, Task* task, const char* path);
bool TraceLog_dyldLoadAddress(TraceLog* self, VMAddr dyldImageLoadAddress);
bool TraceLog_libraryNotification(TraceLog* self, Thread* thread);
bool TraceLog_block(TraceLog* self, Block* block);
void TraceLog_close(TraceLog* self);


#endif
