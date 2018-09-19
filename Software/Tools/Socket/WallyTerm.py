# -*- coding: Cp1252 -*-
#------------------------------------------------------------------------------#
# WallyTerm : Simple terminal for WallyBox
# Version : 0.1
#------------------------------------------------------------------------------#


import sys
import msvcrt
from WallySocket import cSocketWB


#---------------------------------------------------------------------------#
def GetCommand() :
   print "Cmd:",
   Str = raw_input()
   return Str


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   HostIp = None

   if len(sys.argv) > 1 :
      HostIp = sys.argv[1]
      #if HostIp is not xxx.xxx.xxx.xxx:
      #   print "socket.py <IpAdress>"
      #   sys.exit(0)

   SockWB = cSocketWB()
   if not HostIp :
      HostIp = SockWB.SearchAndConnect()
   else :
      SockWB.Connect( HostIp )

   LastSend = ""

   while True:
      if msvcrt.kbhit():
         StrToSend = ""
         Char = msvcrt.getch()
         if Char == 'a':
            StrToSend = u"$01:AT"
         if Char == 'p':
            StrToSend = u"$01:AT+S.PING=192.168.0.1"
         if Char == 's':
            StrToSend = GetCommand()
         if Char == 'r':
            StrToSend = LastSend
         if Char == 'q':
            break

         if StrToSend != "":
            LastSend = StrToSend
            print StrToSend
            SockWB.Send( StrToSend + "\r\n" )
      try:
         buf = SockWB.Receive()
         if len(buf) != 0:
            for Line in buf.split( '\r\n' ) :
               print "\t%s"%Line
      except:
         pass

   print "Close"
   SockWB.Close()