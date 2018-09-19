# -*- coding: Cp1252 -*-
#------------------------------------------------------------------------------#
# WallyTerm : SSID/pwd wifi setting
# Version : 0.1
#------------------------------------------------------------------------------#

import time
import getpass
from WallySocket import cSocketWB


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   SockWB = cSocketWB()
   SockWB.SearchAndConnect()

   ssid = raw_input('SSID=')
   pwd = getpass.getpass("enter pwd=")

   print ssid

   SockWB.Send( "$02:%s"%ssid )
   time.sleep(3)
   SockWB.Send( "$03:%s"%pwd )
   time.sleep(3)
   SockWB.Send( "$04:" )
   time.sleep(3)

   SockWB.Close()