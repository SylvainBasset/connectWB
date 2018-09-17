# -*- coding: Cp1252 -*-


import sys
import re
import subprocess
import socket
import msvcrt

DEFAULT_PORT = 15555
DEVICE_NAME = "ConnectWB"


class cSocketWB :

   #---------------------------------------------------------------------------#
   def __init__( self ):
      self.socket = None

   #---------------------------------------------------------------------------#
   def SearchDevice( self ):

      print "Scanning for device ..."

      deviceIp = None
      ArpRet = subprocess.check_output(["arp", "-a"])
      IpList = re.findall("\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}", ArpRet )[1:]

      for Ip in IpList :
         print Ip,
         sckt = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
         sckt.settimeout(0.5)
         try :
            sckt.connect( ( Ip, DEFAULT_PORT ) )
            sckt.settimeout(5)
            sckt.send( "$05:\r\n" )
            DeviceName = sckt.recv( 1024 ).strip()
            if DeviceName == "$85:%s"%DEVICE_NAME:
               deviceIp = Ip
               self.socket = sckt
               self.socket.setblocking(0)
            break
         except Exception as e:
            print e
            del sckt

      if not deviceIp:
         raise ValueError( "ConnectWB devices not found" )
      else :
         print ""
         print "device found at %s"%deviceIp

      return deviceIp


   #---------------------------------------------------------------------------#
   def Connect( self, HostIp, Port ):
      self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      print HostIp
      print Port
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

   HostIp = None

   if len(sys.argv) > 1 :
      HostIp = sys.argv[1]
      #if HostIp is not xxx.xxx.xxx.xxx:
      #   print "socket.py <IpAdress>"
      #   sys.exit(0)

   SockWB = cSocketWB()
   if not HostIp :
      HostIp = SockWB.SearchDevice()
   else :
      SockWB.Connect( HostIp )

   print "----------------"
   print "Socket connected"
   print "----------------"
   print ""

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


   print "Close"
   SockWB.socket.close()
