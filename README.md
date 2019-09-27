
# WallyBox project

Wallybox is a connected EVSE (electrical vehicle supply equipment), allowing Wifi communication on home network. It also provides a simple HTTP dashboard, accessible by a web browser.

It involves a basic [OpenEVSE](https://www.openevse.com/) part for vehicle charging, and a NUCLEO-L053R8 board + X-NUCLEO-IDW01M1 daughter board for connectivity.



                     _______________                  ______________
                    |               | (serial Tx-Rx) |              |
                    | NUCLEO-L053R8 | <------------> |   OpenEVSE   | ====> EV Charging
                    |_______________|                |______________|
                             ^
                             | (serial Tx-Rx)
                     ________v______
     home    (Wifi) |               |
     network <----> |    IDW01M1    |
                    |_______________|


Main functionalities are :

 - Week calendar to allow/disable charging over time
 - Charging power/current setting
 - Charging status view

Moreover, it is possible to have full access to OpenEVSE RAPI protocol for debug purpose (the nucleo board act as a bridge in this case)

This project might evolve to add some interesting functionalities (not yet implemented):
 - Adapt charging allowance to electric network status (see [electricitymap](https://www.electricitymap.org/?lang=fr/)
 - Adapt power to house consumption (Maximum current per phase, ...)


**Note: this is also an example project, intended to test practical coding on STM32 and nucelo board.**