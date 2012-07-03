//
//  TraceLog.c
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

#include "TraceLog.h"
#include "Log.h"

#include <limits.h>
#include <mach-o/dyld_images.h>




/*
 * Static function definitions
 */
static kern_return_t getDyldImageInfo32(Task* task, 
										uint64_t* infoPtr, 
										uint64_t* baseAddr, 
										char* path);
static kern_return_t getDyldImageInfo64(Task* task, 
										uint64_t* infoPtr, 
										uint64_t* baseAddr, 
										char* path);




/*
 * Exported functions
 */
bool TraceLog_open(TraceLog* self, Task* task, const char* path) {
	bool retVal = false;
	if (self == NULL || path == NULL) {
		Log_invalidArgument("self: %p, path: %p", self, path);
		
	} else {
		self->log = fopen(path, "wb");
		if (self->log == NULL) {
			Log_invalidArgument("fopen");
			
		} else {
			// write out the cpuType
			uint32_t type = Task_getCpuType(task);
			if (fwrite(&type, sizeof(type), 1, self->log) != 1) {
				Log_error("fwrite");
				
			} else {
				self->task = task;
				retVal = true;
			}
		}
		
		if (retVal == false) {
			TraceLog_close(self);
		}
	}
	return retVal;
}


bool TraceLog_dyldLoadAddress(TraceLog* self, VMAddr dyldImageLoadAddress) {
	bool retVal = false;
	uint8_t type = eTraceLogRecord_dyldLoadAddress;
	if (	(fwrite(&type, sizeof(type), 1, self->log) != 1)
		 || (fwrite(&dyldImageLoadAddress, sizeof(dyldImageLoadAddress), 1, self->log) != 1)) {
		Log_error("fwrite");
		
	 } else {
		 retVal = true;	
	}	
	return retVal;
}


bool TraceLog_libraryNotification(TraceLog* self, Thread* thread) {
	bool retVal = false;
	
	// process notification i.e. this is being called
	// typedef void (*dyld_image_notifier)(enum dyld_image_mode mode, 
	//									   uint32_t infoCount, 
	//									   const struct dyld_image_info info[]);
	FunctionArgs args = {0};
	FunctionArgs_initialize(&args, thread, false);
	
	uint64_t mode = 0;
	uint32_t infoCount = 0;
	VMAddr info = 0;
	
	uint64_t wordSize = Task_getWordSize(self->task);
	if (	(FunctionArgs_get(&args, wordSize, &mode) == KERN_SUCCESS)
		 && (FunctionArgs_get(&args, sizeof(uint32_t), &infoCount) == KERN_SUCCESS)
		 && (FunctionArgs_get(&args, wordSize, &info) == KERN_SUCCESS)) {
		
		// we've got the args, now write out them
		uint8_t type = eTraceLogRecord_libraryNotification;
		if (	(fwrite(&type, sizeof(type), 1, self->log) != 1)
			 || (fwrite(&mode, sizeof(mode), 1, self->log) != 1)
			 || (fwrite(&infoCount, sizeof(infoCount), 1, self->log) != 1)) {
			Log_error("fwrite params");
			
		} else {
			retVal = true;
			
			// now read all the info structures and log them out
			for (uint32_t i = 0; retVal && i < infoCount; i++) {
				uint64_t baseAddress = 0;
				char path[PATH_MAX] = {0};
				
				if (wordSize == sizeof(uint32_t)) {
					retVal = getDyldImageInfo32(self->task, &info, &baseAddress, path) == KERN_SUCCESS;
					
				} else {
					retVal = getDyldImageInfo64(self->task, &info, &baseAddress, path) == KERN_SUCCESS;
				}
				
				printf("%s %llx, path: %s\n", mode == dyld_image_adding ? "+": "-", baseAddress, path);
				
				uint16_t length = strlen(path);
				if (	(fwrite(&baseAddress, sizeof(baseAddress), 1, self->log) != 1)
					 || (fwrite(&length, sizeof(length), 1, self->log) != 1)
					 || (fwrite(&path, length, 1, self->log) != 1)) {
					Log_error("fwrite imageInfo: %u", i);
					retVal = false;
				}
			}
		}
	}
	return retVal;
}


