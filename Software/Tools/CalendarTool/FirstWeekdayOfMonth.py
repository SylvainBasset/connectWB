# -*- coding: Cp1252 -*-


import datetime


#------------------------------------------------------------------------------
def GotoNextFirstDay( date ):
   date += datetime.timedelta(days=1)
   while ( date.day != 1 ):
      date += datetime.timedelta(days=1)
   return date

#------------------------------------------------------------------------------
if __name__ == '__main__':

   f = open("FirstWeekdayOfMonth.txt","w")

   MemWeekday = None

   date = datetime.datetime(2000, 01, 01)

   CntItem = 0
   CntLine = 0

   while ( date.year < 2100 ):
      Weekday1 = date.weekday()

      if CntItem == 0 :
         MemFirstLineDate = date

      date = GotoNextFirstDay( date )

      f.write( "CB(%d,%d), "%(Weekday1, date.weekday()) )

      CntItem += 1
      if ( CntItem == 9 ):

         f.write( "/* %d %d/%04d */\n"%((CntLine*9), MemFirstLineDate.month, MemFirstLineDate.year) )

         CntLine += 1
         CntItem = 0

      date = GotoNextFirstDay( date )

   if ( CntItem != 9 ):
      f.write( "/* %d %d/%04d */\n"%((CntLine*9), MemFirstLineDate.month, MemFirstLineDate.year) )

f.close()

