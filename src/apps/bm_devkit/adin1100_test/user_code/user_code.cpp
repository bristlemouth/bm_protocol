#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "util.h"

#include "adin1100.h"

#define LED_ON_TIME_MS 250
#define LED_PERIOD_MS 1000

#define ADIN1100_INIT_ITER      (5)

uint8_t phyDevMem[ADI_PHY_DEVICE_SIZE];

adi_phy_DriverConfig_t phyDrvConfig = {
    .addr       = 0,
    .pDevMem    = (void *)phyDevMem,
    .devMemSize = sizeof(phyDevMem),
    .enableIrq  = true,
};

volatile uint32_t lastLinkCheckTime = 0;
volatile bool     irqFired = false;

adin1100_DeviceStruct_t dev;
adin1100_DeviceHandle_t hDevice = &dev;
bool device_ready = false;

adi_phy_LinkStatus_e    linkstatus = ADI_PHY_LINK_STATUS_DOWN;
adi_phy_LinkStatus_e    oldlinkstatus = ADI_PHY_LINK_STATUS_DOWN;
adi_phy_AnStatus_t      anstatus;

bool heartbeat_on = false;

/* Static configuration function for the ADIN1100, it is executed after */
/* reset and initialization, with the ADIN1100 in software powerdown.   */
adi_eth_Result_e appPhyConfig(adin1100_DeviceHandle_t hDevice)
{
    adi_eth_Result_e        result = ADI_ETH_SUCCESS;

    /* Any static configuration that needs to be done before the link */
    /* is brought up can be included here.                            */
    result = adin1100_AnAdvTxMode(hDevice, ADI_PHY_AN_ADV_TX_REQ_2P4V);

    // result = adin1100_EnterMediaConverterMode( hDevice );
    // if( result != ADI_ETH_SUCCESS ) {
    //   printf( "mac fail\n" );
    // }
    // else {
    //   printf( "mac success\n" );
    // }

    return result;
}

void phyCallback(void *pCBParam, uint32_t Event, void *pArg)
{
  (void)pCBParam;
  (void)Event;
  (void)pArg;

  irqFired = true;
}



void setup(void)
{
  vTaskDelay(pdMS_TO_TICKS(3000));

  adi_eth_Result_e        result;
  uint32_t                status;
  uint16_t                capabilities;

  /* USER ONE-TIME SETUP CODE GOES HERE */

  /****** Driver Init *****/
  /* If both the MCU and the ADIN1100 are reset simultaneously */
  /* using the RESET button on the board, the MCU may start    */
  /* scanning for ADIN1100 devices before the ADIN1100 has     */
  /* powered up. This is worse if PHY address is configured as */
  /* 0 (default configuration of the board).                   */
  /* This is taken care of by iterating more than once over    */
  /* the valid MDIO address space.                             */
  for (uint32_t i = 0; i < ADIN1100_INIT_ITER; i++)
  {
      for (uint32_t phyAddr = 0; phyAddr < 8; phyAddr++)
      {
          phyDrvConfig.addr = phyAddr;
          result = adin1100_Init(hDevice, &phyDrvConfig);
          if (ADI_ETH_SUCCESS == result)
          {
              printf("Found PHY device at address %d\n", phyAddr);
              result = adin1100_ReadIrqStatus(hDevice, &status);
              result = adin1100_GetCapabilities(hDevice, &capabilities);
              printf("adin1100_GetCapabilities: %d\n", (int)result);

              if (capabilities & ADI_PHY_CAP_TX_HIGH_LEVEL)
              {
                  printf("  - Supports 2.4V Tx level\n");
              }
              else
              {
                  printf("  - Does not support 2.4V Tx level\n");
              }

              if (capabilities & ADI_PHY_CAP_PMA_LOCAL_LOOPBACK)
              {
                  printf("  - Supports PMA local loopback\n");
              }
              else
              {
                  printf("  - Does not support PMA local loopback\n");
              }

              break;
          }
      }
      if (ADI_ETH_SUCCESS == result)
      {
          break;
      }
  }

  if( result != ADI_ETH_SUCCESS )
  {
    printf( "No device found \n" );
    return;
  }

  /* Device configuration, performed while the device is in software powerdown. */
  result = appPhyConfig(hDevice);
  printf("appPhyConfig: %d\n", (int)result);

  /* Register callback for selected events. Note that the interrupts corresponding to */
  /* ADI_PHY_EVT_HW_RESET and ADI_PHY_EVT_CRSM_HW_ERROR are always unmasked so they   */
  /* can be ommitted from the argument, here they are explicit for clarity.           */
  result = adin1100_RegisterCallback(hDevice, phyCallback, ADI_PHY_EVT_HW_RESET | ADI_PHY_EVT_CRSM_HW_ERROR | ADI_PHY_EVT_LINK_STAT_CHANGE);
  printf("adin1100_RegisterCallback: %d\n", (int)result);

  /* Exit software powerdown and attamept to bring up the link. */
  result = adin1100_ExitSoftwarePowerdown(hDevice);
  printf("adin1100_ExitSoftwarePowerdown: %d\n", (int)result);

  /* The link may not be up at this point */
  result = adin1100_GetLinkStatus(hDevice, &linkstatus);
  printf("adin1100_GetLinkStatus: %d\n", (int)result);

  if( result != ADI_ETH_SUCCESS ) {
    printf( "Failed to initialize device: %d\n", (int)result );
    return;
  }


  device_ready = true;
}

