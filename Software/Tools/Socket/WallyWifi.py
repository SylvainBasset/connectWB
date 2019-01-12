# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# WallyWifi : SSID/pwd wifi setting
# Version : 0.1
#------------------------------------------------------------------------------#


import getpass
from WallySocket import cSocketWB
import time

#---------------------------------------------------------------------------#
def SendAndCheck( SockWB, StrCmd, Check = True ):

   SockWB.Send( StrCmd )

   if Check :
      Error = False
      buf = ""
      while( True ) :
         buf =  buf + SockWB.Receive(60)
         if ":OK" in buf :
            break
         if ":ERR" in buf :
            Error = True
            break ;

      for Line in buf.splitlines() :
         print Line

      if Error :
         raise ValueError( "command error" )


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   SockWB = cSocketWB()
   SockWB.SearchAndConnect()

   ssid = raw_input('SSID=')
   pwd = getpass.getpass("enter pwd=")

   print ssid
   print pwd

   SendAndCheck( SockWB, "$02:%s\r\n"%ssid )
   SendAndCheck( SockWB, "$03:%s\r\n"%pwd )

   print "Restart"
   SendAndCheck( SockWB, "$04:\r\n", False)                  # ask for restart,
   time.sleep(1)

   SockWB.Close()