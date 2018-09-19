# -*- coding: Cp1252 -*-
#------------------------------------------------------------------------------#
# WallySocket : Socket class for WallyBox communication
# Version : 0.1
#------------------------------------------------------------------------------#


import re
import subprocess
import socket
import netifaces
from pprint import pprint
import msvcrt

DEFAULT_PORT = 15555
DEVICE_NAME = "WallyBox"


#------------------------------------------------------------------------------#
class cSocketWB :

   def __init__( self ):
      self.socket = None


   #---------------------------------------------------------------------------#
   def Connect( self, HostIp, Port=None ):
      if Port == None :
         Port = DEFAULT_PORT
      self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      print HostIp
      print Port
      self.socket.connect((HostIp, Port))
      self.socket.setblocking(0)

      print "----------------"
      print "Socket connected"
      print "----------------"
      print ""

   #---------------------------------------------------------------------------#
   def SearchAndConnect( self ):

      print "Scanning for device ..."

      deviceIp = None

      IsMiniAp = self.IsWallyBoxMiniAp()
      if IsMiniAp :
         IpList = [self.GetGatewayIp()]
         print IpList
      else:
         #try:
         #   f = open( "LastIp.dat", "r")

         subprocess.call( "ping -n 1 192.168.1.255", stdout=subprocess.PIPE )
         ArpRet = subprocess.check_output( ["arp", "-a"] )
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
            else:
               print "Bad response"
               del sckt
         except Exception as e:
            print e
            del sckt

      if not deviceIp:
         raise ValueError( "%s device not found"%DEVICE_NAME )
      else :
         print ""
         print "device found at %s"%deviceIp

      print "----------------"
      print "Socket connected"
      print "----------------"
      print ""
      return deviceIp

   #---------------------------------------------------------------------------#
   def IsWallyBoxMiniAp( self ) :

      NetshRet = subprocess.check_output( ["netsh", "wlan", "show", "interface"] )
      if re.search("SSID.*:.*WallyBox_Maint", NetshRet ) :
         return True
      else:
         return False

   #---------------------------------------------------------------------------#
   def GetGatewayIp( self ) :
      gateway = netifaces.gateways()['default']
      gatewayIp = gateway[gateway.keys()[0]][0]
      return gatewayIp

   #---------------------------------------------------------------------------#
   def Close( self ):
      self.socket.close()


   #---------------------------------------------------------------------------#
   def Receive( self ):
      Data = self.socket.recv(1024)
      return Data


   #---------------------------------------------------------------------------#
   def Send( self, StrData ):
      #//StrData += '\r\n'
      self.socket.send(StrData)