/*! \file enc28j60.c \brief Microchip ENC28J60 Ethernet Interface Driver. */
//*****************************************************************************
//
// File Name	: 'enc28j60.c'
// Title		: Microchip ENC28J60 Ethernet Interface Driver
// Author		: Pascal Stang (c)2005
// Created		: 9/22/2005
// Revised		: 9/22/2005
// Version		: 0.1
// Target MCU	: Atmel AVR series
// Editor Tabs	: 4
//
// Description	: This driver provides initialization and transmit/receive
//	functions for the Microchip ENC28J60 10Mb Ethernet Controller and PHY.
// This chip is novel in that it is a full MAC+PHY interface all in a 28-pin
// chip, using an SPI interface to the host processor.
//
//*****************************************************************************

#include <avr/io.h>
#include <util/delay.h>
//#include "timer.h"	//Note have been replaced with _delay_us() as this is more convient

#include "enc28j60.h"

/*#ifndef SPDR
#ifdef SPDR0
	#define SPDR	SPDR0
	#define SPCR	SPCR0
	#define SPSR	SPSR0

	#define SPIF	SPIF0
	#define MSTR	MSTR0
	#define CPOL	CPOL0
	#define DORD	DORD0
	#define SPR0	SPR00
	#define SPR1	SPR01
	#define SPI2X	SPI2X0
	#define SPE		SPE0
#endif
#endif
*/
// include configuration
//#include "enc28j60conf.h"

#define cbi(reg,bit)    reg &= ~(_BV(bit))
#define sbi(reg,bit)    reg |= (_BV(bit))
#define MIN(a,b)                        ((a<b)?(a):(b))

uint8_t Enc28j60Bank;
uint16_t NextPacketPtr;

uint8_t enc28j60ReadOp(uint8_t op, uint8_t address)
{
	uint8_t data;
   
	// assert CS
	CONFIG_DRIVERS_ENC28J60_CTL_PORT &= ~(1<<CONFIG_DRIVERS_ENC28J60_CTL_PIN);
	
	// issue read command
	SPDR = op | (address & ADDR_MASK);
	while(!(SPSR & (1<<SPIF)));
	// read data
	SPDR = 0x00;
	while(!(SPSR & (1<<SPIF)));
	// do dummy read if needed
	if(address & 0x80)
	{
		SPDR = 0x00;
		while(!(SPSR & (1<<SPIF)));
	}
	data = SPDR;
	
	// release CS
	CONFIG_DRIVERS_ENC28J60_CTL_PORT |= (1<<CONFIG_DRIVERS_ENC28J60_CTL_PIN);

	return data;
}

void enc28j60WriteOp(uint8_t op, uint8_t address, uint8_t data)
{
	// assert CS
	CONFIG_DRIVERS_ENC28J60_CTL_PORT &= ~(1<<CONFIG_DRIVERS_ENC28J60_CTL_PIN);

	// issue write command
	SPDR = op | (address & ADDR_MASK);
	while(!(SPSR & (1<<SPIF)));
	// write data
	SPDR = data;
	while(!(SPSR & (1<<SPIF)));

	// release CS
	CONFIG_DRIVERS_ENC28J60_CTL_PORT |= (1<<CONFIG_DRIVERS_ENC28J60_CTL_PIN);
}

void enc28j60ReadBuffer(uint16_t len, uint8_t* data)
{
	// assert CS
	CONFIG_DRIVERS_ENC28J60_CTL_PORT &= ~(1<<CONFIG_DRIVERS_ENC28J60_CTL_PIN);
	
	// issue read command
	SPDR = ENC28J60_READ_BUF_MEM;
	while(!(SPSR & (1<<SPIF)));
	while(len--)
	{
		// read data
		SPDR = 0x00;
		while(!(SPSR & (1<<SPIF)));
		*data++ = SPDR;
	}	
	// release CS
	CONFIG_DRIVERS_ENC28J60_CTL_PORT |= (1<<CONFIG_DRIVERS_ENC28J60_CTL_PIN);
}

void enc28j60WriteBuffer(uint16_t len, uint8_t* data)
{
	// assert CS
	CONFIG_DRIVERS_ENC28J60_CTL_PORT &= ~(1<<CONFIG_DRIVERS_ENC28J60_CTL_PIN);
	
	// issue write command
	SPDR = ENC28J60_WRITE_BUF_MEM;
	while(!(SPSR & (1<<SPIF)));
	while(len--)
	{
		// write data
		SPDR = *data++;
		while(!(SPSR & (1<<SPIF)));
	}	
	// release CS
	CONFIG_DRIVERS_ENC28J60_CTL_PORT |= (1<<CONFIG_DRIVERS_ENC28J60_CTL_PIN);
}

void enc28j60SetBank(uint8_t address)
{
	// set the bank (if needed)
	if((address & BANK_MASK) != Enc28j60Bank)
	{
		// set the bank
		enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1|ECON1_BSEL0));
		enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, (address & BANK_MASK)>>5);
		Enc28j60Bank = (address & BANK_MASK);
	}
}

