//  rfBeeCore.cpp core routines for the rfBee
//  see www.seeedstudio.com for details and ordering rfBee hardware.

//  Copyright (c) 2010 Hans Klunder <hans.klunder (at) bigfoot.com>
//  Author: Hans Klunder, based on the original Rfbee v1.0 firmware by Seeedstudio
//  Version: July 14, 2010
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include "rfBeeCore.h"

// send data via RF
void transmitData(byte *txData, byte len, byte srcAddress, byte destAddress)
{
  DEBUGPRINT()
  byte stat;
  unsigned long t; 

  //Serial.println(len,DEC);
  CCx.Strobe(CCx_SIDLE); 
  CCx.Strobe(CCx_SFTX); 
  CCx.Write(CCx_TXFIFO, len + 2);
  CCx.Write(CCx_TXFIFO, destAddress);
  CCx.Write(CCx_TXFIFO, srcAddress);
  CCx.WriteBurst(CCx_TXFIFO, txData, len); // write len bytes of the serialData buffer into the CCx txfifo
  CCx.Strobe(CCx_STX);

  t = millis(); 
  
  while (1)
  {
    byte size;
    stat = CCx.Read(CCx_TXBYTES, &size);

    if (0 == (size & 0x7f))
    {
      break;
    } 
    else if (millis() - t > 100)
    {
      CCx.Strobe(CCx_SFTX);
      DEBUGPRINT("timeout")
      break;
    }
  }

#ifdef DEBUG
  txData[len] = '\0';
  Serial.println((char *)txData);
  Serial.println("----");
#endif

}

// read available txFifo size and handle underflow (which should not have occured anyway)
byte txFifoFree()
{
  DEBUGPRINT()
  byte stat;
  byte size;

  stat = CCx.Read(CCx_TXBYTES, &size);
  // handle a potential TX underflow by flushing the TX FIFO as described in section 10.1 of the CC 1100 datasheet
  if (size >= 64) //state got here seems not right, so using size to make sure it no more than 64
  {
    CCx.Strobe(CCx_SFTX);
    stat = CCx.Read(CCx_TXBYTES, &size);
#ifdef DEBUG
    Serial.println(stat, HEX);
#endif
  }
#ifdef DEBUG
  //Serial.println(stat,HEX);
  Serial.println(CCx_FIFO_SIZE - size, DEC);
#endif
  return (CCx_FIFO_SIZE - size);
}

// receive data via RF, rxData must be at least CCx_PACKT_LEN bytes long
int receiveData(byte *rxData, byte *len, byte *srcAddress, byte *destAddress, byte *rssi, byte *lqi)
{
  DEBUGPRINT()

  byte stat;

  stat = CCx.Read(CCx_RXFIFO, len);
#ifdef DEBUG
  Serial.print("length:");
  Serial.println(*len, DEC);
#endif
  CCx.Read(CCx_RXFIFO, destAddress);
  CCx.Read(CCx_RXFIFO, srcAddress);
  *len -= 2;  // discard address bytes from payloadLen
  CCx.ReadBurst(CCx_RXFIFO, rxData, *len);
  CCx.Read(CCx_RXFIFO, rssi);
  *rssi = CCx.RSSIdecode(*rssi);
  stat = CCx.Read(CCx_RXFIFO, lqi);
  // check checksum ok
  if ((*lqi & 0x80) == 0)
  {
    return NOTHING;
  }
  *lqi = *lqi & 0x7F; // strip off the CRC bit

  // handle potential RX overflows by flushing the RF FIFO as described in section 10.1 of the CC 1100 datasheet
  if ((stat & 0xF0) == 0x60) //Modified by Icing. When overflows, STATE[2:0] = 110
  {
    errNo = 3; //Error RX overflow
    CCx.Strobe(CCx_SFRX); // flush the RX buffer
    CCx.Strobe(CCx_SIDLE); 
    CCx.Strobe(CCx_SRX); 
    return ERR;
  }
  return OK;
}

// power saving
void sleepNow(byte mode)
{
  /* Now is the time to set the sleep mode. In the Atmega8 datasheet
   * http://www.atmel.com/dyn/resources/prod_documents/doc2486.pdf on page 35
   * there is a list of sleep modes which explains which clocks and 
   * wake up sources are available in which sleep modus.
   *
   * In the avr/sleep.h file, the call names of these sleep modus are to be found:
   *
   * The 5 different modes are:
   *     SLEEP_MODE_IDLE         -the least power savings 
   *     SLEEP_MODE_ADC
   *     SLEEP_MODE_PWR_SAVE
   *     SLEEP_MODE_STANDBY
   *     SLEEP_MODE_PWR_DOWN     -the most power savings
   *
   *  the power reduction management <avr/power.h>  is described in 
   *  http://www.nongnu.org/avr-libc/user-manual/group__avr__power.html
   */
  DEBUGPRINT()

  set_sleep_mode(mode);   // sleep mode is set here

  sleep_enable();          // enables the sleep bit in the mcucr register
                           // so sleep is possible. just a safety pin

  power_adc_disable();
  power_spi_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_twi_disable();


  sleep_mode();            // here the device is actually put to sleep!!

  // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP
  sleep_disable();         // first thing after waking from sleep:
                           // disable sleep...

  power_all_enable();

}

void lowPowerOn()
{
  DEBUGPRINT()
  CCx.Write(CCx_WORCTRL, 0x78);  // set WORCRTL.RC_PD to 0 to enable the wakeup oscillator
  CCx.Strobe(CCx_SWOR);
  sleepNow(SLEEP_MODE_IDLE);
  //CCx.Strobe(CCx_SIDLE);
}





