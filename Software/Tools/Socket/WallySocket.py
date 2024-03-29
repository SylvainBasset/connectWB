# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# WallySocket : Socket class for WallyBox communication
# Version : 0.1
#------------------------------------------------------------------------------#


import os
import re
import subprocess
import socket
#//import netifaces
#//import msvcrt

DEFAULT_PORT = 15555
DEVICE_NAME = "WallyBox"
LAST_IP_FILE = ".\LastIp.dat"

#------------------------------------------------------------------------------#
class cSocketWB :

   def __init__( self ):
      self.socket = None


   #---------------------------------------------------------------------------#
   def Connect( self, DeviceIp, Force=False, Port=None ):
      if Port == None :
         Port = DEFAULT_PORT
      sckt = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      sckt.connect((DeviceIp, Port))
      if ( not Force and not self.IsCorrectDevice( sckt )  ):
         raise ValueError( "unknown device")
      self.socket = sckt
      self.socket.setblocking(0)

      print( DeviceIp )
      print( Port )
      print( "----------------" )
      print( "Socket connected" )
      print( "----------------" )
      print( "" )

   #---------------------------------------------------------------------------#
   def SearchAndConnect( self ):

      print( "Scanning for devices" )

      DeviceIp = None

      IsMiniAp = self.IsWallyBoxMiniAp()
      if IsMiniAp :
         IpList = [self.GetGatewayIp()]

      else:
         HostIp = self.GetGatewayIp()
         if not HostIp :
            raise ValueError( "No connection" )
         BroadCastIp = HostIp.rsplit( ".", 1 )[0] + ".*"


         ArpRet = subprocess.check_output( ["nmap", "-sP", BroadCastIp] )
         IpList = re.findall("\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}", str( ArpRet ) )[1:]

         LastIp = self.GetLastIp()
         if LastIp in IpList:
            IpList.remove(LastIp)
            IpList.insert(0, LastIp)

      for Ip in IpList :
         print( Ip, )

         sckt = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
         sckt.settimeout(5)

         try :
            sckt.connect( ( Ip.encode('utf-8'), DEFAULT_PORT ) )
            if self.IsCorrectDevice( sckt ):
               DeviceIp = Ip
               break
            else:
               print( "Bad response" )
               del sckt
         except Exception as e:
            print( e )
            del sckt

      if not DeviceIp:
         raise ValueError( "%s device not found"%DEVICE_NAME )
      else :
         self.socket = sckt
         self.socket.setblocking(0)
         if not IsMiniAp:
            self.SaveLastIp( DeviceIp )
         print( "" )
         print( "device found at %s"%DeviceIp )

      print( DeviceIp )
      print( DEFAULT_PORT )
      print( "----------------" )
      print( "Socket connected" )
      print( "----------------" )
      print( "" )

      return DeviceIp


   #---------------------------------------------------------------------------#
   def IsCorrectDevice( self, sckt ):
      sckt.settimeout(5)
      sckt.send( "$05:\r\n".encode('utf-8') )
      DeviceName = sckt.recv( 1024 ).strip()
      if DeviceName.decode("utf-8") == "$85:%s"%DEVICE_NAME:
         return True
      else:
         return False


   #---------------------------------------------------------------------------#
   def GetLastIp( self ):
      LastIp = ""
      if os.path.isfile( LAST_IP_FILE ) :
         try:
            f = open( LAST_IP_FILE, "r")
            LastIp = f.read().strip()
         except:
            pass
      return LastIp

   #---------------------------------------------------------------------------#
   def SaveLastIp( self, LastIp ):
      try:
         f = open( LAST_IP_FILE, "w")
         LastIp = f.write( LastIp )
         f.close()
      except:
         pass

   #---------------------------------------------------------------------------#
   def IsWallyBoxMiniAp( self ) :
      iwconfigRet = subprocess.check_output( ["iwconfig"] )

      if re.search("ESSID.*:.*WallyBox_Maint", str( iwconfigRet ) ) :
         return True
      else:
         return False

   #---------------------------------------------------------------------------#
   def GetGatewayIp( self ) :
      ipRet = subprocess.check_output( ["ip","r"] )
      match = re.search("default[A-Za-z\s]*(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})", str( ipRet ) )

      if match :
         gatewayIp = match.groups()[0]
      else:
         gatewayIp = None
      return gatewayIp

   #---------------------------------------------------------------------------#
   def GetLocalIp( self )  :
      s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      s.connect(("8.8.8.8", 80))
      Ip = s.getsockname()[0]
      s.close()
      return Ip

   #---------------------------------------------------------------------------#
   def Close( self ):
      self.socket.close()


   #---------------------------------------------------------------------------#
   def Receive( self, TimeOut=None ):
      if TimeOut :
          self.socket.settimeout(TimeOut)
      Data = self.socket.recv(1024)
      if TimeOut :
          self.socket.setblocking(0)

      return Data


   #---------------------------------------------------------------------------#
   def Send( self, StrData ):
      #//StrData += '\r\n'
      self.socket.send(StrData.encode('utf-8'))