uint8_t enc28j60Read(uint8_t address)
{
	// set the bank
	enc28j60SetBank(address);
	// do the read
	return enc28j60ReadOp(ENC28J60_READ_CTRL_REG, address);
}

void enc28j60Write(uint8_t address, uint8_t data)
{
	// set the bank
	enc28j60SetBank(address);
	// do the write
	enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, address, data);
}

uint16_t enc28j60PhyRead(uint8_t address)
{
	uint16_t data;

	// Set the right address and start the register read operation
	enc28j60Write(MIREGADR, address);
	enc28j60Write(MICMD, MICMD_MIIRD);

	// wait until the PHY read completes
	while(enc28j60Read(MISTAT) & MISTAT_BUSY);

	// quit reading
	enc28j60Write(MICMD, 0x00);
	
	// get data value
	data  = enc28j60Read(MIRDL);
	data |= enc28j60Read(MIRDH);
	// return the data
	return data;
}

void enc28j60PhyWrite(uint8_t address, uint16_t data)
{
	// set the PHY register address
	enc28j60Write(MIREGADR, address);
	
	// write the PHY data
	enc28j60Write(MIWRL, data);	
	enc28j60Write(MIWRH, data>>8);

	// wait until the PHY write completes
	while(enc28j60Read(MISTAT) & MISTAT_BUSY);
}

void enc28j60Init(struct uip_eth_addr *mac)
{
	// initialize I/O
	sbi(CONFIG_DRIVERS_ENC28J60_CTL_DDR, CONFIG_DRIVERS_ENC28J60_CTL_PIN);
	sbi(CONFIG_DRIVERS_ENC28J60_CTL_PORT, CONFIG_DRIVERS_ENC28J60_CTL_PIN);

	// setup SPI I/O pins
	sbi(CONFIG_DRIVERS_SPI_PORT, CONFIG_DRIVERS_SPI_SCK);	// set SCK hi
	sbi(CONFIG_DRIVERS_SPI_DDR, CONFIG_DRIVERS_SPI_SCK);	// set SCK as output
	cbi(CONFIG_DRIVERS_SPI_DDR, CONFIG_DRIVERS_SPI_MISO);	// set MISO as input
	sbi(CONFIG_DRIVERS_SPI_DDR, CONFIG_DRIVERS_SPI_MOSI);	// set MOSI as output
	sbi(CONFIG_DRIVERS_SPI_DDR, CONFIG_DRIVERS_SPI_SS);		// SS must be output for Master mode to work
	// initialize SPI interface
	// master mode
	sbi(SPCR, MSTR);
	// select clock phase positive-going in middle of data
	cbi(SPCR, CPOL);
	// Data order MSB first
	cbi(SPCR,DORD);
	// switch to f/4 2X = f/2 bitrate
	cbi(SPCR, SPR0);
	cbi(SPCR, SPR1);
	sbi(SPSR, SPI2X);
	// enable SPI
	sbi(SPCR, SPE);

	// perform system reset
	enc28j60WriteOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
	// check CLKRDY bit to see if reset is complete
//	_delay_us(50);
//	while(!(enc28j60Read(ESTAT) & ESTAT_CLKRDY));
	_delay_ms(1); // Silicon errata

	// do bank 0 stuff
	// initialize receive buffer
	// 16-bit transfers, must write low byte first
	// set receive buffer start address
	NextPacketPtr = RXSTART_INIT;
	enc28j60Write(ERXSTL, RXSTART_INIT&0xFF);
	enc28j60Write(ERXSTH, RXSTART_INIT>>8);
	// set receive pointer address
	enc28j60Write(ERXRDPTL, RXSTART_INIT&0xFF);
	enc28j60Write(ERXRDPTH, RXSTART_INIT>>8);
	// set receive buffer end
	// ERXND defaults to 0x1FFF (end of ram)
	enc28j60Write(ERXNDL, RXSTOP_INIT&0xFF);
	enc28j60Write(ERXNDH, RXSTOP_INIT>>8);
	// set transmit buffer start
	// ETXST defaults to 0x0000 (beginnging of ram)
	enc28j60Write(ETXSTL, TXSTART_INIT&0xFF);
	enc28j60Write(ETXSTH, TXSTART_INIT>>8);

	// do bank 2 stuff
	// enable MAC receive
	enc28j60Write(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
	// bring MAC out of reset
	enc28j60Write(MACON2, 0x00);
	// enable automatic padding and CRC operations
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
//	enc28j60Write(MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
	// set inter-frame gap (non-back-to-back)
	enc28j60Write(MAIPGL, 0x12);
	enc28j60Write(MAIPGH, 0x0C);
	// set inter-frame gap (back-to-back)
	enc28j60Write(MABBIPG, 0x12);
	// Set the maximum packet size which the controller will accept
	enc28j60Write(MAMXFLL, MAX_FRAMELEN&0xFF);	
	enc28j60Write(MAMXFLH, MAX_FRAMELEN>>8);

	// do bank 3 stuff
	// write MAC address
	// NOTE: MAC address in ENC28J60 is byte-backward
	enc28j60Write(MAADR5, mac->addr[0]);
	enc28j60Write(MAADR4, mac->addr[1]);
	enc28j60Write(MAADR3, mac->addr[2]);
	enc28j60Write(MAADR2, mac->addr[3]);
	enc28j60Write(MAADR1, mac->addr[4]);
	enc28j60Write(MAADR0, mac->addr[5]);

	// no loopback of transmitted frames
	enc28j60PhyWrite(PHCON2, PHCON2_HDLDIS);

	// switch to bank 0
	enc28j60SetBank(ECON1);
	// enable interrutps
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE|EIE_PKTIE);
	// enable packet reception
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);
/*
	enc28j60PhyWrite(PHLCON, 0x0AA2);

	// setup duplex ----------------------

	// Disable receive logic and abort any packets currently being transmitted
	enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS|ECON1_RXEN);
	
	{
		uint16_t temp;
		// Set the PHY to the proper duplex mode
		temp = enc28j60PhyRead(PHCON1);
		temp &= ~PHCON1_PDPXMD;
		enc28j60PhyWrite(PHCON1, temp);
		// Set the MAC to the proper duplex mode
		temp = enc28j60Read(MACON3);
		temp &= ~MACON3_FULDPX;
		enc28j60Write(MACON3, temp);
	}

	// Set the back-to-back inter-packet gap time to IEEE specified 
	// requirements.  The meaning of the MABBIPG value changes with the duplex
	// state, so it must be updated in this function.
	// In full duplex, 0x15 represents 9.6us; 0x12 is 9.6us in half duplex
	//enc28j60Write(MABBIPG, DuplexState ? 0x15 : 0x12);	
	
	// Reenable receive logic
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);

	// setup duplex ----------------------
*/
}

