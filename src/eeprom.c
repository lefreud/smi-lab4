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
static unsigned int ReadStatusRegister();
static int IsWriteInProgress();

void initEEPROM()
{
	SPI1->CR1 |= 0b1 << 6; // SPI enabled
	SPI1->CR1 |= 0b111 << 3; // Baud rate control (f_PCLK/256) TODO: find optimal baud rate
	SPI1->CR1 |= 0b1 << 2; // Master mode
	SPI1->CR2 |= 0b1 << 2; // SS output enabled
}

char LireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Destination)
{
	// TODO: validate addr

	return 0; // TODO: check for failures
}

char EcrireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Source)
{
	// TODO: validate addr
	// TODO: use DMA for faster/less cumbersome data transfers?

	// TODO: MAKE SURE ADRESS IS ALIGNED AT PAGE, OR SPLIT IN MULTIPLE PAGES
	// -> call EcrirePageEEPROM for each page
	unsigned char toWrite[] = {
			0, 1, 2, 3
	};
	EcrirePageEEPROM(0x0000, 4, toWrite);

	return 0; // TODO: check for failures
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

	/*
	 * WRITE ENABLE
	 */

	SPI1->NSS = 0;

	while (SPI1->SR & TXE_FLAG != 0);
	SPI1->DR = 0b00000110;
	while (SPI1->SR & BSY_FLAG != 0);

	SPI1->NSS = 1;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns

	/*
	 * START TX
	 */

	SPI1->NSS = 0;

	while (SPI1->SR & TXE_FLAG != 0);

	// send WRITE instruction
	SPI1->DR = 0b00000010;

	// send 8 LSB address bits
	while (SPI1->SR & TXE_FLAG != 0);
	SPI1->DR = AdresseEEPROM & 0xFF;

	// send 8 MSB address bits
	while (SPI1->SR & TXE_FLAG != 0);
	SPI1->DR = AdresseEEPROM & 0xFF00;

	// send data
	for (int i = 0; i < NbreOctets; i++) {
		while (SPI1->SR & TXE_FLAG != 0);
		SPI1->DR = Source[i];
	}
	while (SPI1->SR & BSY_FLAG != 0);
	SPI1->NSS = 1;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns

	/*
	 * WRITE DISABLE
	 */

	SPI1->NSS = 0;

	while (SPI1->SR & TXE_FLAG != 0);
	SPI1->DR = 0b00000100;
	while (SPI1->SR & BSY_FLAG != 0);

	// Slave select disabled
	SPI1->NSS = 1;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns
}

static unsigned int ReadStatusRegister()
{

	SPI1->NSS = 0;
	while (SPI1->SR & TXE_FLAG != 0);
	SPI1->DR = 0b00000101;

	while (SPI1->SR & RXNE_FLAG != 0);
	unsigned int statusRegisterValue = SPI1->DR;

	// Slave select disabled
	SPI1->NSS = 1;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns

	return statusRegisterValue;
}

static int IsWriteInProgress()
{
	return ReadStatusRegister() & 1;
}

