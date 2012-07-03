Flow ----

Flow is a simple branch tracer for OSX (and iOS when I can be bothered to
write the ARM code)

Basically you run it like a debugger (because thats what it is); it can attach
or spawn a process.  It halts the process and searches forward until it hits a
branch instruction; jmp, call, ret, syscall etc.  It then sets a breakpoint on
that instruction and runs the program until the breakpoint.  It then single
steps and starts again.

Basically, the output is trace log file with every block of code; a block
being a group of instructions starting with the destination of the last branch
and ending with the next branch (you also get the branch type).

You can then use FlowCalls.py to dump the call tree out in a terminal window.


Building 
-------- 
Its an XCode project so nothing too difficult; you will need a self signed cert 
created like this: https://llvm.org/svn/llvm-project/lldb/trunk/docs/code-signing.txt

and a built copy of distorm in the distorm directory: http://www.ragestorm.net/distorm/


TODO/Issues 
----------- 
* ARM Support 
* Springboard Launch 
* Log out thread info 
* Check all threads are handled correctly when attaching to a running 
  multithreaded process


License (MIT style)
-------------------
Copyright (c) 2012 Mountainstorm

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.