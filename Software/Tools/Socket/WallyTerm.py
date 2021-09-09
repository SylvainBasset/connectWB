# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# WallyTerm : Simple terminal for WallyBox
# Version : 0.1
#------------------------------------------------------------------------------#


import sys
import re
import queue
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
            if Char == 'h':
               self.help()
            if Char == 'a':
               StrToSend = u"$01:AT"
            if Char == 'p':
               StrToSend = u"$01:AT+S.PING=192.168.0.1"
            if Char == 'f':
               StrToSend = u"$01:AT+S.FSL"
            if Char == 's':
               self.Lock.acquire()
               StrToSend = input("\rCmd: ")
               self.Lock.release()
            if Char == 'r':
               StrToSend = LastSend
            if Char == 'q':
               break
            if StrToSend != "":
               self.QOutput.put_nowait(StrToSend)
               LastSend = StrToSend
         except Exception as e:
            print(e)
            break

      exitFlag = True
      print( "\rEnd of listenning" )
      del self.QOutput


   def help(self):
      print( "" )
      print( "h : aide" )
      print( "a : commande AT simple ($01:AT)" )
      print( "p : ping ($01:AT+S.PING=192.168.0.1)" )
      print( "f : liste fichiers html ($01:AT+S.FSL)" )
      print( "s : envoi commande AT utilisateur (en entée)" )
      print( "r : réenvoi de la précédente commande" )
      print( "q : quit" )
      print( "" )
      print( "$01:<arg> : wifi bridge (reponse code 0x81) : Received argument is sent back to       " )
      print( "            Wifi module as an external AT command. The response is delayed. Execution " )
      print( "            result is sent with the response.                                         " )
      print( "$02:<arg> : SSID set (reponse code 0x82) : Received argument is used as new home      " )
      print( "            wifi SSID (stored in eeprom).                                             " )
      print( "$03:<arg> : Password set (reponse code 0x83) : Received argument is used as new       " )
      print( "            home wifi password (stored in eeprom).                                    " )
      print( "$04:      : Exit maintenance mode (reponse code 0x84) : Force the Wifi module to      " )
      print( "            leave maintenance mode.                                                   " )
      print( "$05:      : Get device name (reponse code 0x84) : device name is sent with the        " )
      print( "            response                                                                  " )
      print( "$10:<arg> : OpenEvse RAPI bridge (reponse code 0x90) : Received argument is sent      " )
      print( "            to OpenEVSE module as an external command. The response is delayed.       " )
      print( "            Execution result is sent with the response.                               " )
      print( "$11:      : Get charge information (response code 0x91) : Charge information are      " )
      print( "            sent with the response                                                    " )
      print( "$12:      : Get charge history (response code 0x92) : The 10 last charge states       " )
      print( "            are sent with the response (see ChargeState.c)                            " )
      print( "$13:      : Get CP line adc value (response code 0x93) : Adc Value is sent with       " )
      print( "            the response.                                                             " )
      print( "$7F:      : \"ScktFrame\" reset (response code 0xFF) : reset the \"ScktFrame\" state  " )
      print( "            <l_eFrmId>, in case of pending delayed response.                          " )




#---------------------------------------------------------------------------#

def PrintTerm( fLog, String ):
   """colorise les SocketFrames de commande et de réponses"""
   if fLog:
      fLog.write( String )

   String = re.sub( "(\$[0-7]\d\:)",  C_BOLD + C_BLUE + "\g<1>" + C_END, String )
   String = re.sub( "(\$[8-F]\d\:)",  C_BOLD + C_ORANGE + "\g<1>" + C_END, String )

   print( String )


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   DeviceIp = None

   parser = OptionParser()
   parser.add_option( "-i", "--ip", dest="DeviceIp", help="specify Ip address" )
   parser.add_option( "-l", "--log", dest="LogFile", help="specify log file" )
   parser.add_option( "-f", "--force", dest="Force", action="store_true",
                      help="force connection, event if bad response" )

   (options, args) = parser.parse_args()

   fLog = None
   if options.LogFile:
      try:
         fLog = open( options.LogFile, "w" )
      except IOError :
         print( "Unable to open log file" )
         sys.exit(1)

   SockWB = cSocketWB()
   if options.DeviceIp :
      DeviceIp = options.DeviceIp
      #if DeviceIp is not xxx.xxx.xxx.xxx:
      #   print( "socket.py <IpAdress>" )
      #   sys.exit(0)
      SockWB.Connect( DeviceIp, Force=options.Force )
   else :
      DeviceIp = SockWB.SearchAndConnect()


   queue_var = queue.Queue(10)
   lock = threading.Lock()

   thread = GetCmd(queue_var, lock)
   thread.start()

   while(not exitFlag) :
      try:
         StrToSend = queue_var.get_nowait()
         PrintTerm( fLog, "\r" + StrToSend )
         SockWB.Send( StrToSend + "\r\n" )
      except queue.Empty:
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

   del queue_var

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
   #//            print( "\t%s"%Line )
   #//   except:
   #//      pass
   #//
   #//print( "Close" )
   #//SockWB.Close()