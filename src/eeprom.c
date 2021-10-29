/*
 * eeprom.c
 *
 *  Created on: Oct 27, 2021
 *      Author: freud
 */
#include "stm32f4xx.h"
#include "macros_utiles.h"

#define BSY_FLAG BIT7
#define TXE_FLAG BIT1
#define RXNE_FLAG BIT0
#define SSI_FLAG BIT8

#define SPI_ALTERNATE_FUNCTION 0x5
#define GPIO_ALTERNATE_FUNCTION 0b10

static void EcrirePageEEPROM(unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Source);
static unsigned int ReadStatusRegister();
static int IsWriteInProgress();

void initEEPROM()
{
	/*
	 * Configure SPI
	 */

	// clocks
	RCC->AHB1ENR |= BIT0; // Enable port A
	RCC->APB2ENR |= BIT12; // Enable SPI1 clock


	// SPI-specific config
	SPI1->CR2 |= BIT2; // SS output enabled
	SPI1->CR1 |= BIT2; // Master mode
	SPI1->CR1 |= 0b000 << 3; // Baud rate control (f_PCLK/2) TODO: find optimal baud rate
	SPI1->CR1 |= BIT9; // software slave select management
	//SPI1->CR1 |= BIT15; // bidimode
	//SPI1->CR1 |= BIT14; // bidioe

	NVIC->ISER[1] |= BIT3; // SPI global interrupt (bit 35)

	SPI1->CR1 |= BIT6; // SPI enabled

	// Set alternate GPIO function for pins 4,5,6,7 to SPI
	GPIOA->OSPEEDR |= BIT9 | BIT11 | BIT13 | BIT15;

	GPIOA->MODER |= GPIO_ALTERNATE_FUNCTION << 8 |
					GPIO_ALTERNATE_FUNCTION << 10 |
					GPIO_ALTERNATE_FUNCTION << 12 |
					GPIO_ALTERNATE_FUNCTION << 14;

	GPIOA->AFR[0] |= SPI_ALTERNATE_FUNCTION << 16 |
			SPI_ALTERNATE_FUNCTION << 20 |
			SPI_ALTERNATE_FUNCTION << 24 |
			SPI_ALTERNATE_FUNCTION << 28;

	/*
	 * Start SPI master
	 */

	// Slave select disabled
	SPI1->CR1 &= ~SSI_FLAG;
	SPI1->CR1 |= SSI_FLAG;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns
}

char LireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Destination)
{
	// TODO: validate addr
	while (IsWriteInProgress());

	for (unsigned int i = 0; i < NbreOctets; i++) {
		SPI1->CR1 &= ~SSI_FLAG;

		// send READ instruction
		while ((SPI1->SR & TXE_FLAG) == 0);
		SPI1->DR = 0b00000011;

		// send 8 MSB address bits
		while ((SPI1->SR & TXE_FLAG) == 0);
		SPI1->DR = (AdresseEEPROM + i) & 0xFF00;

		// send 8 LSB address bits
		while ((SPI1->SR & TXE_FLAG) == 0);
		SPI1->DR = (AdresseEEPROM + i) & 0xFF;

		// read data
		while ((SPI1->SR & RXNE_FLAG) == 0);
		Destination[i] = SPI1->DR;

		while ((SPI1->SR & BSY_FLAG) != 0);

		// Slave select disabled
		SPI1->CR1 |= SSI_FLAG;
		for (volatile int i = 0; i < 1000000; i++); // at least 50 ns
	}


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

	SPI1->CR1 &= ~SSI_FLAG;

	while ((SPI1->SR & TXE_FLAG) == 0);
	SPI1->DR = 0b00000110;
	while ((SPI1->SR & BSY_FLAG) != 0);

	SPI1->CR1 |= SSI_FLAG;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns

	/*
	 * START TX
	 */

	SPI1->CR1 &= ~SSI_FLAG;

	// send WRITE instruction
	while ((SPI1->SR & TXE_FLAG) == 0);
	SPI1->DR = 0b00000010;

	// send 8 MSB address bits
	while ((SPI1->SR & TXE_FLAG) == 0);
	SPI1->DR = AdresseEEPROM & 0xFF00;

	// send 8 LSB address bits
	while ((SPI1->SR & TXE_FLAG) == 0);
	SPI1->DR = AdresseEEPROM & 0xFF;

	// send data
	for (int i = 0; i < NbreOctets; i++) {
		while ((SPI1->SR & TXE_FLAG) == 0);
		SPI1->DR = Source[i];
	}
	while ((SPI1->SR & BSY_FLAG) != 0);
	SPI1->CR1 |= SSI_FLAG;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns

	/*
	 * WRITE DISABLE
	 */

	SPI1->CR1 &= ~SSI_FLAG;

	while ((SPI1->SR & TXE_FLAG) == 0);
	SPI1->DR = 0b00000100;
	while ((SPI1->SR & BSY_FLAG) != 0);

	// Slave select disabled
	SPI1->CR1 |= SSI_FLAG;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns
}

static unsigned int ReadStatusRegister()
{
	SPI1->CR1 &= ~SSI_FLAG;
	while (!(SPI1->SR & TXE_FLAG)) {}
	SPI1->DR = 0b00000101;

	while (!(SPI1->SR & RXNE_FLAG)) {}
	unsigned int statusRegisterValue = SPI1->DR;

	// Slave select disabled
	SPI1->CR1 |= SSI_FLAG;
	for (volatile int i = 0; i < 1000000; i++); // at least 50 ns

	return statusRegisterValue;
}

static int IsWriteInProgress()
{
	return ReadStatusRegister() & 1;
}

