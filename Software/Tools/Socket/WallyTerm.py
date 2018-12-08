# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# WallyTerm : Simple terminal for WallyBox
# Version : 0.1
#------------------------------------------------------------------------------#


import sys
import Queue
import threading
import readchar
from WallySocket import cSocketWB



exitFlag = False

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
            if Char == 's':
               self.Lock.acquire()
               print "\rCmd:",
               StrToSend = raw_input()
               self.Lock.release()
            if Char == 'r':
               StrToSend = LastSend
            if Char == 'q':
               break
            self.QOutput.put_nowait(StrToSend)
            LastSend = StrToSend
         except:
            break

      exitFlag = True
      print "\rEnd of listenning"
      del self.QOutput


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   DeviceIp = None

   if len(sys.argv) > 1 :
      DeviceIp = sys.argv[1]
      #if DeviceIp is not xxx.xxx.xxx.xxx:
      #   print "socket.py <IpAdress>"
      #   sys.exit(0)

   SockWB = cSocketWB()
   if not DeviceIp :
      DeviceIp = SockWB.SearchAndConnect()
   else :
      SockWB.Connect( DeviceIp )


   queue = Queue.Queue(10)
   lock = threading.Lock()

   thread = GetCmd(queue, lock)
   thread.start()

   while(not exitFlag) :
      try:
         StrToSend = queue.get_nowait()
         print "\r"+StrToSend
         SockWB.Send( StrToSend + "\r\n" )
      except Queue.Empty:
         pass

      try:
         buf = SockWB.Receive()
         if len(buf) != 0:
            lock.acquire()
            for Line in buf.split( '\r\n' ) :
               print "\t%s"%Line
            lock.release()
      except:
         pass

   del queue


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