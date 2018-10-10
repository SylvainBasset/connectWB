/******************************************************************************/
/*                                                                            */
/*                                 UartWifi.c                                 */
/*                                                                            */
/******************************************************************************/
/* Created on:   08 apr. 2018   Sylvain BASSET        Version 0.1             */
/* Modifications:                                                             */
/******************************************************************************/


#include "Define.h"
#include "Communic.h"
#include "System.h"
#include "System/Hard.h"


/*
Module description:
//TBD
   - uilisation de l'USART 1
   - Gestion hard flow RTS/CTS
   - Gestion détection erreur de bruit, de débordement, et d'erreur de trames (a détailler)
   - Détection d'erreur activable/désactivable
   - DMA utilisé en emission et en récpetion
   - En emission:
      . configuration du channel DMA sur le buffer à mettre
      . indication transfert en cours et si nouvelle demande alors la
        fonction Send() renvoie FALSE
      . interruption en fin de transfert pour autoriser nouvelle transmission
      . détection erreurs DMA
   - En réception:
      . configuration du channel DMA sur le buffer de lecture "l_byRxBuffer" en
        mode circulaire
      . pointeur l_wRxIdxIn pour écriture dans buffer et l_wRxIdxOut pour lecture
        dans buffer
      . interruption DMA sur fin demi-transfert et fin transfert. Si pas assez
        de place libre pour demi transfert suivant alors on suspend la réception
        (arrêt DMA et donc mise à 1 de RTS)
      . Fonction Read() pour lecture n octets. Si on est en réception suspendue
        et que la lecture permet de libérer assez de place, alors la réception
        est a nouveau autorisée.
      . Il aurait été aussi possible d'activer l'inerruption DMA à chaque transfert
        d'octet (permettant une gestion plus fine du buffer de réception). Mais
        cette méthode d'interruption demi/fin transfert permet de réduire le
        taux d'occupation CPU, (au prix d'une consommation de RAM un peu plus
        importante)
*/


/*----------------------------------------------------------------------------*/
/* Defines                                                                    */
/*----------------------------------------------------------------------------*/

#define UWIFI_BAUDRATE     115200llu   /* baudrate (bits per second) value  */
#define UWIFI_RXBUFSIZE    1024        /* size of reception buffer */

                                       /* disable/suspend reception channel DMA */
#define UWIFI_DISABLE_DMA_RX()     ( UWIFI_DMA_RX->CCR &= ~DMA_CCR_EN )
                                       /* enable reception channel DMA */
#define UWIFI_ENABLE_DMA_RX()      ( UWIFI_DMA_RX->CCR |= DMA_CCR_EN )
                                       /* disable/suspend transmit channel DMA */
#define UWIFI_DISABLE_DMA_TX()     ( UWIFI_DMA_TX->CCR &= ~DMA_CCR_EN )
                                       /* enable transmit channel DMA */
#define UWIFI_ENABLE_DMA_TX()      ( UWIFI_DMA_TX->CCR |= DMA_CCR_EN )


/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static void uwifi_HrdInit( void ) ;
static void wifi_DmaTxIrqHandle( void ) ;
static void wifi_DmaRxIrqHandle( void ) ;
static BOOL uwifi_IsNeedRxSuspend( void ) ;


/*----------------------------------------------------------------------------*/
/* Variables                                                                  */
/*----------------------------------------------------------------------------*/

BYTE l_byRxBuffer [UWIFI_RXBUFSIZE] ;  /* circular reception buffer */
WORD l_wRxIdxIn ;                      /* input index of reception buffer  */
WORD l_wRxIdxOut ;                     /* output index of reception buffer */
BOOL l_bRxSuspend ;                    /* suspended reception indicator */

BOOL l_bTxPending ;                    /* transmission DMA transfer is ongoing */
BOOL l_byErrors ;                      /* errors status, cf. UWIFI_ERROR_xxx */


/*----------------------------------------------------------------------------*/
/* initialization of UART communication                                       */
/*----------------------------------------------------------------------------*/

void uwifi_Init( void )
{
                                       /* clear reception buffer */
   memset( l_byRxBuffer, 0, sizeof(l_byRxBuffer) ) ;
   l_wRxIdxIn = 0 ;                    /* set RX input index to 0 */
   l_wRxIdxOut = 0 ;                   /* set RX output index to 0 */
   l_bRxSuspend = FALSE ;              /* reception is active */

   l_bTxPending = 0 ;                  /* no TX DMA transfer */
   l_byErrors = 0 ;                    /* no errors */

   uwifi_HrdInit() ;                   /* WIFI UART hardware initialization */
}


