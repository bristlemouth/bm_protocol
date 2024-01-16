/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2021 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary <and confidential> to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */

#include <stdint.h>
#include "adin1100_mdio_gpio.h"
#include "adin1100_common.h"


#define MMD_ACR_ADDRESS                 (0x0000U)
#define MMD_ACR_DATA 		                (0x4000U)
#define MMD_ACR_DATA_RW_INCREMENT       (0x8000U)
#define MMD_ACR_DATA_WRITE_INCREMENT    (0xC000U)

#define DEVTYPE(a)         (a >> 16)
#define REGADDR(a)         (a & 0xFFFF)


void delay (void) {;};
/*
 * @brief Sends bits out for the number of clocks
 *
 * @param [in] value
 * @param [in] nbBits
 * @return none
 *
 * @details
 *
 * @sa
 */
static void bitsOut(uint32_t  value, int32_t nbBits)
{
    int32_t i = 0;

    for(i=0; i < nbBits; i++)
    {
        if ((value & (0x80000000u >> i)) != 0)
        {
            BSP_SetPinMDIO(true);
        }
        else
        {
            BSP_SetPinMDIO(false);
        }

        delay ();
        BSP_SetPinMDC(true);
       // delay ();
        BSP_SetPinMDC(false);
    }
}

/*
 * @brief Read input pin for number of clocks
 *
 * @param [in] value
 * @param [in] nbBits
 * @return none
 *
 * @details
 *
 * @sa
 */
static uint16_t bitsIn (void)
{
    /* Input a value from the MII PHY management interface. */
    unsigned int i,val = 0;

    BSP_SetPinMDC(false);
    delay ();
    for (i = 0; i < 16; i++)
    {
        val <<= 1;
        BSP_SetPinMDC(true);
        delay ();
        BSP_SetPinMDC(false);
        delay ();
        val |= BSP_GetPinMDInput();
    }
    BSP_SetPinMDC(true);
    delay ();
    BSP_SetPinMDC(false);

   return (val & 0xFFFF);
}

/*
 * @brief MDIO Read Clause 22
 *
 * @param [in] phyAddr - Hardware Phy address
 * @param [in] phyReg - Register address
 * @param [out] phyData - pointer to the data buffer
 * @return error if TA bit is not pulled down by the slave
 *
 * @details
 *
 * @sa
 */
uint32_t mdioGPIORead(uint8_t phyAddr, uint8_t phyReg, uint16_t *phyData )
{
    uint32_t error = 0;
    int32_t i = 0;
    uint8_t tempBuffer[8] = {0};


    uint32_t temp = 0;
    uint16_t dataIn = 0;

    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;

    /* ST('01) + OP('10) + PHYAD(4 MSB) */
    tempBuffer[i] = 0x60;
    tempBuffer[i++] |= (phyAddr & 0x1F) >> 1;

    /* PHYAD(1 LSB) + REGADD (4) + 2 TA('00) */
    tempBuffer[i] = (phyAddr & 0x01) << 7 ;
    tempBuffer[i] |= (phyReg & 0x1F) << 2;
    tempBuffer[i++] |= ( 0x03) ;//3
    tempBuffer[i++] = 0xFF;
    tempBuffer[i++] = 0xFF;


    BSP_ChangeMDIPinDir(true);//output
    temp = (tempBuffer[0] << 24) | (tempBuffer[1] <<16) | (tempBuffer[2] << 8) | tempBuffer[3];
    bitsOut(temp, 32);
    temp = (tempBuffer[4] << 24) | (tempBuffer[5] <<16) | (tempBuffer[6] << 8) | tempBuffer[7];
    bitsOut(temp, 15);
    BSP_ChangeMDIPinDir(false);//input
    dataIn = bitsIn();
    *phyData = dataIn;

    return error;
}

/*
 * @brief MDIO Write Clause 22
 *
 * @param [in] phyAddr - Hardware Phy address
 * @param [in] phyReg - Register address
 * @param [out] phyData -  data to write
 * @return none
 *
 * @details
 *
 * @sa
 */