void loop(void)
{
  adi_eth_Result_e        result;
  uint32_t                status;
  char                    *strBuffer;

  /* USER LOOP CODE GOES HERE */
  /// This section demonstrates a simple non-blocking bare metal method for rollover-safe timed tasks,
  ///   like blinking an LED.
  /// More canonical (but more arcane) modern methods of implementing this kind functionality
  ///   would bee to use FreeRTOS tasks or hardware timer ISRs.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool ledState = false;

  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!ledState &&
      ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    ledState = true;
  }

  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (ledState && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS))
  {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    ledState = false;



    // Piggy back some ADIN1100 code
    if (irqFired)
    {
        irqFired = false;

        result = adin1100_ReadIrqStatus(hDevice, &status);
        printf("adin1100_ReadIrqStatus: %d\n", (int)result );

        if (status & ADI_PHY_EVT_HW_RESET)
        {
            result = adin1100_ReInitPhy(hDevice);
            printf("adin1100_ReInitPhy: %d\n", (int)result );

            result = appPhyConfig(hDevice);
            printf("appPhyConfig: %d\n", (int)result );

            result = adin1100_ExitSoftwarePowerdown(hDevice);
            printf("adin1100_ExitSoftwarePowerdown: %d\n", (int)result );
        }
        if (status & ADI_PHY_EVT_LINK_STAT_CHANGE)
        {
            printf("INT: Link status changed\n");
        }
    }

    result = adin1100_GetLinkStatus(hDevice, &linkstatus);
    if (result == ADI_ETH_COMM_ERROR)
    {
        printf("MDIO communication error\n");
    }
    else
    {
      printf("adin1100_GetLinkStatus: %d\n", (int)result );

      if (oldlinkstatus != linkstatus)
      {
          switch (linkstatus)
          {
              case ADI_PHY_LINK_STATUS_UP:
                  result = adin1100_GetAnStatus(hDevice, &anstatus);
                  if (result == ADI_ETH_COMM_ERROR)
                  {
                    printf("MDIO communication error\n");
                  }

                  strBuffer = "Link Up\n";
                  printf("%s", strBuffer );
                  switch(anstatus.anMsResolution)
                  {
                    case ADI_PHY_AN_MS_RESOLUTION_SLAVE:
                      strBuffer = "M/S: Master\n";
                    break;
                    case ADI_PHY_AN_MS_RESOLUTION_MASTER:
                          strBuffer= "M/S: Slave\n";
                    break;
                    default:
                      strBuffer= "ADI_PHY_AN_MS_DEFAULT\n";
                      break;
                  }
                  printf("%s", strBuffer );
                  switch(anstatus.anTxMode)
                  {
                    case ADI_PHY_AN_TX_LEVEL_1P0V:
                          strBuffer = "Tx Level: 1.0V\n";
                    break;
                    case ADI_PHY_AN_TX_LEVEL_2P4V:
                          strBuffer = "Tx Level: 2.4V\n";
                    break;
                    default:
                      strBuffer= "ADI_PHY_AN_TX_DEFAULT\n";
                      break;
                  }
                  printf("%s", strBuffer );
                  // BSP_ErrorLed(true);
                  break;

              case ADI_PHY_LINK_STATUS_DOWN:
                  strBuffer = "Link Down\n";
                  printf("%s", strBuffer );
                  // BSP_ErrorLed(false);
                  break;

              default:
                  printf("Invalid link status: %d", 0);
          }

      }
      oldlinkstatus = linkstatus;
    }
    if( heartbeat_on ){
      bristlefin.setLed(2, Bristlefin::LED_GREEN);
      heartbeat_on = false;
    }
    else {
      bristlefin.setLed(2, Bristlefin::LED_OFF);
      heartbeat_on = true;
    }

  }
}
