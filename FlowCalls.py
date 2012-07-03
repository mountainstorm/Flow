# Copyright (c) 2012 Mountainstorm
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#!/usr/bin/python
# coding: latin-1

from macholib import MachO 
import subprocess
import struct	
import os


class TraceBlock:
	def __init__(self, landed, fc, fcType):
		self.landed = landed
		self.fc = fc
		self.fcType = fcType
		self.timeline = []
	
		
class TraceArch:
	def __init__(self, root, cpuType):
		self.root = root
		self.cpuType = cpuType
	

class TraceLibrary:
	def __init__(self, arch, path, baseAddr):
		self.arch = arch
		self.path = path
		self.baseAddr = baseAddr
		self.defaultBaseAddr = 0
		self.macho = None
		self.sections = {}
		self.symbols = {}
		fullPath = os.path.join(self.arch.root, path)
		macho = MachO.MachO(fullPath)
		for h in macho.headers:
			if h.header.cputype == self.arch.cpuType:
				self.macho = h
				# find the endAddr
				endAddr = 0
				for c in self.macho.commands:
					if c[0].cmd == MachO.LC_SEGMENT or c[0].cmd == MachO.LC_SEGMENT_64:
						if c[1].segname.strip("\0") == "__TEXT":
							self.defaultBaseAddr = c[1].vmaddr
						if (c[1].vmaddr+c[1].vmsize) > endAddr:
							endAddr = c[1].vmaddr+c[1].vmsize
						if c[1].maxprot & 0x4: # VM_PROT_EXECUTE
							# add all the sections
							for section in c[2]:
								self.sections[section.addr] = section
				self.endAddr = self.baseAddr+endAddr
				#print "%s: %x - %x" % (self.path, self.baseAddr, self.endAddr)
		# get the symbols (not the best way)
		if self.arch.cpuType & 0x01000000:
			arch = "x86_64" # out version doesn't have the 64bit support
		else:
			arch = MachO.CPU_TYPE_NAMES[self.arch.cpuType]		
		symbolText = subprocess.check_output(['/usr/bin/otool', '-Iv', '-arch', arch, fullPath])
		symbolLines = symbolText.split("\n")
		for l in symbolLines:
			parts = l.split()
			if len(parts) == 3 and len(parts[0]) > 2:
				if parts[0][:2] == "0x":
					offset = int(parts[0][2:], 16)-self.defaultBaseAddr
					self.symbols[offset] = parts[2].strip()

			
	def resolveSymbol(self, offset):
		retVal = None
		if offset in self.symbols:	
			retVal = self.symbols[offset]
		return retVal
			
						
class TraceProcess:
	def __init__(self, arch):
		self.arch = arch	
		self.timeline = []
		self.libraries = {}
		self.parseStack = [self]
		
	def addLibrary(self, path, baseAddr):
		#print "%x - %s" %  (baseAddr, path)
		self.libraries[baseAddr] = TraceLibrary(self.arch, path, baseAddr)
		
	def removeLibrary(self, baseAddr):
		del self.libraries[baseAddr]
		
	def addLFCPair(self, landed, fc, fcType):
		tb = TraceBlock(landed, fc, fcType)
		self.parseStack[-1].timeline.append(tb)
		if fcType == 0x1:
			# call
			self.parseStack.append(tb)
		elif fcType == 0x2:
			# ret
			if len(self.parseStack) != 1:
				self.parseStack.pop()

	def resolveLibrary(self, pc):
		offset = pc
		library = None
		for l in self.libraries.values():
			if pc >= l.baseAddr and pc < l.endAddr:
				offset = pc-l.baseAddr # get offset into library
				library = l
				break
		return (offset, library)
		

class FlowLog:
	def __init__(self, filename, root):
		self.log = file(filename, "rb")
		cpuType, = struct.unpack("I", self.log.read(struct.calcsize("I")))
		self.process = TraceProcess(TraceArch(root, cpuType))
		self._parseFile()
		
	def _parseFile(self):
		try:
			while True:
				type, = struct.unpack("B", self.log.read(struct.calcsize("B")))
				if (type & 0x80) == 0:
					# landed, next flow control pair
					landed, = struct.unpack("Q", self.log.read(struct.calcsize("Q")))
					fcType = type & 0x60
					fc = landed + (type & 0x1F)
					if (type & 0x1F) == 0x1F:
						fc, = struct.unpack("Q", self.log.read(struct.calcsize("Q")))
					self.process.addLFCPair(landed, fc, fcType >> 5)
				else:
					if type == 0x81:
						mode, = struct.unpack("Q", self.log.read(struct.calcsize("Q")))
						infoCount, = struct.unpack("I", self.log.read(struct.calcsize("I")))
						for i in xrange(0, infoCount):
							baseAddr, = struct.unpack("Q", self.log.read(struct.calcsize("Q")))
							strlen, = struct.unpack("H", self.log.read(struct.calcsize("H")))
							path = self.log.read(strlen) # drop the null at the end
							if mode == 0:
								self.process.addLibrary(path, baseAddr)
							else:
								self.process.removeLibrary(baseAddr)
					elif type == 0x80:
						dyldAddr, = struct.unpack("Q", self.log.read(struct.calcsize("Q")))
						self.process.addLibrary("/usr/lib/dyld", dyldAddr)						
		except struct.error:
			pass
		
		
if __name__ == "__main__":
	import sys
	import difflib
	
	if len(sys.argv) != 3:
		print "Usage: FlowView.py <sdk-root> <log1>"
		sys.exit(0)
	
	f1 = FlowLog(sys.argv[2], sys.argv[1])
	

	def addrStr(addr):
		offset, lib = f1.process.resolveLibrary(addr)
		name = '%x !' % offset
		if lib:
			symbol = lib.resolveSymbol(offset)
			
		if lib and symbol:
			name = '%s::%s' % (lib.path, symbol)
		elif lib:
			name = '%s:%x' % (lib.path, offset)		
		return name
	
	
	def outputStr(target, lib, str):
		if target == None or lib == None or (lib and (lib.path == target)):
			print str
		
	'''
	def outputBlock(block, target = None, padding = ''):
		offset, lib = f1.process.resolveLibrary(block.fc)
		end = ''
		if block.landed != block.fc:	
			end = ' - %x' % offset
		outputStr(target, lib, padding+'├ '+addrStr(block.landed)+end)

		if block.fcType == 1: # call
			outputStr(target, lib, padding+'├┬')
		elif block.fcType == 3: # syscall
			outputStr(target, lib, padding+'├┬')
			outputStr(target, lib, padding+'│├ #')
			
		if len(block.timeline) > 0:
			for b in block.timeline:
				outputBlock(b, target, padding+'│')
			outputStr(target, lib, padding+'│')
	'''

	def outputBlock(block, idx, target, padding = ''):
		offset, lib = f1.process.resolveLibrary(block.fc)
		if idx == 0 or lib == None or (lib != None and lib.resolveSymbol(offset)):
			if idx == 0:
				outputStr(target, lib, padding+'├┬ '+addrStr(block.landed))
			else:
				outputStr(target, lib, padding+'├ '+addrStr(block.landed))

		if block.fcType == 3: # syscall
			outputStr(target, lib, padding+'├─ #')
			
		if len(block.timeline) > 0:
			i = 0
			for b in block.timeline:
				outputBlock(b, i, target, padding+'│')
				i += 1
		
	
	def outputProcess(process, target = None):
		i = 0
		for b in process.timeline:
			outputBlock(b, i, target)
			i += 1
			
	
	outputProcess(f1.process, "/usr/bin/bc")
	
