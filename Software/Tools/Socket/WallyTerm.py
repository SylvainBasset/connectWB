# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# WallyTerm : Simple terminal for WallyBox
# Version : 0.1
#------------------------------------------------------------------------------#


import sys
import re
import Queue
import threading
import readchar
import readline                        # keep to get input history
from WallySocket import cSocketWB
from optparse import OptionParser


exitFlag = False

C_RED = '\033[91m'
C_GREEN = '\033[92m'
C_ORANGE = '\033[93m'
C_BLUE = '\033[94m'
C_END = '\033[0m'
C_BOLD = '\033[1m'


#---------------------------------------------------------------------------#

class GetCmd(threading.Thread):
   def __init__(self, QOutput, Lock):
      super(GetCmd, self).__init__()
      self.QOutput = QOutput
      self.Lock = Lock

   def run(self):
      global exitFlag
      LastSend = ""
      while(True):
         try :
            Char = readchar.readkey()
            StrToSend = ""
            if Char == 'a':
               StrToSend = u"$01:AT"
            if Char == 'p':
               StrToSend = u"$01:AT+S.PING=192.168.0.1"
            if Char == 'f':
               StrToSend = u"$01:AT+S.FSL"
            if Char == 's':
               self.Lock.acquire()
               StrToSend = raw_input("\rCmd: ")
               self.Lock.release()
            if Char == 'r':
               StrToSend = LastSend
            if Char == 'q':
               break
            if StrToSend != "":
               self.QOutput.put_nowait(StrToSend)
               LastSend = StrToSend
         except:
            break

      exitFlag = True
      print "\rEnd of listenning"
      del self.QOutput


#---------------------------------------------------------------------------#

def PrintTerm( fLog, String ):
   if fLog:
      fLog.write( String )

   String = re.sub( "(\$[0-7]\d\:)",  C_BOLD + C_BLUE + "\g<1>" + C_END, String )
   String = re.sub( "(\$[8-F]\d\:)",  C_BOLD + C_ORANGE + "\g<1>" + C_END, String )

   print String


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   DeviceIp = None

   parser = OptionParser()
   parser.add_option("-i", "--ip", dest="DeviceIp", help="specify Ip address")
   parser.add_option("-l", "--log", dest="LogFile", help="specify log file")

   (options, args) = parser.parse_args()

   fLog = None
   if options.LogFile:
      try:
         fLog = open( options.LogFile, "w" )
      except IOError :
         print "Unable to open log file"
         sys.exit(1)

   SockWB = cSocketWB()
   if options.DeviceIp :
      DeviceIp = options.DeviceIp
      #if DeviceIp is not xxx.xxx.xxx.xxx:
      #   print "socket.py <IpAdress>"
      #   sys.exit(0)
      SockWB.Connect( DeviceIp )
   else :
      DeviceIp = SockWB.SearchAndConnect()


   queue = Queue.Queue(10)
   lock = threading.Lock()

   thread = GetCmd(queue, lock)
   thread.start()

   while(not exitFlag) :
      try:
         StrToSend = queue.get_nowait()
         PrintTerm( fLog, "\r" + StrToSend )
         SockWB.Send( StrToSend + "\r\n" )
      except Queue.Empty:
         pass

      try:
         buf = SockWB.Receive()
         if len(buf) != 0:
            lock.acquire()
            for Line in buf.splitlines() :
               PrintTerm( fLog, "\r\t%s"%Line )
            lock.release()
      except:
         pass

   #--------------------#

   del queue

   if options.LogFile:
      try:
         fLog.close()
      except:
         pass

   #//LastSend = ""
   #//
   #//while True:
   #//   if msvcrt.kbhit():
   #//      StrToSend = ""
   #//      Char = msvcrt.getch()
   #//      if Char == 'a':
   #//         StrToSend = u"$01:AT"
   #//      if Char == 'p':
   #//         StrToSend = u"$01:AT+S.PING=192.168.0.1"
   #//      if Char == 's':
   #//         StrToSend = GetCommand()
   #//      if Char == 'r':
   #//         StrToSend = LastSend
   #//      if Char == 'q':
   #//         break
   #//
   #//      if StrToSend != "":
   #//         LastSend = StrToSend
   #//         print StrToSend
   #//         SockWB.Send( StrToSend + "\r\n" )
   #//   try:
   #//      buf = SockWB.Receive()
   #//      if len(buf) != 0:
   #//         for Line in buf.split( '\r\n' ) :
   #//            print "\t%s"%Line
   #//   except:
   #//      pass
   #//
   #//print "Close"
   #//SockWB.Close()