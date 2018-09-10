# -*- coding: Cp1252 -*-

import sys
import socket
import msvcrt

DEFAULT_PORT = 15555


class cSocketWB :

   #---------------------------------------------------------------------------#
   def __init__( self ):
      self.socket = None

   #---------------------------------------------------------------------------#
   def SearchDevice( self ):
      raise ValueError( "ConnectWB devices not found" )

   #---------------------------------------------------------------------------#
   def Connect( self, HostIp=None, Port=None ):
      if not HostIp:
         HostIp = self.SearchDevice()   # Search for device in network
      if not Port :
         Port = DEFAULT_PORT

      self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      self.socket.connect((HostIp, Port))
      self.socket.setblocking(0)

   #---------------------------------------------------------------------------#
   def Send( self, StrData ):
      print "%s"%StrData
      StrData += '\r\n'
      self.socket.send(StrData)


#---------------------------------------------------------------------------#

def GetCommand() :
   print "Cmd:",
   Str = raw_input()
   return Str


#---------------------------------------------------------------------------#
if __name__ == "__main__":

   if len(sys.argv) < 2:
      print "socket.py <IpAdress>"
      sys.exit(0)

   #//print socket.gethostbyname_ex(socket.gethostname())
   #//print ([ip for ip in socket.gethostbyname_ex(socket.gethostname())[2]])

   HostIp = sys.argv[1]
   SockWB = cSocketWB()
   SockWB.Connect( HostIp )
   print "Socket connected"

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
            SockWB.Send( StrToSend )
      try:
         buf = SockWB.socket.recv(1024)
         if len(buf) != 0:
            for Line in buf.split( '\r\n' ) :
               print "\t%s"%Line
      except:
         pass


#//   print "Close"
#//   sock.close()