/*----------------------------------------------------------------------------*/
/* Send Data to Wifi module                                                   */
/*    - <i_pvData> transmit data buffer                                        */
/*    - <i_dwSize> number of bytes to send                                    */
/* Return:                                                                    */
/*    - Transfer acceptation:                                                 */
/*       . TRUE : data transfer to wifi module is accpted                     */
/*       . FALSE : data transfer to wifi module is denied, as previous        */
/*                 transfer is not complete                                   */
/*----------------------------------------------------------------------------*/

BOOL uwifi_Send( void const* i_pvData, DWORD i_dwSize )
{
   BOOL bRet ;

   if ( ! l_bTxPending )            /* no TX transfer is pending */
   {
      l_bTxPending = TRUE ;         /* Start of TX transfer */

      UWIFI->ICR |= USART_ICR_TCCF ; /* clear UART transmission complete flag */
                                    /* set the size of transfer */
      UWIFI_DMA_TX->CNDTR = i_dwSize ;
                                    /* set periferal address (USART TDR register) */
      UWIFI_DMA_TX->CPAR = (DWORD)&(UWIFI->TDR) ;
                                    /* set memory address (transmit buffer) */
      UWIFI_DMA_TX->CMAR = (DWORD)i_pvData ;
      UWIFI_ENABLE_DMA_TX() ;       /* enable TX DMA channel */

      bRet = TRUE ;                 /* transfer accepted */
   }
   else
   {
      bRet = FALSE ;                /* transfer denied */
   }

   return bRet ;
}


/*----------------------------------------------------------------------------*/
/* Check if last transmit is completed                                        */
/* Return:                                                                    */
/*    - transmission status:                                                  */
/*       . TRUE : transfert completed                                         */
/*       . FALSE : transfert is still pending                                 */
/*----------------------------------------------------------------------------*/

BOOL uWifi_IsSendDone( void )
{
   BOOL bRet ;

   bRet = ! l_bTxPending ;            /* get transmission status */

   return bRet ;
}


/*----------------------------------------------------------------------------*/
/* Read data received from Wifi module                                        */
/*    - <o_pbyData> reception buffer address                                  */
/*    - <i_dwMaxSize> maximum size to read in bytes                           */
/*    - <i_bGetPending> limit the read to data between two CR+LF characters.  */
/*                      If this option is set and the condition is meet, only */
/*                      the data placed between CR+LF characters are copied   */
/*                      to <o_pbyData>. Any data before first CR+LF character */
/*                      is discarded as well as CR+LF charaters themselves.   */
/*                      If the condition is not meet (0 or 1 CR+LS found      */
/*                      before end of read) the read size is set to 0 (no     */
/*                      read).                                                */
/* Return:                                                                    */
/*    - actual size of read data in bytes                                     */
/*----------------------------------------------------------------------------*/

