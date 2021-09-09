# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# WallyWeb : Search and open WallyBox webpage
# Version : 0.1
#------------------------------------------------------------------------------#

import os
from WallySocket import cSocketWB


#---------------------------------------------------------------------------#
if __name__ == "__main__" :

   SockWB = cSocketWB()
   Ip = SockWB.SearchAndConnect()
   SockWB.Close()

   StrCall = "sensible-browser http://%s"%Ip
   os.system( StrCall )