bool TraceLog_block(TraceLog* self, Block* block) {
	bool retVal = false;
	
	/*
	 * So the basic format of the record is a 1 byte type value
	 * followed by a 64bit entry address.  This may optionaly be followed
	 * by a 64bit branch address.
	 *
	 * Type is basically |mttooooo| 
	 *  m: if set its a meta record, if zero (as in this case) its a branch record
	 *  t: branch type
	 *  o: offset
	 *
	 * The idea is we try to encode the branch address as a delta from entry; if we can (its 
	 * less than 0x1F bytes away) we encode it in the bottom bits of type; otherwise we 
	 * output the full 64bit address.
	 */
	uint8_t fcBits = 0;
	switch (block->type) {
		case eBranchType_call:	fcBits = 0x20;	break;
		case eBranchType_ret:	fcBits = 0x40;	break;
		case eBranchType_sys:	fcBits = 0x60;	break;								
		default:				fcBits = 0x00;	break;
	}
	
	uint64_t delta = block->branch - block->entry;
	uint8_t deltaBits = 0x1F;
	if (delta < 0x1F) {
		deltaBits = delta & 0x1F;
	}
	uint8_t type = 0x00 | fcBits | deltaBits;
	if (	(fwrite(&type, sizeof(type), 1, self->log) != 1)
		 || (fwrite(&block->entry, sizeof(block->entry), 1, self->log) != 1)) {
		Log_error("fwrite type/entry");
		
	} else {
		retVal = true;
		if (deltaBits == 0x1F) {
			if (fwrite(&block->branch, sizeof(block->branch), 1, self->log) != 1) {
				Log_error("fwrite offset");
				retVal = false;
			}
		}
	}
	return retVal;	
}


void TraceLog_close(TraceLog* self) {
	if (self && self->log) {
		fclose(self->log);
		self->log = NULL;
	}
}




/*
 * Static function implementations
 */

/*
 * architecture independent versions of struct dyld_image_info
 * (basically pointers are uitn32_t or uint64_t) we only include 
 * the fields we need as we're lazy
 */
typedef struct sDyldImageInfo32 {
	uint32_t	imageLoadAddress;	/* base address image is mapped into */
	uint32_t	imageFilePath;		/* path dyld used to load the image */
	uint32_t	imageFileModDate;	/* time_t of image file */
	/* if stat().st_mtime of imageFilePath does not match imageFileModDate, */
	/* then file has been modified since dyld loaded it */
} DyldImageInfo32;


typedef struct sDyldImageInfo64 {
	uint64_t	imageLoadAddress;	/* base address image is mapped into */
	uint64_t	imageFilePath;		/* path dyld used to load the image */
	uint64_t	imageFileModDate;	/* time_t of image file */
	/* if stat().st_mtime of imageFilePath does not match imageFileModDate, */
	/* then file has been modified since dyld loaded it */
} DyldImageInfo64;




static kern_return_t getDyldImageInfo32(Task* task, 
										uint64_t* infoPtr, 
										uint64_t* baseAddr, 
										char* path) {
	DyldImageInfo32 imageInfo = {0};
	kern_return_t retVal = Task_readMemory(task, 
										   *infoPtr, 
										   &imageInfo, 
										   sizeof(imageInfo));
	if (retVal == KERN_SUCCESS) {
		retVal = Task_readString(task, imageInfo.imageFilePath, path, PATH_MAX);
		if (retVal == KERN_SUCCESS) {
			*baseAddr = imageInfo.imageLoadAddress;
			*infoPtr += sizeof(imageInfo);
		}		
	}
	return retVal;
}


static kern_return_t getDyldImageInfo64(Task* task, 
										uint64_t* infoPtr, 
										uint64_t* baseAddr, 
										char* path) {
	DyldImageInfo64 imageInfo = {0};
	kern_return_t retVal = Task_readMemory(task, 
										   *infoPtr, 
										   &imageInfo, 
										   sizeof(imageInfo));
	if (retVal == KERN_SUCCESS) {
		retVal = Task_readString(task, imageInfo.imageFilePath, path, PATH_MAX);
		if (retVal == KERN_SUCCESS) {
			*baseAddr = imageInfo.imageLoadAddress;
			*infoPtr += sizeof(imageInfo);
		}		
	}
	return retVal;	
}