uint32_t mdioGPIOWrite(uint8_t phyAddr, uint8_t phyReg, uint16_t phyData )
{
    int32_t i = 0;
    uint8_t tempBuffer[8] = {0};

    uint32_t temp = 0;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;

    /*Preamble*/
    /* ST('01) + OP('01) + PHYAD(4 MSB) */
    tempBuffer[i] = 0x50 ;
    tempBuffer[i++] |= (phyAddr & 0x1F) >> 1;

    /* PHYAD(1 LSB) + REGADD (4) + 2 TA('10) */
    tempBuffer[i] = (phyAddr & 0x01) << 7 ;
    tempBuffer[i] |= (phyReg & 0x1F) << 2;
    tempBuffer[i++] |=  0x2 ;

    /* DATA */
    tempBuffer[i++] = phyData >> 8;
    tempBuffer[i++] = phyData & 0xFF;

    BSP_ChangeMDIPinDir(true);//output
    temp = (tempBuffer[0] << 24) | (tempBuffer[1] <<16) | (tempBuffer[2] << 8) | tempBuffer[3];
    bitsOut(temp, 32);

    temp = (tempBuffer[4] << 24) | (tempBuffer[5] <<16) | (tempBuffer[6] << 8) | tempBuffer[7];
    bitsOut(temp, 32);
    BSP_ChangeMDIPinDir(false);//input

  return 0;
}



/*
 * @brief Helping function MDIO Read  Clause 45
 *
 * @param [in] phyAddr - Hardware Phy address
 * @param [in] devType - Device Type
 * @param [out] phyData - pointer to data buffer
 * @return error if TA bit is not pulled down by the slave
 *
 * @details
 *
 * @sa
 */
static uint32_t mdioRead45Clause(uint8_t phyAddr, uint8_t devType, uint16_t *phyData )
{
    uint32_t error = 0;
    int32_t i = 0;
    uint8_t tempBuffer[8] = {0};

    uint32_t temp = 0;
    uint16_t dataIn = 0;

    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;

    /* ST('00) + OP('11) + PHYAD(4 MSB) */
    tempBuffer[i] = 0x30;
    tempBuffer[i++] |= (phyAddr & 0x1F) >> 1;

    /* PHYAD(1 LSB) + DEVTYPE (4) + 2 TA('00) */
    tempBuffer[i] = (phyAddr & 0x01) << 7 ;
    tempBuffer[i] |= (devType & 0x1F) << 2;
    tempBuffer[i++] |= ( 0x03) ;
    tempBuffer[i++] = 0xFF;
    tempBuffer[i++] = 0xFF;

    BSP_ChangeMDIPinDir(true);//output
    temp = (tempBuffer[0] << 24) | (tempBuffer[1] <<16) | (tempBuffer[2] << 8) | tempBuffer[3];
    bitsOut(temp, 32);
    temp = (tempBuffer[4] << 24) | (tempBuffer[5] <<16) | (tempBuffer[2] << 6) | tempBuffer[7];
    bitsOut(temp, 15);
    BSP_ChangeMDIPinDir(false);//input
    dataIn = bitsIn();
    *phyData = dataIn;

    return error;
}

/*
 * @brief Helping function MDIO Write Clause 45
 *
 * @param [in] phyAddr - Hardware Phy address
 * @param [in] devType - Device Type
 * @param [out] phyData - data to write
 * @return none
 *
 * @details
 *
 * @sa
 */