WORD uwifi_Read( BYTE * o_pbyData, WORD i_dwMaxSize, BOOL i_bGetPending )     // reprendre comment pour i_bGetPending
{
   WORD wSizeRead ;
   BYTE * pbyDataBuf ;
   BYTE * pbyDataOut ;
   BYTE byData ;
   BYTE byPrevData ;
   WORD wIdx ;
   BOOL bCRLFFound ;
   BOOL bNeedSuspendRx ;

                                       /* disable interruption to prevent data corruption */
   HAL_NVIC_DisableIRQ( UWIFI_DMA_IRQn ) ;
                                       /* update input buffer index */
   l_wRxIdxIn = sizeof(l_byRxBuffer) - UWIFI_DMA_RX->CNDTR ;

   if ( l_wRxIdxIn < l_wRxIdxOut )     /* if input index is below output index */
   {                                   /* compute size with overflow */
      wSizeRead = sizeof(l_byRxBuffer) - ( l_wRxIdxOut - l_wRxIdxIn ) ;
   }
   else                                /* input index is above (or equal) output index */
   {                                   /* compute size without overflow */
      wSizeRead = l_wRxIdxIn - l_wRxIdxOut ;
   }
                                       /* if unread data number is higher than */
                                       /* requested (0 end of string included) */
   if ( wSizeRead > ( i_dwMaxSize - 1 ) )
   {
      wSizeRead = ( i_dwMaxSize - 1 ) ; /* limit size to requested value */
   }
                                       /* data pointer initialization */
   pbyDataBuf = &l_byRxBuffer[l_wRxIdxOut] ;
   pbyDataOut = o_pbyData ;
   byPrevData = 0 ;                    /* no previous data */
   bCRLFFound = FALSE ;
                                       /* loop over data to read */
   for ( wIdx = 0 ; wIdx < wSizeRead ; wIdx++ )
   {
      byData = *pbyDataBuf ;           /* get data value */
      *pbyDataOut = byData ;
      pbyDataOut++ ;
                                       /* if buffer overflow */
      if ( pbyDataBuf == &l_byRxBuffer[sizeof(l_byRxBuffer) - 1] )
      {
         pbyDataBuf = l_byRxBuffer ;   /* set data pointer to first element */
      }
      else
      {
         pbyDataBuf++ ;                /* next data */
      }
                                       /* if current char is 'LF' and */
                                       /* previous one is 'CR' */
      if ( ( byData == 0x0A ) && ( byPrevData == 0x0D ) )
      {
         bCRLFFound = TRUE ;           /* set CR/LF flag */
         wIdx++ ;                      /* set pointer on next char */
         break ;
      }
      byPrevData = byData ;            /* actual data becomes previous */
   }

   if ( i_bGetPending )                /* if pre-read data without getting it */
   {
      wSizeRead = wIdx ;               /* get read size */
      *pbyDataOut = '\0' ;             /* end of string */
   }
   else
   {
      if ( ! bCRLFFound )              /* CR/LF not found */
      {
         wSizeRead = 0 ;               /* discard already read characters */
         o_pbyData[0] = '\0' ;         /* no data */
      }
      else
      {                                /* set new out-index value */
         l_wRxIdxOut = ( l_wRxIdxOut + wIdx ) % sizeof(l_byRxBuffer) ;
         wSizeRead = wIdx ;            /* get read size */
         *pbyDataOut = '\0' ;          /* end of string */
      }
   }
                                       /* test if RX suspention is needed */
   bNeedSuspendRx = uwifi_IsNeedRxSuspend() ;
                                       /* if suspention is not more needed */
   if ( l_bRxSuspend & ! bNeedSuspendRx )
   {
      l_bRxSuspend = FALSE ;           /* clear suspension flag */
      UWIFI_ENABLE_DMA_RX() ;          /* re-activate RX DMA channel */
   }
                                       /*re-activate interrupts */
   HAL_NVIC_EnableIRQ( UWIFI_DMA_IRQn ) ;

   return wSizeRead ;
}


/*----------------------------------------------------------------------------*/
/* Enable/Disable error detection                                             */
/*    - <i_bEnable> enable status:                                            */
/*       . TRUE : enable detection                                            */
/*       . FALSE : disable detection                                          */
/* Note : Hardware errors may be raised by USART or DMAs modules. They        */
/* trigger interrupts if their corresponding interrupt enable bit is set.     */
/* Errors are analysed in USART and DMAs interrupt, and collected into        */
/* <l_byError> status variable                                                */
/*----------------------------------------------------------------------------*/

void uwifi_SetErrorDetection( BOOL i_bEnable )
{
   if ( i_bEnable )
   {
      l_byErrors = 0 ;                 /* clear error status variable */
                                       /* clear all USART error flags */
      UWIFI->ICR |= ( USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF ) ;
                                       /* clear DMA TX error flag */
      UWIFI_DMA->IFCR = UWIFI_DMA_TX_ISRIFCR( DMA_IFCR_CTEIF1 ) ;
                                       /* clear DMA RX error flag */
      UWIFI_DMA->IFCR = UWIFI_DMA_RX_ISRIFCR( DMA_IFCR_CTEIF1 ) ;

      UWIFI->CR3 |= USART_CR3_EIE ;    /* enable USART overrun/noise/frame errors interrupt */
      UWIFI->CR1 |= USART_CR1_PEIE ;   /* enable USART parity error interrupt */
                                       /* enable DMA TX error interrupt */
      UWIFI_DMA_TX->CCR |= DMA_CCR_TEIE ;
                                       /* enable DMA RX error interrupt */
      UWIFI_DMA_RX->CCR |= DMA_CCR_TEIE ;
   }
   else
   {
      UWIFI->CR3 &= ~USART_CR3_EIE ;   /* disable USART overrun/noise/frame errors interrupt */
      UWIFI->CR1 &= ~USART_CR1_PEIE ;  /* disable USART parity error interrupt */
                                       /* disable DMA TX error interrupt */
      UWIFI_DMA_TX->CCR &= ~DMA_CCR_TEIE ;
                                       /* disable DMA RX error interrupt */
      UWIFI_DMA_RX->CCR &= ~DMA_CCR_TEIE ;
   }
}


