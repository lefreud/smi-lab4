/*
 * eeprom.c
 *
 *  Created on: Oct 27, 2021
 *      Author: freud
 */
#include "stm32f4xx.h"

#define BSY_FLAG (1 << 7)
#define TXE_FLAG (1 << 1)
#define RXNE_FLAG (1 << 0)

static void EcrirePageEEPROM(unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Source);
static int IsWriteInProgress();

void initEEPROM()
{
	SPI1->CR1 |= 0b1 << 6; // SPI enabled
	SPI1->CR1 |= 0b0 << 3; // Baud rate control (f_PCLK/2)
	SPI1->CR1 |= 0b1 << 2; // Master mode
	SPI1->CR1 |= 0b1 << 7; // Transmit LSB first
	SPI1->CR2 |= 0b1 << 2; // SS output enabled
}

char LireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Destination)
{

}

char EcrireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Source)
{
	// TODO: use DMA for faster/less cumbersome data transfers?

	// TODO: MAKE SURE ADRESS IS ALIGNED AT PAGE, OR SPLIT IN MULTIPLE PAGES
	// -> call EcrirePageEEPROM for each page
}

/**
 * Function responsible for writing a page to the EEPROM.
 *
 * The bytes can start anywhere in the page but must not overflow
 * into the next page.
 */
static void EcrirePageEEPROM(unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Source)
{
	while (IsWriteInProgress());

	// TODO: Set write enable latch WREN

	/*
	 * WRITE ENABLE
	 */

	SPI1->NSS = 0;

	while (SPI1->SR & TXE_FLAG != 0);
	SPI1->DR = 0b00000110;
	while (SPI1->SR & BSY_FLAG != 0);

	SPI1->NSS = 1;

	while (SPI1->SR & TXE_FLAG != 0);

	/*
	 * START TX
	 */

	SPI1->NSS = 0;

	while (SPI1->SR & TXE_FLAG != 0);

	// send WRITE instruction
	SPI1->DR = 0b00000010;

	while (SPI1->SR & TXE_FLAG != 0);
	// send 8 LSB address bits
	SPI1->DR = AdresseEEPROM & 0xFF;

	// wait
	while (SPI1->SR & TXE_FLAG != 0);
	// send 8 MSB address bits
	SPI1->DR = AdresseEEPROM & 0xFF00;

	while (SPI1->SR & BSY_FLAG != 0);

	// ... TODO: boucle while de transmission des bits

	/*
	 * WRITE DISABLE
	 */
	while (SPI1->SR & TXE_FLAG != 0);
	SPI1->DR = 0b00000100;
	while (SPI1->SR & BSY_FLAG != 0);

	// Slave select disabled
	SPI1->NSS = 1;

}

static int IsWriteInProgress()
{
	// TODO
	return 1;
}

