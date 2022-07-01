import struct
import numpy as np
from collections import namedtuple
import pylab as plt
import os
import time 
import argparse 
from pathlib import Path

'''
Usage: 

python3 sult.py <filepath> -s
'''

updateRate = 10
'''
Naming each entry in the header struct
'''
SultHeaderStruct = namedtuple('SultHeader', ['sync', 'type', 'id', 'flags', 'len', 'rtcL', 'rtcH', 'seqlen'])

def isValidHead(head):
  '''
  confim that header is valid
  '''
  if head.sync != 0xAA:
    return False
  return True

def pktFromFile(f):
  '''
  stripping the headers from the file
  '''
  headBytes = f.read(16)
  if len(headBytes) < 16:
    return None

  haveHead = False
  while not haveHead:
    head = SultHeaderStruct._make(struct.unpack('BBBBHHII', headBytes))
    haveHead = isValidHead(head)
    if haveHead:
      break
    if any(b != 0 for b in headBytes[:4]):
      raiseFileException(f, "lost sult sync")
    else:

      headBytes = headBytes[4:] + f.read(4)
      if len(headBytes) < 16:
        return None
  data = np.fromfile(f, np.uint32, head.len)
  if len(data) != head.len:   
    return None
  ret = Packet(head, data)
  return ret

def raiseFileException(f, msg):
  errorStr = "ERROR:"+msg+"\n"
  fOffset = f.tell()
  errorStr += "Location of ERROR %d = 0x%X"%(fOffset, fOffset)
  raise Exception(errorStr)

def typeAndId(t, i):
  return t | (i<<8)

def typeAndId2Type(v):
  return v & 0xFF

def typeAndId2Id(v):
  return v >> 8

class Packet:
  '''
  creating functions used later in the code.
  '''
  def __init__(self, head, data):
    self.head = head
    self.rawData = data
  '''
  Referencing the header entries
  '''
  def typeAndId(self):
    return typeAndId(self.head.type, self.head.id)

  def hRtc(self):
    return self.head.rtcL | (self.head.rtcH<<16)

  def hSync(self):
    return self.head.sync

  def hFlags(self):
    return self.head.flags
  
  def hFlagSof(self):
    return (self.hFlags() & (1<<0)) != 0

  def hFlagEof(self):
    return (self.hFlags() & (1<<1)) != 0

  def hFlagDataLoss(self):
    return (self.hFlags() & (1<<2)) != 0

  # def hRtcL(self):
  #     return self.head.rtcL

  # def hRtcH(self):
  #     return self.head.rtcH

  def hType(self):
    return self.head.type

  def hId(self):
    return self.head.id

  def hSeqLen(self):
    return self.head.seqlen

  def hLen(self):
    return self.head.len

  def data(self, dtype=np.uint32):
    return self.rawData.view(dtype)
  
  def pktMatchingTypeandID(self):
    '''
    defining the different possible types and ID's
    '''
    typeIds =  ((self.hType(), self.hId()), (self.hType(), 0xff), (0xff, self.hId()), (0xff, 0xff))
    return (typeAndId(t, i) for (t,i) in typeIds )

  def printhead(self):
    print(self.headStr())

  def headStr(self):
    '''
    formatting the header and converting to hex and returning as a string
    '''
    return ' '.join(('Packet.head(', 'sync =', hex(self.hSync()),
      'Type =', hex(self.hType()),
      'Id =', hex(self.hId()),
      'Flags =', hex(self.hFlags()),
      'Len =', hex(self.hLen()),
      'Rtc =', hex(self.hRtc()),
      'Seqlen =', hex(self.hSeqLen()),')'))



