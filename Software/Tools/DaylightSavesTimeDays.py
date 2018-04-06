# -*- coding: Cp1252 -*-

import datetime


def GetDays( f, Month ):
   date = datetime.datetime(2000, 01, 01)

   while (date.year < 2100):
      if ( date.month == Month and date.weekday() == 6 ):
         LastSunday = date
      if ( date.month == Month+1 and date.day == 1 ):
         f.write( "%02d,"%(LastSunday.day) )
      date += datetime.timedelta(days=1)

#---------------------------------------------------------------------------#

if __name__ == '__main__':


   f = open("SpringTimeDays.txt","w")
   GetDays( f, 3 )
   f.close()

   f = open("AutumnTimeDays.txt","w")
   GetDays( f, 10 )
   f.close()