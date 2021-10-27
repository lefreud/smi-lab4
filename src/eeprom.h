/*
 * eeprom.h
 *
 *  Created on: Oct 27, 2021
 *      Author: freud
 */

#ifndef EEPROM_H_
#define EEPROM_H_

void initEEPROM();
char LireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Destination);
char EcrireMemoireEEPROM (unsigned int AdresseEEPROM, unsigned int NbreOctets, unsigned char *Source);

#endif /* EEPROM_H_ */