/*----------------------------------------------------------------------------*/
/* Read error status                                                          */
/*    - <i_bReset> ask for error reset :                                      */
/*       . TRUE : reset errors status                                         */
/*       . FALSE : keep previous errors                                       */
/* Return :                                                                   */
/*    - error status, cf. UWIFI_ERROR_xxx                                     */
/*----------------------------------------------------------------------------*/

BYTE uwifi_GetError( BOOL i_bReset )
{
   BYTE byErrors ;

   byErrors = l_byErrors ;             /* read error status */

   if ( i_bReset )
   {
      l_byErrors = 0 ;                 /* reset errors if needed */
   }

   return byErrors ;                   /* return errors */
}


/*----------------------------------------------------------------------------*/
/* Wifi USART interrupt                                                       */
/*----------------------------------------------------------------------------*/

void UWIFI_IRQHandler( void )
{
   DWORD dwErrorFlag ;
                                       /* tested errors are "Parity error",  */
                                       /* "Framing error", "Noise error", "OverRun error" */
   dwErrorFlag = USART_ISR_PE | USART_ISR_FE | USART_ISR_ORE | USART_ISR_NE ;
                                       /* if one error bit is set */
   if ( ( UWIFI->ISR & dwErrorFlag ) != 0 )
   {                                   /* clear error bits */
      UWIFI->ICR |= ( USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF ) ;

      l_byErrors |= UWIFI_ERROR_RX ;   /* set RX UASRT error */
   }
}


/*----------------------------------------------------------------------------*/
/* DMA interrupt interrupt                                                    */
/* Note : this interrupt is raised for both TX and RX channel                 */
/*----------------------------------------------------------------------------*/

void UWIFI_DMA_IRQHandler( void )
{
   wifi_DmaTxIrqHandle() ;             /* process TX DMA interrupt */
   wifi_DmaRxIrqHandle() ;             /* process RX DMA interrupt */
}


/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* Hardware initialization                                                    */
/*----------------------------------------------------------------------------*/

