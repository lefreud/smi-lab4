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
#define EEPROM_DELAY_TICKS 1000 // at least 50 ns
#define SS_PIN BIT1

static void EcrirePageEEPROM(unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Source);
static unsigned int ReadStatusRegister();
static int IsWriteInProgress();
static void startSPIcommunication();
static void endSPIcommunication();
static int transmitWord(unsigned int byte);
static unsigned int receiveWord();

void initEEPROM()
{
	/*
	 * Configure SPI
	 */

	// clocks
	RCC->AHB1ENR |= BIT0 | BIT1; // Enable port A, B
	RCC->APB1ENR |= BIT14; // Enable SPI2 clock

	// GPIO output for slave select on PA1
	GPIOA->MODER |= BIT2;

	// SPI-specific config

	SPI2->CR2 |= BIT2; // SS output enabled
	SPI2->CR1 |= BIT2 // Master mode
	          | 0b111 << 3 // Baud rate control (f_PCLK/2) TODO: find optimal baud rate
			  // | BIT15 // bidimode
	          ;//| BIT9; // software slave select management
	// SPI2->CR1 |= BIT14; // bidioe


	NVIC->ISER[1] |= BIT3; // SPI global interrupt (bit 35)

	/*
	 * Set PB12, PB13, PB14, PB15 to alternate function
	 */

	GPIOB->OSPEEDR |= BIT25 | BIT27 | BIT29 | BIT31;

	// Mode alternate function
	GPIOB->MODER |= GPIO_ALTERNATE_FUNCTION << 24 |
					GPIO_ALTERNATE_FUNCTION << 26 |
					GPIO_ALTERNATE_FUNCTION << 28 |
					GPIO_ALTERNATE_FUNCTION << 30;

	GPIOB->AFR[1] |= SPI_ALTERNATE_FUNCTION << 16 |
					SPI_ALTERNATE_FUNCTION << 20 |
					SPI_ALTERNATE_FUNCTION << 24 |
					SPI_ALTERNATE_FUNCTION << 28;

	/*
	 * Start SPI master
	 */

	// Slave select disabled
	GPIOA->ODR |= SS_PIN;

	// TODO
	while (1) {
		ReadStatusRegister();
		for (volatile int i = 0; i < EEPROM_DELAY_TICKS; i++); // at least 50 ns
	}
}

char LireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Destination)
{
	// TODO: validate addr
	while (IsWriteInProgress());

	for (unsigned int i = 0; i < NbreOctets; i++) {
		GPIOA->ODR &= ~SS_PIN;

		// send READ instruction
		while ((SPI2->SR & TXE_FLAG) == 0);
		SPI2->DR = 0b00000011;

		// send 8 MSB address bits
		while ((SPI2->SR & TXE_FLAG) == 0);
		SPI2->DR = (AdresseEEPROM + i) & 0xFF00;

		// send 8 LSB address bits
		while ((SPI2->SR & TXE_FLAG) == 0);
		SPI2->DR = (AdresseEEPROM + i) & 0xFF;

		// TODO
		for (volatile int i = 0; i < 1000; i++);

		// read data
		while ((SPI2->SR & RXNE_FLAG) == 0);
		volatile unsigned int a = SPI2->DR;
		Destination[i] = SPI2->DR;

		while ((SPI2->SR & BSY_FLAG) != 0);

		// Slave select disabled
		GPIOA->ODR |= SS_PIN;
		for (volatile int i = 0; i < EEPROM_DELAY_TICKS; i++); // at least 50 ns
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

	GPIOA->ODR &= ~SS_PIN;

	while ((SPI2->SR & TXE_FLAG) == 0);
	SPI2->DR = 0b00000110;
	while ((SPI2->SR & BSY_FLAG) != 0);

	GPIOA->ODR |= SS_PIN;
	for (volatile int i = 0; i < EEPROM_DELAY_TICKS; i++); // at least 50 ns

	/*
	 * START TX
	 */

	GPIOA->ODR &= ~SS_PIN;

	// send WRITE instruction
	while ((SPI2->SR & TXE_FLAG) == 0);
	SPI2->DR = 0b00000010;

	// send 8 MSB address bits
	while ((SPI2->SR & TXE_FLAG) == 0);
	SPI2->DR = AdresseEEPROM & 0xFF00;

	// send 8 LSB address bits
	while ((SPI2->SR & TXE_FLAG) == 0);
	SPI2->DR = AdresseEEPROM & 0xFF;

	// send data
	for (int i = 0; i < NbreOctets; i++) {
		while ((SPI2->SR & TXE_FLAG) == 0);
		SPI2->DR = Source[i];
	}
	while ((SPI2->SR & BSY_FLAG) != 0);
	GPIOA->ODR |= SS_PIN;
	for (volatile int i = 0; i < EEPROM_DELAY_TICKS; i++); // at least 50 ns

	/*
	 * WRITE DISABLE
	 */

	GPIOA->ODR &= ~SS_PIN;

	while ((SPI2->SR & TXE_FLAG) == 0);
	SPI2->DR = 0b00000100;
	while ((SPI2->SR & BSY_FLAG) != 0);

	// Slave select disabled
	GPIOA->ODR |= SS_PIN;
	for (volatile int i = 0; i < EEPROM_DELAY_TICKS; i++); // at least 50 ns
}

static unsigned int ReadStatusRegister()
{
	startSPIcommunication();
	transmitWord(0b00000101);
	transmitWord(0xFF);

	unsigned int statusRegisterValue = receiveWord();

	endSPIcommunication();

	return statusRegisterValue;
}

static int IsWriteInProgress()
{
	return ReadStatusRegister() & BIT0;
}

inline static int transmitWord(unsigned int byte)
{
	while (!(SPI2->SR & TXE_FLAG)) {}
	SPI2->DR = 0xFF & byte;
	while (!(SPI2->SR & TXE_FLAG)) {}
	while (!(SPI2->SR & RXNE_FLAG)) {}
}

inline static void startSPIcommunication()
{
	SPI2->CR1 |= BIT6; // SPI enabled
	GPIOA->ODR &= ~SS_PIN;
}

inline static void endSPIcommunication()
{
	while ((SPI2->SR & BSY_FLAG)) {}

	SPI2->CR1 &= ~BIT6; // SPI disabled
	GPIOA->ODR |= SS_PIN;

	for (volatile int i = 0; i < EEPROM_DELAY_TICKS; i++); // at least 50 ns
}

inline static unsigned int receiveWord()
{
	return SPI2->DR;
}
