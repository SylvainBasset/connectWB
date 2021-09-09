# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# WallyUp : update Html filesystem
# Version : 0.1
#------------------------------------------------------------------------------#


import getpass
from WallySocket import cSocketWB
import socket
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
         print ( Line )

      if Error :
         raise ValueError( "command error" )


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   SockWB = cSocketWB()
   SockWB.SearchAndConnect()

   Ip = SockWB.GetLocalIp()

   Cmd = "$01:AT+S.HTTPDFSUPDATE=%s,/outfile.img\r\n"%Ip

   SendAndCheck( SockWB, Cmd)

   print( "Restart" )
   SendAndCheck( SockWB, "$04:\r\n", False)                  # ask for restart,
   time.sleep(1)

   SockWB.Close()