static uint32_t mdioWrite45Clause(uint8_t phyAddr, uint8_t devType, uint16_t phyData )
{
    int32_t i = 0;
    uint8_t tempBuffer[8] = {0};

    uint32_t preamble = 0xffffffff;
    uint32_t temp = 0;

    /*Preamble*/
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;
    tempBuffer[i++] = 0xff;

    /* ST('00) + OP('01) + PHYAD(4 MSB) */
    tempBuffer[i] = 0x10 ;
    tempBuffer[i++] |= (phyAddr & 0x1F) >> 1;

    /* PHYAD(1 LSB) + DEVTYPE (4) + 2 TA('10) */
    tempBuffer[i] = (phyAddr & 0x01) << 7 ;
    tempBuffer[i] |= (devType & 0x1F) << 2;
    tempBuffer[i++] |=  0x2 ;

    /* DATA */
    tempBuffer[i++] = phyData >> 8;
    tempBuffer[i++] = phyData & 0xFF;



  BSP_ChangeMDIPinDir(false);//input
  BSP_ChangeMDIPinDir(true);//output
  temp = (tempBuffer[0] << 24) | (tempBuffer[1] <<16) | (tempBuffer[2] << 8) | tempBuffer[3];
  bitsOut(preamble, 32);
  temp = (tempBuffer[4] << 24) | (tempBuffer[5] <<16) | (tempBuffer[6] << 8) | tempBuffer[7];
    bitsOut(temp, 32);

  return 0;
}

/*
 * @brief Helping function MDIO Writing the Device Type and Register Address Clause 45
 *
 * @param [in] phyAddr - Hardware Phy address
 * @param [in] devType -  Device Type
 * @param [in] regAddr - Register Address
 * @return none
 *
 * @details
 *
 * @sa
 */
static uint32_t mdioAddr45Clause(uint8_t phyAddr, uint8_t devType, uint16_t regAddr )
{
	int32_t i = 0;
	uint8_t tempBuffer[8] = {0};

	uint32_t temp = 0;
	uint32_t preamble = 0xffffffff;

	/*Preamble*/
	delay();
	tempBuffer[i++] = 0xff;
	tempBuffer[i++] = 0xff;
	tempBuffer[i++] = 0xff;
	tempBuffer[i++] = 0xff;

	/* ST('00) + OP('00) + PHYAD(4 MSB) */
	tempBuffer[i] = 0x00 ;
	tempBuffer[i++] |= (phyAddr & 0x1F) >> 1;

	/* PHYAD(1 LSB) + DEVTYPE (4) + 2 TA('10) */
	tempBuffer[i] = (phyAddr & 0x01) << 7 ;
	tempBuffer[i] |= (devType & 0x1F) << 2;
	tempBuffer[i++] |=  0x2 ;

	/* DATA */
	tempBuffer[i++] = regAddr >> 8;
	tempBuffer[i++] = regAddr & 0xFF;


    BSP_ChangeMDIPinDir(false);//input
    BSP_ChangeMDIPinDir(true);//output
    temp = (tempBuffer[4] << 24) | (tempBuffer[5] <<16) | (tempBuffer[6] << 8) | tempBuffer[7];
    BSP_ChangeMDIPinDir(true);//output
    bitsOut(preamble, 32);
    bitsOut(temp, 32);
    BSP_ChangeMDIPinDir(false);//input


  return 0;
}

/*
 * @brief MDIO Read Clause 45
 *
 * @param [in] phyAddr - Hardware Phy address
 * @param [in] phyReg - Register address in clause 45 combined devType and regAddr
 * @param [out] phyData - pointer to the data buffer
 * @return error if TA bit is not pulled down by the slave
 *
 * @details
 *
 * @sa
 */
uint32_t mdioGPIORead_cl45(uint8_t phyAddr, uint32_t phyReg, uint16_t *phyData )
{
  /*Applying the device type */
   mdioAddr45Clause(phyAddr, DEVTYPE(phyReg), REGADDR(phyReg) );

   /*Reading from register address*/
   return mdioRead45Clause(phyAddr, DEVTYPE(phyReg), phyData );
}

/*
 * @brief MDIO Write Clause 45
 *
 * @param [in] phyAddr - Hardware Phy address
 * @param [in] phyReg - Register address in clause 45 combined devAddr and regAddr
 * @param [out] phyData -  data
 * @return none
 *
 * @details
 *
 * @sa
 */
uint32_t mdioGPIOWrite_cl45(uint8_t phyAddr, uint32_t phyReg, uint16_t phyData )
{
     /*Applying the device type*/
    mdioAddr45Clause(phyAddr, DEVTYPE(phyReg), REGADDR(phyReg));

    /*Writing data to register address*/
    mdioWrite45Clause(phyAddr, DEVTYPE(phyReg), phyData );
   return 0;
}