static void uwifi_HrdInit( void )
{
   GPIO_InitTypeDef sGpioInit ;

      /* configure TX pin as alternate, no push/pull, high freq */
   WIFI_TX_GPIO_CLK_ENABLE() ;         /* start TX gpio clock */
   sGpioInit.Pin = WIFI_TX_PIN ;
   sGpioInit.Mode = GPIO_MODE_AF_PP ;
   sGpioInit.Pull = GPIO_NOPULL ;
   sGpioInit.Speed = GPIO_SPEED_FREQ_HIGH ;
   sGpioInit.Alternate = WIFI_TX_AF ;
   HAL_GPIO_Init( WIFI_TX_GPIO_PORT, &sGpioInit ) ;

      /* configure RX pin as alternate, no push/pull, high freq */
   WIFI_RX_GPIO_CLK_ENABLE() ;         /* start RX gpio clock */
   sGpioInit.Pin = WIFI_RX_PIN ;
   sGpioInit.Alternate = WIFI_RX_AF ;
   HAL_GPIO_Init( WIFI_RX_GPIO_PORT, &sGpioInit ) ;

      /* configure RTS pin as alternate, no push/pull, high freq */
   WIFI_RTS_GPIO_CLK_ENABLE() ;        /* start RTS gpio clock */
   sGpioInit.Pin = WIFI_RTS_PIN ;
   sGpioInit.Alternate = WIFI_RTS_AF ;
   HAL_GPIO_Init( WIFI_RTS_GPIO_PORT, &sGpioInit ) ;

      /* configure CTS pin as alternate, no push/pull, high freq */
   WIFI_CTS_GPIO_CLK_ENABLE() ;        /* start CTS gpio clock */
   sGpioInit.Pin = WIFI_CTS_PIN ;
   sGpioInit.Alternate = WIFI_CTS_AF ;
   HAL_GPIO_Init( WIFI_CTS_GPIO_PORT, &sGpioInit ) ;

   /* -------- USART -------- */

   UWIFI_CLK_ENABLE() ;                /* enable USART clock */

   UWIFI_FORCE_RESET() ;               /* reset USART  */
   UWIFI_RELEASE_RESET() ;
                                       /* activate emission and reception */
   UWIFI->CR1 |= ( USART_CR1_TE | USART_CR1_RE ) ;
                                       /* activate DMA and RTS/CTS management */
   UWIFI->CR3 |= ( USART_CR3_DMAR | USART_CR3_DMAT | USART_CR3_RTSE | USART_CR3_CTSE ) ;
                                       /* set baudrate */
   UWIFI->BRR = (uint16_t)( UART_DIV_SAMPLING16( APB1_CLK, UWIFI_BAUDRATE ) ) ; //SBA: APB2

   UWIFI->CR1 |= USART_CR1_UE ;        /* enable USART */

   UWIFI->RDR ;                        /* read input register to avoid unwanted data */
                                       /* reset errors */
   UWIFI->ICR |= ( USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF ) ;
                                       /* set USART interrupt priority level */
   HAL_NVIC_SetPriority( UWIFI_IRQn, UWIFI_IRQPri, 0 ) ;
   HAL_NVIC_EnableIRQ( UWIFI_IRQn ) ;  /* enable USART interrupt */

   /* -------- DMA -------- */

   UWIFI_DMA_CLK_ENABLE() ;            /* enable DMA clock */
                                       /* set direction mem-to-perif, memory increment, */
                                       /* enable transfer complete and error interrupt */
   UWIFI_DMA_TX->CCR = ( DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TEIE | DMA_CCR_TCIE ) ;
                                       /* set TX request source */
   UWIFI_DMA_CSELR->CSELR &= ~UWIFI_DMA_TX_CSELR( DMA_CSELR_C1S_Msk ) ;
   UWIFI_DMA_CSELR->CSELR |= UWIFI_DMA_TX_CSELR( UWIFI_DMA_TX_REQ ) ;

                                       /* direction perif-to-mem, memory increment, */
                                       /* enable half-transfer, transfer-complete */
                                       /* and error interrupt */
   UWIFI_DMA_RX->CCR = ( DMA_CCR_MINC | DMA_CCR_CIRC |
                         DMA_CCR_TEIE | DMA_CCR_HTIE | DMA_CCR_TCIE ) ;
                                       /* set RX request source */
   UWIFI_DMA_CSELR->CSELR &= ~UWIFI_DMA_RX_CSELR( DMA_CSELR_C1S_Msk ) ;
   UWIFI_DMA_CSELR->CSELR |= UWIFI_DMA_RX_CSELR( UWIFI_DMA_RX_REQ ) ;
                                       /* set RX transfer size (equals RX buffer size) */
   UWIFI_DMA_RX->CNDTR = sizeof(l_byRxBuffer) ;
                                       /* set periferal address (USART RDR register) */
   UWIFI_DMA_RX->CPAR = (DWORD)&(UWIFI->RDR) ;
                                       /* set memory address (reception buffer) */
   UWIFI_DMA_RX->CMAR = (DWORD)l_byRxBuffer ;
                                       /* set DMA interrupt priority level */
   HAL_NVIC_SetPriority( UWIFI_DMA_IRQn, UWIFI_DMA_IRQPri, 0 ) ;
                                       /* enable USART interrupt */
   HAL_NVIC_EnableIRQ( UWIFI_DMA_IRQn ) ;

   UWIFI_ENABLE_DMA_RX() ;             /* enable RX DMA channel */
}


/*----------------------------------------------------------------------------*/
/* Test and management of TX DMA interrupt                                    */
/*----------------------------------------------------------------------------*/

static void wifi_DmaTxIrqHandle( void )
{
   DWORD dwIsrVal ;

   dwIsrVal = UWIFI_DMA->ISR ;         /* read DMA interrupt flags status */
                                       /* if TX global interrupt flag is set */
   if ( ISSET( dwIsrVal, UWIFI_DMA_TX_ISRIFCR( DMA_ISR_GIF1 ) ) )
   {                                   /* clear global interrupt flag */
      UWIFI_DMA->IFCR = UWIFI_DMA_TX_ISRIFCR( DMA_IFCR_CGIF1 ) ;
                                       /* if end of transfer interrupt */
      if ( ISSET( dwIsrVal, UWIFI_DMA_TX_ISRIFCR( DMA_ISR_TCIF1 ) ) )
      {                                /* clear end of tranfser flag */
         UWIFI_DMA->IFCR = UWIFI_DMA_TX_ISRIFCR( DMA_IFCR_CTCIF1 ) ;
         UWIFI_DISABLE_DMA_TX() ;      /* stop TX DMA channel */
         l_bTxPending = FALSE ;        /* current tranfer is done */
      }
                                       /* if tranfser error interrupt */
      if ( ISSET( dwIsrVal, UWIFI_DMA_TX_ISRIFCR( DMA_ISR_TEIF1 ) ) )
      {                                /* clear error flag */
         UWIFI_DMA->IFCR = UWIFI_DMA_TX_ISRIFCR( DMA_IFCR_CTEIF1 ) ;
                                       /* set DMA TX error */
         l_byErrors |= UWIFI_ERROR_DMA_TX ;
      }
   }
}


