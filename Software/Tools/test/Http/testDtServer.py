# -*- coding: Utf-8 -*-
#------------------------------------------------------------------------------#
# testDtServer : Test response from date/time server
# Version : 0.1
#------------------------------------------------------------------------------#


import requests


r = requests.get("http://192.168.1.16")
print(r.text)