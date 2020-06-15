
# this example test the speed of IDX 
import os,sys,math, numpy as np
from OpenVisus import *

KB,MB,GB=1024,1024*1024,1024*1024*1024

# ////////////////////////////////////////////////////////////////
def GenerateRandomData(dims, dtype):
	"""
	half data will be zero, half will be random
	in case you want to test with compression having a 50% compression ratio
	"""
	
	W,H,D=dims
	ret=np.zeros((D, H, W),dtype=convert_dtype(dtype.get(0).toString()))

	if ret.dtype==numpy.float32 or ret.dtype==numpy.float64:
		ret[0:int(D/2),:,]=np.random.rand(int(D/2), H, W,dtype=ret.dtype)
	else:
		ret[0:int(D/2),:,]=np.random.randint(0, 65535, (int(D/2), H, W),dtype=ret.dtype)
	return ret	

# ////////////////////////////////////////////////////////////////
def CreateIdxDataset(filename, DIMS=None, dtype=None, blocksize=0, default_layout="rowmajor",default_compression=""):

	print("Creating idx dataset", filename,"...")

	dtype=DType.fromString(dtype)
	totvoxels=DIMS[0]*DIMS[1]*DIMS[2]
	samplessize=dtype.getByteSize()
	samplesperblock=int(blocksize/samplessize)
	bitsperblock=int(math.log(samplesperblock,2))
	nblocks=int(totvoxels/samplesperblock)
	blocksperfile=nblocks # I want only one file containing all blocks (to avoid fopen/fclose slowness)
	
	#print("blocksize",StringUtils.getStringFromByteSize(int(blocksize)))
	#print("samplesperblock",samplesperblock)
	#print("bitsperblock",bitsperblock)
	#print("nblocks",nblocks)
	#print("blocksperfile",blocksperfile)

	field=Field("data")
	field.dtype=dtype
	field.default_layout=default_layout
	field.default_compression=default_compression
	CreateIdx(url=filename,dims=DIMS,fields=[field], bitsperblock=bitsperblock, blocksperfile=blocksperfile, filename_template="./" + os.path.basename(filename)+".bin")

	# fill with fake data
	db=LoadDataset(filename)
	field=db.getDefaultField()
	time=db.getDefaultTime()
	access = IdxDiskAccess.create(db)
	access.disableAsync()
	access.disableWriteLock()
	access.beginWrite()
	nblocks=db.getTotalNumberOfBlocks()
	for block_id in range(nblocks):
		write_block = BlockQuery(db, field, time, access.getStartAddress(block_id), access.getEndAddress(block_id), ord('w'), Aborted())
		nsamples=write_block.getNumberOfSamples().toVector()
		buffer=GenerateRandomData(nsamples,dtype)
		write_block.buffer=Array.fromNumPy(buffer, bShareMem=True)
		db.executeBlockQueryAndWait(access, write_block)
		Assert(write_block.ok())
		#if block_id % 300 ==0:
		#	print(block_id,"{}%".format(int(100*block_id/nblocks)))
	access.endWrite()	
	del access
	del db

# /////////////////////////////////////////////////////
def CreateFullRes(filename, totsize=0,writesize=1024*1024*1024):
	if os.path.exists(filename): os.remove(filename)
	file=File()
	Assert(file.createAndOpen(filename, "w"))
	cursor=0
	print("Creating fullres file",filename,"...")
	while cursor<totsize:
		nwrite=min(writesize,totsize-cursor)
		print("{}% ".format(int(100*cursor/totsize)), end='',flush=True)
		buffer=np.zeros(nwrite,numpy.uint8)
		buffer[0:int(nwrite/2)]=np.random.randint(0, 256, int(nwrite/2),dtype=numpy.uint8)
		array=Array.fromNumPy(buffer, bShareMem=True)
		Assert(file.write(cursor, writesize, array.c_ptr()))
		cursor+=nwrite
	file.close()
	del file
	print("Created fullres file",filename)

# /////////////////////////////////////////////////////
def ReadFullResBlocks(filename, blocksize=0):
	totsize=os.path.getsize(filename)
	nblocks=int(totsize/blocksize)
	file=File()
	Assert(file.open(filename, "r"))
	array=Array(blocksize, DType.fromString("uint8"))
	try:
		while True:
			blockid=np.random.randint(0,nblocks)
			file.read(blockid * blocksize,array.c_size(),array.c_ptr())
			yield (array,array.c_size())
	except GeneratorExit:
		pass
	file.close()
	del file