void enc28j60PacketSend(unsigned int len1, unsigned char* packet1, unsigned int len2, unsigned char* packet2)
{
	//Errata: Transmit Logic reset
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRST);
	enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRST);

	// Set the write pointer to start of transmit buffer area
	enc28j60Write(EWRPTL, TXSTART_INIT&0xff);
	enc28j60Write(EWRPTH, TXSTART_INIT>>8);
	// Set the TXND pointer to correspond to the packet size given
	enc28j60Write(ETXNDL, (TXSTART_INIT+len1+len2));
	enc28j60Write(ETXNDH, (TXSTART_INIT+len1+len2)>>8);

	// write per-packet control byte
	enc28j60WriteOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);

	// copy the packet into the transmit buffer
	enc28j60WriteBuffer(len1, packet1);
	if(len2>0) enc28j60WriteBuffer(len2, packet2);
	
	// send the contents of the transmit buffer onto the network
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
}

unsigned int enc28j60PacketReceive(unsigned int maxlen, unsigned char* packet)
{
	uint16_t rxstat;
	uint16_t len;

	// check if a packet has been received and buffered
//	if( !(enc28j60Read(EIR) & EIR_PKTIF) )
	if( !enc28j60Read(EPKTCNT) )
		return 0;
	
	// Make absolutely certain that any previous packet was discarded	
	//if( WasDiscarded == FALSE)
	//	MACDiscardRx();

	// Set the read pointer to the start of the received packet
	enc28j60Write(ERDPTL, (NextPacketPtr));
	enc28j60Write(ERDPTH, (NextPacketPtr)>>8);
	// read the next packet pointer
	NextPacketPtr  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
	NextPacketPtr |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;
	// read the packet length
	len  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
	len |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;
	// read the receive status
	rxstat  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
	rxstat |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;

	// limit retrieve length
	// (we reduce the MAC-reported length by 4 to remove the CRC)
	len = MIN(len, maxlen);

	// copy the packet from the receive buffer
	enc28j60ReadBuffer(len, packet);

	// Move the RX read pointer to the start of the next received packet
	// This frees the memory we just read out
//	enc28j60Write(ERXRDPTL, (NextPacketPtr));
//	enc28j60Write(ERXRDPTH, (NextPacketPtr)>>8);
	if (NextPacketPtr == RXSTART_INIT) {
		enc28j60Write(ERXRDPTL, (RXSTOP_INIT) & 0xff);
		enc28j60Write(ERXRDPTH, (RXSTOP_INIT)>>8);
	}
	else {
		enc28j60Write(ERXRDPTL, (NextPacketPtr - 1));
		enc28j60Write(ERXRDPTH, (NextPacketPtr - 1)>>8);
	}

	// decrement the packet counter indicate we are done with this packet
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);

	return len;
}

