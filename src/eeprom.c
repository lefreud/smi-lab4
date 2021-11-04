/*
 * eeprom.c
 *
 *  Created on: Oct 27, 2021
 *      Author: freud
 */
#include "stm32f4xx.h"
#include "macros_utiles.h"
#include "eeprom.h"

#define BSY_FLAG BIT7
#define TXE_FLAG BIT1
#define RXNE_FLAG BIT0
#define SSI_FLAG BIT8

#define SPI_ALTERNATE_FUNCTION 0x5
#define GPIO_ALTERNATE_FUNCTION 0b10
#define EEPROM_DELAY_TICKS 1000 // at least 50 ns
#define SS_PIN BIT1

#define EEPROM_PAGE_SIZE 64

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
	          ;


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
}

char LireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Destination)
{
	if (AdresseEEPROM >= EEPROM_MAX_ADDRESS) {
		return 1;
	}

	while (IsWriteInProgress());

	for (unsigned int i = 0; i < NbreOctets; i++) {
		startSPIcommunication();

		// send command to read word
		transmitWord(0b00000011);

		// send address
		transmitWord(((AdresseEEPROM + i) & 0xFF00) >> 8);
		transmitWord((AdresseEEPROM + i) & 0xFF);

		// read data
		transmitWord(0xFF);
		volatile unsigned int x = receiveWord();
		Destination[i] = receiveWord();

		endSPIcommunication();
	}


	return 0;
}

char EcrireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Source)
{
	if (AdresseEEPROM >= EEPROM_MAX_ADDRESS) {
		return 1;
	}

	// TODO: use DMA for faster/less cumbersome data transfers?

	unsigned int maxAddressToWrite = AdresseEEPROM + NbreOctets;
	unsigned int currentAddress = AdresseEEPROM;
	unsigned int currentPage = AdresseEEPROM / EEPROM_PAGE_SIZE;
	int i = 0;

	// write each page individually
	while (currentAddress < maxAddressToWrite) {
		unsigned int bytesToWrite;
		if (maxAddressToWrite - currentAddress < EEPROM_PAGE_SIZE) {
			bytesToWrite = maxAddressToWrite - currentAddress;
		} else if ((currentPage + 1) * EEPROM_PAGE_SIZE - currentAddress < EEPROM_PAGE_SIZE) {
			bytesToWrite = (currentPage + 1) * EEPROM_PAGE_SIZE - currentAddress;
		} else {
			bytesToWrite = EEPROM_PAGE_SIZE;
		}

		// write
		EcrirePageEEPROM(currentAddress, bytesToWrite, &Source[currentAddress - AdresseEEPROM]);

		i++;
		currentPage++;
		currentAddress = currentPage * EEPROM_PAGE_SIZE;
	}

	return 0;
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

	startSPIcommunication();
	transmitWord(0b00000110);
	endSPIcommunication();

	/*
	 * START TX
	 */

	startSPIcommunication();

	// send WRITE instruction
	transmitWord(0b00000010);

	// send 8 MSB address bits
	transmitWord((AdresseEEPROM & 0xFF00) >> 8);

	// send 8 LSB address bits
	transmitWord(AdresseEEPROM & 0xFF);

	// send data
	for (int i = 0; i < NbreOctets; i++) {
		transmitWord(Source[i]);
	}

	endSPIcommunication();

	/*
	 * WRITE DISABLE
	 */

	startSPIcommunication();
	transmitWord(0b00000100);
	endSPIcommunication();
}

static unsigned int ReadStatusRegister()
{
	startSPIcommunication();

	// read status register
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
	volatile unsigned int x = SPI2->DR;
}

inline static void startSPIcommunication()
{
	SPI2->CR1 |= BIT6; // SPI enabled
	while (!(SPI2->SR & TXE_FLAG)) {}
	GPIOA->ODR &= ~SS_PIN;
}

inline static void endSPIcommunication()
{
	while ((SPI2->SR & BSY_FLAG)) {}

	GPIOA->ODR |= SS_PIN;

	SPI2->CR1 &= ~BIT6; // SPI disabled

	for (volatile int i = 0; i < EEPROM_DELAY_TICKS; i++); // at least 50 ns
}

inline static unsigned int receiveWord()
{
	return SPI2->DR;
}
