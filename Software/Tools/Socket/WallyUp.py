# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# WallyUp : update Html filesystem
# Version : 0.1
#------------------------------------------------------------------------------#


import getpass
from WallySocket import cSocketWB
import socket


#---------------------------------------------------------------------------#
def SendAndCheck( SockWB, StrCmd, Check = True ):

   SockWB.Send( StrCmd )
   if Check :
      Rec = SockWB.Receive( 1000 ).lower()
      print Rec
      if "err" in Rec:
         raise ValueError( "setting error" )


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   SockWB = cSocketWB()
   SockWB.SearchAndConnect()

   Ip = SockWB.GetLocalIp()

   Cmd = "$01:AT+S.HTTPDFSUPDATE=%s,/outfile.img"%Ip

   SendAndCheck( SockWB, Cmd)
   SendAndCheck( SockWB, "$04:", False)                  # ask for restart,

   SockWB.Close()