/*----------------------------------------------------------------------------*/
/* Test and management of RX DMA interrupt                                    */
/*----------------------------------------------------------------------------*/

static void wifi_DmaRxIrqHandle( void )
{
   DWORD dwIsrVal ;
   BOOL bNeedCheck ;
   BOOL bNeedSuspendRx ;

   dwIsrVal = UWIFI_DMA->ISR ;         /* read DMA interrupt flags status */
                                       /* if RX global interrupt flag is set */
   if ( ISSET( dwIsrVal, UWIFI_DMA_RX_ISRIFCR( DMA_ISR_GIF1 ) ) )
   {                                   /* clear global interrupt flag */
      UWIFI_DMA->IFCR = UWIFI_DMA_RX_ISRIFCR( DMA_IFCR_CGIF1 ) ;

      bNeedCheck = FALSE ;
                                       /* if end of transfer interrupt */
      if ( ISSET( dwIsrVal, UWIFI_DMA_RX_ISRIFCR( DMA_ISR_TCIF1 ) ) )
      {                                /* clear end of tranfser flag */
         UWIFI_DMA->IFCR = UWIFI_DMA_RX_ISRIFCR( DMA_IFCR_CTCIF1 ) ;
         bNeedCheck = TRUE ;           /* check of index pointers is needed */
      }
                                       /* if half transfer interrupt */
      if ( ISSET( dwIsrVal, UWIFI_DMA_RX_ISRIFCR( DMA_ISR_HTIF1 ) ) )
      {                                /* clear half tranfser flag */
         UWIFI_DMA->IFCR = UWIFI_DMA_RX_ISRIFCR( DMA_IFCR_CHTIF1 ) ;
         bNeedCheck = TRUE ;           /* check of index pointers is needed */
      }

      if ( bNeedCheck )
      {                                /* update input index */
         l_wRxIdxIn = sizeof(l_byRxBuffer) - UWIFI_DMA_RX->CNDTR ;
                                       /* get suspend status */
         bNeedSuspendRx = uwifi_IsNeedRxSuspend() ;
         if ( bNeedSuspendRx )         /* if RX suspend is required */
         {
            UWIFI_DISABLE_DMA_RX() ;   /* suspend RX DMA channel */
            l_bRxSuspend = TRUE ;      /* indicate RX is suspended */
         }
      }
                                       /* if transfer error interrupt */
      if ( ISSET( dwIsrVal, UWIFI_DMA_RX_ISRIFCR( DMA_ISR_TEIF1 ) ) )
      {                                /* clear error flag */
         UWIFI_DMA->IFCR = UWIFI_DMA_RX_ISRIFCR( DMA_IFCR_CTEIF1 ) ;
                                       /* set DMA RX error */
         l_byErrors |= UWIFI_ERROR_DMA_RX ;
      }
   }
}


/*----------------------------------------------------------------------------*/
/* Test if suspension of RX transfer is needed                                */
/* Return :                                                                   */
/*    - Suspension status :                                                   */
/*       . TRUE : data reception must be suspended                            */
/*       . FALSE : data reception can be processed, as reception buffer is    */
/*                 empty enough until next interrupt (half buffer size)       */
/*----------------------------------------------------------------------------*/

static BOOL uwifi_IsNeedRxSuspend( void )
{
   BOOL bNeedSuspendRx ;

   bNeedSuspendRx = FALSE ;            /* no halt by default */

   if ( l_wRxIdxIn < l_wRxIdxOut )     /* if input index is below output index */
   {                                   /* if free space is below half-buffer */
      if ( ( l_wRxIdxOut - l_wRxIdxIn ) <= ( sizeof(l_byRxBuffer) / 2 ) )
      {
         bNeedSuspendRx = TRUE ;       /* RX suspension is needed */
      }
   }
   else
   {                                   /* if free space is below half-buffer */
      if ( ( l_wRxIdxIn - l_wRxIdxOut ) >= ( sizeof(l_byRxBuffer) / 2 ) )
      {
         bNeedSuspendRx = TRUE ;       /* RX suspension is needed */
      }
   }

   return bNeedSuspendRx ;
}