# /////////////////////////////////////////////////////
def ReadSeq(filename):
	file=File()
	Assert(file.open(filename, "r"))
	cursor=0
	array=Array(1024*1024*1024, DType.fromString("uint8")) 
	try:
		while True:
			if not file.read(cursor,array.c_size(),array.c_ptr()):
				cursor=0
			else:
				cursor+=array.c_size()
			yield (array,array.c_size())
	except GeneratorExit:
		pass
	file.close()
	del file

# ////////////////////////////////////////////////////////////////
def ReadIdxBlockQuery(filename):
	db=PyDataset(filename)
	access = IdxDiskAccess.create(db)
	access.disableAsync()
	access.beginRead()
	nblocks=db.getTotalNumberOfBlocks()
	try:
		while True:
			block_id=np.random.randint(0,nblocks)
			data=db.readBlock(block_id, access=access)
			Assert(data is not None)
			yield (data,data.nbytes)
	except GeneratorExit:
		pass
	access.endRead()
	del access

# ////////////////////////////////////////////////////////////////
def ReadIdxBoxQuery(filename, dims):
	db=PyDataset(filename)
	DIMS=db.getLogicSize()
	access = IdxDiskAccess.create(db)
	access.disableAsync()
	access.beginRead()
	samplesize=db.getDefaultField().dtype.getByteSize()
	try:
		while True:
			x=np.random.randint(0,DIMS[0]/dims[0])*dims[0]
			y=np.random.randint(0,DIMS[1]/dims[1])*dims[1]
			z=np.random.randint(0,DIMS[2]/dims[2])*dims[2]
			BlockQuery.global_stats().resetStats()
			data=db.read(logic_box=[(x,y,z),(x+dims[0],y+dims[1],z+dims[2])],access=access)
			Assert(data.nbytes==dims[0]*dims[1]*dims[2]*samplesize)
			N=BlockQuery.global_stats().getNumRead()
			# print(N)
			yield (data,data.nbytes)
	except GeneratorExit:
		pass
	access.endRead()
	del access

# ////////////////////////////////////////////////////////////////
def TimeIt(name, gen, max_seconds=60):
	data, needed=next(gen) # skip any headers (open/close file)
	NEEDED, DISK, DONE, T1=0,0,0,Time.now()
	while T1.elapsedSec()<max_seconds:
		File.global_stats().resetStats()
		data, needed=next(gen)
		NEEDED+=needed
		DISK+=File.global_stats().getReadBytes()
		DONE+=1
	SEC=T1.elapsedSec()
	print(name,"{:0.2f}".format(NEEDED/(SEC*MB)),"\t{:0.2f}".format(DISK/(SEC*MB)),"\t{:0.2f}".format(DISK/NEEDED))

# ////////////////////////////////////////////////////////////////
def Main():

	np.random.seed()

	# 32^3 uint16 is 64k
	# 16^3 uint16 is  8k

	# 128GB
	DIMS=(4096,4096,8192)
	DTYPE="uint8"

	for dir in ["C:/tmp/test_speed", "D:/tmp/test_speed"]:
		filename=dir+"/fullres.bin"
		CreateFullRes(filename,totsize=DIMS[0]*DIMS[1]*DIMS[2]) 
		TimeIt(filename+"-readsseq",  ReadSeq(filename))
		for blocksize in (128,64,32,16,8,4):
			TimeIt(filename+"-{:03d}k".format(blocksize), ReadFullResBlocks(filename, blocksize=128*KB))
		RemoveFiles(filename)
	
		for blocksize in (128,64,32,16,8,4):
			filename=dir + "/{:03d}k.idx".format(blocksize)
			CreateIdxDataset(filename, DIMS=DIMS, dtype=DTYPE, blocksize=blocksize*KB)
			TimeIt(filename+"-BlockQuery", ReadIdxBlockQuery(filename))
			for dims in [(16,16,16),(16,16,32),(16,32,32),(32,32,32),(32,32,64),(32,64,64)]:
				TimeIt(filename+"-BoxQuery{:03d}k".format(int(dims[0]*dim[1]*dim[2]/1024)),  ReadIdxBoxQuery(filename, dims))
			RemoveFiles(filename+"*")

	print("all done")
	sys.exit(0)


# ////////////////////////////////////////////////////////////////
if __name__=="__main__":
	Main()