class Router:
  def __init__(self):
    '''
    Creation of a series of dictionaries.
    '''
    self.seqLenDict = {}
    self.cbDictArr = {}
    self.cbAllArr = []

  def readIO(self, io, hType=0xff, hId=0xff, maxReadSize=None, stopEndFrame=False):
    '''
    Read packets from IO (can be a file or fileName)
    '''
    if(isinstance(io, str)):
      f = open(io,'rb')
      h = os.path.getsize(io)
    else:
      f = io
      h = f.tell()
    try:
      pkt = pktFromFile(f)
      if pkt is None:
        return False
      
      updateTime = time.time()

      while pkt is not None:
        self._routePkt(pkt)
        if(stopEndFrame and pkt.hFlagEof()):
          if ((hType == 0xff or pkt.hType() == hType) and (hId == 0xff or pkt.hId() == hId)):
            return True
        pkt = pktFromFile(f)
        '''
        Percentage counter through file
        '''
        newTime = time.time()
        if newTime - updateTime >= updateRate:
          progress = ((f.tell()/h)*100)
          print("%.2f" % progress, '%', ' complete')
          updateTime = newTime
        if maxReadSize is not None and f.tell() >= maxReadSize:
          return True
    except Exception as e:
      fOffset = f.tell()
      print("Location of ERROR %d = 0x%X bytes"%(fOffset, fOffset))
      raise e

  def _routePkt(self, pkt):
    '''
    Checking for any data loss
    '''
    k = pkt.typeAndId()
    if pkt.hFlagDataLoss():
      print('WARNING data loss:', pkt.headStr())
    '''
    Checking whether the sequence length was the expected length
    '''
    if pkt.hType() != 0xCC: #ignore seqnece length for 0xCC
      if k in self.seqLenDict:
        expectedSeqLen = self.seqLenDict[k]                 
        if expectedSeqLen != pkt.hSeqLen():
          print("The sequence length for type 0x%X is incorrect! (got 0x%X expected 0x%X)"%(pkt.hType(), pkt.hSeqLen(), expectedSeqLen) )
          # raise Exception("The sequence length is incorrect! (got 0x%X expected 0x%X)"%(expectedSeqLen, pkt.hSeqLen()) )
          
      nextSeqLen = pkt.hSeqLen() + pkt.hLen()
      self.seqLenDict[k] = nextSeqLen

    self._callReaders(pkt)

    '''print headers'''
    # pkt.printhead()
    '''print sequence length dictionary'''
    # print (self.seqLenDict)

  def addReader(self, hType, hId, cb):
    '''
    Creating unique type and ID entries in a dictionary
    '''
    k = typeAndId(hType,hId)
    if k in self.cbDictArr:
      self.cbDictArr[k].append(cb)
    else:
      self.cbDictArr[k] = [cb]
    
  def _callReaders(self, pkt):
    '''
    defining the keys in a the type and ID dictionary
    '''
    k = pkt.typeAndId()
    mks = pkt.pktMatchingTypeandID()
    for k in mks:
      if k in self.cbDictArr:
        for cb in self.cbDictArr[k]:
          cb(pkt)
  
  def addDataReader(self, hType, hId, cb, maxSamps, dtype=np.uint32):
    data = np.zeros(maxSamps, dtype=dtype)
    p_dataIdx = [0]

    def cbInternal(sPkt):
      '''
      callback function to concatonate multiple data sections and call cb with the concatonated data
      '''
      pktData = sPkt.data(dtype)
      eof = sPkt.hFlagEof()
      idxEnd = p_dataIdx[0] + len(pktData) 
      if idxEnd >= maxSamps:
        
        #copy till end
        cpy1Len = maxSamps-p_dataIdx[0]
        data[p_dataIdx[0]:maxSamps] = pktData[:cpy1Len]

        cb(data) # call callback

        #start leftover in new buffer
        cpy2Len = len(pktData) - cpy1Len
        data[:cpy2Len] = pktData[cpy1Len:]
        p_dataIdx[0] = cpy2Len

      else:
        data[p_dataIdx[0]:idxEnd] = pktData
        p_dataIdx[0] = idxEnd
      
      if eof == True:
        cb(data[:p_dataIdx[0]])
        p_dataIdx[0] = 0

    self.addReader(hType, hId, cbInternal)

def extractData(f, sultType, sultId, dtype=None, maxReadSize=None, stopEndFrame=False):
  '''
  Read packets from IO and extract data
  '''
  lanes = 1
  if dtype is None:
    lanes  = (sultType >> 4) & 0xF
    if not(lanes == 1 or lanes == 2):
      raise Exception("cannot derive dtype "+hex(sultType))
    wBytes =  sultType & 0xF
    if wBytes == 1:
      dtype = np.int8
    elif wBytes == 2:
      dtype = np.int16
    elif wBytes == 4:
      dtype = np.int32
    else:
      raise Exception("cannot derive dtype "+hex(sultType))

  p_data = [None]
  def cb(pkt):
    pktData = pkt.data(dtype)
    # raw IQ data has a 4 byte field that is the sub-rtc timing
    if pkt.hType() == 0x22:
      pktData = pktData[2:]
    if p_data[0] is None:
      p_data[0] = pktData
    else:
      p_data[0] = np.concatenate((p_data[0], pktData))
    
  router = Router()
  router.addReader(sultType, sultId, cb)
  while p_data[0] is None:
    if router.readIO(f, sultType, sultId, maxReadSize, stopEndFrame) == False:
      break

  if(p_data[0] is not None):
    p_data[0] = p_data[0].reshape((-1,lanes))
  return p_data[0]


def summary(filepath):
  '''
  A dictionary that contains each unique type and Id combination that was unpacked in a file.
  '''
  router = Router()
  summaryDict = {}
  
  def cb(pkt):
    '''
    Callback to look up the unique packet in a dictionary and keeps track of the number of packets in the dictionary
    '''
    k = pkt.typeAndId()
    if k in summaryDict:
      summaryDict[k] += 1
    else:
      summaryDict[k] = 1
  
  router.addReader(0xff, 0xff, cb)
  router.readIO(filePath)

  print(summaryDict)
  for k in summaryDict:
    '''
    Formatting the summary dictionary keys and converting to hex
    '''
    print('Type', hex(typeAndId2Type(k)), 'ID', hex(typeAndId2Id(k)), ':',summaryDict[k], 'packets')


def parse_arguments():
  '''
  Setting command line arguments for filepath
  '''
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument('filename', type=str, help='File path to read from', default=None)
  arg_parser.add_argument('-e', type=str, help='Extract data to binary file(s) with this prefix')
  arg_parser.add_argument('-t', dest='type', type=int, help='Data type to extract', default=0x22)
  arg_parser.add_argument('-i', dest='id', type=int, help='Data id to extract', default=0x1)
  arg_parser.add_argument('-s', action='store_true', help='Print summary and exit')
  return arg_parser.parse_args()


if __name__ == '__main__':
  # router = SultRouter()
  '''
  Confirming a filepath is supplied
  '''
  args = parse_arguments()
  filePath = args.filename

  

  if args.filename is None:
    print('A file is required')
    exit

  if args.s:
    summary(args.filename)
  # else:
  
  if args.e is not None:
    print('Extracting...')
    idx = 0
    inFile = open(args.filename, 'br')
    data = extractData(inFile, args.type, args.id, stopEndFrame=True)

    while data is not None:
      fileNum = '_{0:05d}'.format(idx)
      path = Path(args.e)

      fileStr = str(path.with_name(path.stem + fileNum + path.suffix))
      # print('writing to: ' + fileStr)

      outFile = open(fileStr, 'wb')
      outFile.write(data)
      idx += 1
      data = extractData(inFile, args.type, args.id, stopEndFrame=True)
    
