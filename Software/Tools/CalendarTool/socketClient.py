# -*- coding: Cp1252 -*-

import sys
import socket
import msvcrt
import time

class cConnectWB:
   def __init__( self ):
      pass

if __name__ == "__main__":

   if len(sys.argv) < 2:
      print "socket.py <IpAdress>"
      sys.exit(0)

   host = sys.argv[1]
   port = 15555

   sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   sock.connect((host, port))
   sock.setblocking(0)

   print "Connection on {}".format(port)

   #//msvcrt.getch()

   while True:

      if msvcrt.kbhit():
         Str = ""
         Char = msvcrt.getch()

         if Char == 'a':
            Str = u"$01:AT\r\n"
         if Char == 'z':
            Str = u"$01:AT+S.PING=192.168.0.1\r\n"
         if Char == 'e':
            Str = u"$02:\r\n"
         if Char == 'r':
            Str = u"$03:\r\n"
         if Char == 't':
            Str = u"$10:\r\n"

         if Char == 'p':
            Str = u"$42:\r\n"
         if Char == 'm':
            Str = u"$42:"

         if Char == 'c':
            break

         if Str != "":
            sock.send(Str)
            print "sent %s"%Str

      try:
         buf = sock.recv(1024)
         if len(buf) != 0:
            print '-----receive data-----'
            print buf,
            print '----------------------\n'
      except:
         pass



   #sock.shutdown(socket.SHUT_RDWR)

   #sock.connect((host, port))
   #sock.send(u"10:Test3.html")
   #sock.shutdown(socket.SHUT_RDWR)

   print "Close"
   sock.close()
