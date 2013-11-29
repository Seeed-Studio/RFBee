//  CCx.cpp  Class to control the chipcon CCxxxx series transceivers
//  see http://focus.ti.com/lit/ds/symlink/cc1101.pdf for details on the CC1101

//  Copyright (c) 2010 Hans Klunder <hans.klunder (at) bigfoot.com>
//  Author: Hans Klunder, based on the original Rfbee v1.0 firmware by Seeedstudio
//  Version: May 22, 2010
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

#include "Arduino.h"
#include "CCx.h"
#include "CCxCfg.h"
#include "Spi.h"


//---------- constructor ----------------------------------------------------

CCX::CCX():lastCalibrate(0)
{}

// Power On Reset as described in  19.1.2 of cc1100 datasheet, tried APOR as described in 19.1.1 but that did not work :-(
void CCX::PowerOnStartUp()
{
  DEBUGPRINT() 
  Spi.mode((1 << SPR1) | (1 << SPR0));//SPICLK=CPU/64

  // start manual Power On Reset 
  Spi.slaveSelect(HIGH);
  delayMicroseconds(1);
  
  Spi.slaveSelect(LOW);
  delayMicroseconds(10);
  
  Spi.slaveSelect(HIGH);
  delayMicroseconds(41);
  
  Spi.slaveSelect(LOW);
  
  // wait for MISO to go low
  while(digitalRead(MISO_PIN));
  
  Spi.transfer(CCx_SRES);
 
  DEBUGPRINT("Waiting for CCx to complete POR") 

  // wait for MISO to go low
  while(digitalRead(MISO_PIN));
  
  Spi.slaveSelect(HIGH);
  
  DEBUGPRINT("CCx POR complete") 
}

byte CCX::Read(byte addr,byte* data)
{
  DEBUGPRINT() 
  byte result;
  
  Spi.slaveSelect(LOW);
  // wait for MISO to go low
  while(digitalRead(MISO_PIN));
  
  result=Spi.transfer(addr | 0x80);
  *data=Spi.transfer(0);

  Spi.slaveSelect(HIGH);
  return result;
}

byte CCX::ReadBurst(byte addr, byte* dataPtr, byte size)
{
  DEBUGPRINT() 
  byte result;
  
  Spi.slaveSelect(LOW);
  // wait for MISO to go low
  while(digitalRead(MISO_PIN));
  
  result=Spi.transfer(addr | 0xc0);

  while(size)
  {
    *dataPtr++ = Spi.transfer(0);
    size--;
  }
  
  Spi.slaveSelect(HIGH);
  
  return result;
}

byte CCX::Write(byte addr, byte dat)
{
  DEBUGPRINT() 
  byte result;
  
  Spi.slaveSelect(LOW);
  // wait for MISO to go low
  while(digitalRead(MISO_PIN));
  
  result=Spi.transfer(addr);
  result=Spi.transfer(dat);

  Spi.slaveSelect(HIGH);
  
  return result;
}

byte CCX::WriteBurst(byte addr, const byte* dataPtr, byte size)
{
  DEBUGPRINT() 
  byte result;
  
  Spi.slaveSelect(LOW);
  // wait for MISO to go low
  while(digitalRead(MISO_PIN));
  
  result=Spi.transfer(addr | 0x40);

  while(size)
  {
    result = Spi.transfer(*dataPtr++);
    size--;
  }
  
  Spi.slaveSelect(HIGH);
  
  return result;
}

byte CCX::Strobe(byte addr)
{
  DEBUGPRINT() 
  byte result;

  Spi.slaveSelect(LOW);
  // wait for MISO to go low
  while(digitalRead(MISO_PIN));
  
  result=Spi.transfer(addr);
  
  Spi.slaveSelect(HIGH);
  
  return result;
}

//configure registers of cc1100 making it work in specific mode
void CCX::Setup(byte configId)
{
  DEBUGPRINT() 
  byte reg;
  byte val;
  if (configId < CCX_NR_OF_CONFIGS)
      for(byte i = 0; i< CCX_NR_OF_REGISTERS; i++){
        reg=pgm_read_byte(&CCx_registers[i]);
        val=pgm_read_byte(&CCx_registerSettings[configId][i]);//read flash data no problem
        byte temp = Write(reg,val);
        //Serial.print(temp,HEX);
        //Serial.print(" ");
      }
}


// to aid debugging
//#ifdef DEBUG
void CCX::ReadSetup()
{
  DEBUGPRINT()
  byte reg;
  byte value;
  for(byte i = 0; i< CCX_NR_OF_REGISTERS; i++){
    reg=pgm_read_byte(&CCx_registers[i]);
    Read(reg,&value);
    Serial.print(reg,HEX);
    Serial.print(':');
    Serial.println(value,HEX);
  }
}
//#endif

void CCX::setPA(byte configId,byte paIndex)
{
  DEBUGPRINT()
  byte PAval=pgm_read_byte(&CCx_paTable[configId][paIndex]);
  CCx.Write(CCx_PATABLE,PAval);
}


void CCX::Mode(byte md){

}

byte CCX::NrOfConfigs(){
  return CCX_NR_OF_CONFIGS;
}

byte CCX::RSSIdecode(byte rssiEnc){
  byte rssi;
  byte rssiOffset=74;  // is actually dataRate dependant, but for simplicity assumed to be fixed.

  // RSSI is coded as 2's complement see section 17.3 RSSI of the cc1100 datasheet
  if (rssiEnc >= 128)
    rssi = (( rssiEnc - 256) >> 1) - rssiOffset;
  else
    rssi = (rssiEnc >> 1) - rssiOffset;
  return rssi;
}

//---------- preinstantiate CCx object --------------------------------------

CCX CCx = CCX();

