# -*- coding: Cp1252 -*-
#------------------------------------------------------------------------------#
# WallyTerm : SSID/pwd wifi setting
# Version : 0.1
#------------------------------------------------------------------------------#


import getpass
from WallySocket import cSocketWB


#---------------------------------------------------------------------------#
def SendAndCheck( SockWB, StrCmd ):
   SockWB.Send( StrCmd )
   Rec = SockWB.Receive( 3 ).lower()
   if "err" in Rec:
      raise ValueError( "Wifi setting error" )


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   SockWB = cSocketWB()
   SockWB.SearchAndConnect()

   ssid = raw_input('SSID=')
   pwd = getpass.getpass("enter pwd=")

   print ssid

   SendAndCheck( SockWB, "$02:%s"%ssid )
   SendAndCheck( SockWB, "$03:%s"%pwd )
   SendAndCheck( SockWB, "$04:" )                  # ask for restart,

   SockWB.Close()