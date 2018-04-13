//  rfBeeSerial.pde serial interface to rfBee
//  see www.seeedstudio.com for details and ordering rfBee hardware.

//  Copyright (c) 2010 Hans Klunder <hans.klunder (at) bigfoot.com>
//  Author: Hans Klunder, based on the original Rfbee v1.0 firmware by Seeedstudio
//  Version: August 18, 2010
//  ChangeLog: Jack Shao, seeedstudio, 2013/8
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



#include "rfBeeSerial.h"

//
byte getNumParamData(int *result, int size)
{
  DEBUGPRINT()
  // try to read a number
  byte c;
  int value = 0;
  boolean valid = false;
  int pos = 4; // we start to read at pos 5 as 0-1 = AT and 2-3 = CMD

  if (serialData[pos] == SERIALCMDTERMINATOR)  // no data was available
    return NOTHING;

  while (size-- > 0)
  {
    c = serialData[pos++];
    if (c == SERIALCMDTERMINATOR)  // no more data available
      break;
    if ((c < '0') || (c > '9'))     // illegal char
      return ERR;
    // got a digit
    valid = true;
    value = (value * 10) + (c - '0');
  }
  if (valid)
  {
    *result = value;
    return OK;
  }
  return ERR;
}

// simple standardized setup for commands that only modify config
int modifyConfig(byte configLabel, byte paramSize, byte paramMaxValue)
{
  DEBUGPRINT()
    int param;

  byte result = getNumParamData(&param, paramSize);
  if (result == OK)
  {
    if (param <= paramMaxValue)
    {
      Config.set(configLabel, param);
      return MODIFIED;
    }
  }
  if (result == NOTHING)
  {
    // return current setting
    Serial.println(Config.get(configLabel), DEC);
    return (OK);
  }
  return ERR;
}

int processSerialCmd(byte size)
{
  DEBUGPRINT()
    int result = MODIFIED;

  byte configItem;   // the ID used in the EEPROM
  byte paramDigits;  // how many digits for the parameter
  byte maxValue;     // maximum value of the parameter
  byte postProcess;  // do we need to call the function to perform extra actions on change
  AT_Command_Function_t function; // the function which does the real work on change

  // read the AT
  if (strncasecmp("AT", (char *)serialData, 2) == 0)
  {
    // read the command
    for (int i = 0; i <= sizeof(atCommands) / sizeof(AT_Command_t); i++)
    {
      // do we have a known command
      if (strncasecmp_P((char *)serialData + 2, (PGM_P)pgm_read_word(&(atCommands[i].name)), 2) == 0)
      {
        // get the data from PROGMEM
        configItem = pgm_read_byte(&(atCommands[i].configItem));
        paramDigits = pgm_read_byte(&(atCommands[i].paramDigits));
        maxValue = pgm_read_byte(&(atCommands[i].maxValue));
        postProcess = pgm_read_byte(&(atCommands[i].postProcess));
        function = (AT_Command_Function_t)pgm_read_word(&(atCommands[i].function));
        if (paramDigits > 0) result = modifyConfig(configItem, paramDigits, maxValue);
        if (result == MODIFIED)
        {
          result = OK;  // config only commands always return OK
          if (postProcess == true) result = function();  // call the command function
        }
        return (result);  // return the result of the execution of the function linked to the command
      }
    }
  }
  return ERR;
}

void readSerialCmd()
{
  DEBUGPRINT()
    int result;
  char data;
  static byte pos = 0;

  while (Serial.available() && (serialMode == SERIALCMDMODE))      //ATSL changes commandmode while there is a char waiting in the serial buffer.
  {
    result = NOTHING;
    data = Serial.read();
    serialData[pos++] = data; //serialData is our global serial buffer
    if (data == SERIALCMDTERMINATOR)
    {
      if (pos > 3) // we need 4 bytes
      {
        result = processSerialCmd(pos);
      } else result = ERR;
      pos = 0;
    }
    // check if we don't overrun the buffer, if so empty it
    if (pos > BUFFLEN)
    {
      result = ERR;
      pos = 0;
    }
    if (result == OK) Serial.println("ok");
    if (result == ERR) Serial.println("error");
  }
}
void txDataFromSerialToRf()
{
  DEBUGPRINT()
  static byte h = 0;  //push char into head
  static byte t = 0;  //pop char out of tail
  static byte plus_cnt = 0;
  byte ring_len = 0;
  byte free_len = 0;
  byte d;

  ring_len = (h >= t) ? (h - t) : (h + BUFFLEN - t);
  free_len = BUFFLEN - ring_len;
  DEBUGPRINT(ring_len)

  //fill in data
  while (free_len > 0 && Serial.available() > 0)
  {
    d = Serial.read();
    if (d == '+') plus_cnt++;
    else
    {
      plus_cnt = 0;
      //advance head
/*
      if (h == t)               
      {                         
        Serial.println("impsb");
        break;                  
      }                         
*/
      serialData[h] = d; 
      h = (h + 1) % BUFFLEN; 
      ring_len++;
      free_len--;
    }

    if (plus_cnt == 3)
    {
      plus_cnt = 0;
      serialMode = SERIALCMDMODE;
      Serial.println("ok, starting cmd mode");
      break;  // jump out of the loop, but still send the remaining chars in the buffer
    }
  }

  DEBUGPRINT(ring_len)
  
  //tx out sth.
  if (ring_len > Config.get(CONFIG_TX_THRESHOLD)) 
  {
    byte rfBeeMode = Config.get(CONFIG_RFBEE_MODE);   
    if (rfBeeMode == TRANSMIT_MODE || rfBeeMode == TRANSCEIVE_MODE)
    {
      byte tx_len = txFifoFree();
      tx_len = (ring_len > tx_len) ? tx_len : ring_len;
      if (tx_len == 0) return;
      
      DEBUGPRINT(tx_len)
      byte buf[BUFFLEN + 1];  //wish the ram is ok...
      for (int i = 0; i < tx_len; i++)
      {
/*
        if (t == h)                
        {                          
          Serial.println("impsb2");
          break;                   
        }                          
*/
        *(buf + i) = serialData[t]; 
        t = (t + 1) % BUFFLEN; 
      }
      transmitData(buf, tx_len, Config.get(CONFIG_MY_ADDR), Config.get(CONFIG_DEST_ADDR)); 
    }
  }
  //end
}
//sorry, but i dont think the implementation is a good one -shao,T_T
/*{
  DEBUGPRINT()
  byte len;
  byte data;
  byte fifoSize=0;
  static byte plus=0;
  static byte pos=0;
  byte rfBeeMode;
 
  // insert any plusses from last round
  for(int i=pos; i< plus;i++) //be careful, i should start from pos, -changed by Icing
    serialData[i]='+';
  
  len=Serial.available()+plus+pos;
  if (len > BUFFLEN ) len=BUFFLEN; //only process at most BUFFLEN chars
  // check how much space we have in the TX fifo
  fifoSize=txFifoFree();
  
  if(fifoSize <= 0){
    Serial.flush();
    //CCx.Strobe(CCx_SFTX);
    plus = 0;
    pos = 0;
    return;
  }
  if (len > fifoSize)  len=fifoSize;  // don't overflow the TX fifo
    
  for(byte i=plus+pos; i< len;i++){
    data=Serial.read();
    serialData[i]=data;  //serialData is our global serial buffer
    if (data == '+')
      plus++;
    else
      plus=0;
 
    if (plus == 3){
      len=i-2; // do not send the last 2 plusses
      plus=0;
      serialMode=SERIALCMDMODE;
      CCx.Strobe(CCx_SIDLE); 
      Serial.println("ok, starting cmd mode");
      break;  // jump out of the loop, but still send the remaining chars in the buffer 
    }
  }
  
  if (plus > 0)  // save any trailing plusses for the next round
    len-=plus;
   
  // check if we have more input than the transmitThreshold, if we have just switched to commandmode send  the current buffer anyway.
  if ((serialMode!=SERIALCMDMODE)  && (len < Config.get(CONFIG_TX_THRESHOLD))){
    pos=len;  // keep the current bytes in the buffer and wait till next round.
    return;
  }
  
  if (len > 0){
    rfBeeMode=Config.get(CONFIG_RFBEE_MODE);   
    //only when TRANSMIT_MODE or TRANSCEIVE,transmit the buffer data,otherwise ignore
    if( rfBeeMode == TRANSMIT_MODE || rfBeeMode == TRANSCEIVE_MODE )                             
        transmitData(serialData,len,Config.get(CONFIG_MY_ADDR),Config.get(CONFIG_DEST_ADDR)); 
    pos=0; // serial databuffer is free again.
  }
}*/

// write a text label from progmem, uses less bytes than individual Serial.println()
//void writeSerialLabel(byte i){
//  char buffer[64];
//  strcpy_P(buffer, (char * )pgm_read_word(&(labels[i])));
//  Serial.println(buffer);
//}

//
void writeSerialError()
{
  DEBUGPRINT()
    char buffer[64];
  strcpy_P(buffer, (char *)pgm_read_word(&(error_codes[errNo])));
  Serial.print("error: ");
  Serial.println(buffer);
}



// read data from CCx and write it to Serial based on the selected output format
void rxDataFromRfToSerial()
{
  DEBUGPRINT()
  byte rxData[CCx_PACKT_LEN];
  byte len;
  byte srcAddress;
  byte destAddress;
  char rssi;
  byte lqi;
  int result;
  byte of;


  result = receiveData(rxData, &len, &srcAddress, &destAddress, (byte *)&rssi, &lqi);
  DEBUGPRINT(result)

  if (result == ERR)
  {
    writeSerialError();
    return;
  }

  if (result == NOTHING) return;
// write to serial based on output format:
//  0: payload only
//  1: source, dest, payload
//  2: payload len, source, dest, payload, rssi, lqi
//  3: payload len, source, dest, payload,",", rssi (DEC),",",lqi (DEC)
  of = Config.get(CONFIG_OUTPUT_FORMAT);
  if (of == 3)
  {
    Serial.print(len, DEC); // len is size of data EXCLUDING address bytes !
    Serial.print(',');
    // write source & destination
    Serial.print(srcAddress, DEC);
    Serial.print(',');
    Serial.print(destAddress, DEC);
    Serial.print(',');
    // write data
    Serial.write(rxData, len);
    Serial.print(',');
    // write rssi en lqi
    Serial.print((int)rssi);
    Serial.print(',');
    Serial.println(lqi, DEC);
  } else
  {
    if (of > 1) Serial.print(len); // len is size of data EXCLUDING address bytes !
    if (of > 0)
    {
      // write source & destination
      Serial.print(srcAddress);
      Serial.print(destAddress);
    }
    // write data
    Serial.write(rxData, len);
    // write rssi en lqi
    if (of > 1)
    {
      Serial.print(rssi);
      Serial.print(lqi);
    }
  }
}


int setMyAddress()
{
  DEBUGPRINT();
  CCx.Write(CCx_ADDR, Config.get(CONFIG_MY_ADDR));
  return OK;
}

int setAddressCheck()
{
  DEBUGPRINT();
  CCx.Write(CCx_PKTCTRL1, Config.get(CONFIG_ADDR_CHECK) | 0x04);
  return OK;
}

int setPowerAmplifier()
{
  DEBUGPRINT();
  CCx.setPA(Config.get(CONFIG_CONFIG_ID), Config.get(CONFIG_PAINDEX));
  return OK;
}

int changeUartBaudRate()
{
  DEBUGPRINT()
  Serial.println("ok");
  Serial.flush();
  delay(1);
  setUartBaudRate();
}

int setUartBaudRate()
{
  Serial.begin(pgm_read_dword(&baudRateTable[Config.get(CONFIG_BDINDEX)]));
  return NOTHING;  // we already sent an ok.
}

int showFirmwareVersion()
{
  DEBUGPRINT()
  Serial.println(((float)FIRMWAREVERSION) / 10, 1);
  return OK;
}

int showHardwareVersion()
{
  DEBUGPRINT()
  Serial.println(((float)Config.get(CONFIG_HW_VERSION)) / 10, 1);
  return OK;
}

int resetConfig()
{
  DEBUGPRINT()
  Config.reset();
  return OK;
}

int setCCxConfig()
{
  DEBUGPRINT()
  // load the appropriate config table
  byte cfg = Config.get(CONFIG_CONFIG_ID);
  CCx.Setup(cfg);
  //CCx.ReadSetup();
  // and restore the config settings
  setMyAddress();
  setAddressCheck();
  setPowerAmplifier();
  setRFBeeMode(); 
  CCx.lastCalibrate = millis();   //add by shao
  return OK;
}


int setSerialDataMode()
{
  DEBUGPRINT()
  serialMode = SERIALDATAMODE;
  return OK;
}

int setRFBeeMode()
{
  DEBUGPRINT()
  // CCx_MCSM1 is configured to have TX and RX return to proper state on completion or timeout
  switch (Config.get(CONFIG_RFBEE_MODE))
  {
  case TRANSMIT_MODE:
    CCx.Strobe(CCx_SIDLE);
    delay(1);
    CCx.Write(CCx_MCSM1,   0x00); //TXOFF_MODE->stay in IDLE
    CCx.Strobe(CCx_SFTX);
    break;
  case RECEIVE_MODE:
    CCx.Strobe(CCx_SIDLE);
    delay(1);
    CCx.Write(CCx_MCSM1,   0x0c); //RXOFF_MODE->stay in rx
    CCx.Strobe(CCx_SFRX);
    CCx.Strobe(CCx_SRX);
    break;
  case TRANSCEIVE_MODE:
    CCx.Strobe(CCx_SIDLE);
    delay(1);
    CCx.Write(CCx_MCSM1,   0x0f); //RXOFF_MODE and TXOFF_MODE goto RX  
    CCx.Strobe(CCx_SFTX);
    CCx.Strobe(CCx_SFRX);
    CCx.Strobe(CCx_SRX);
    break;
  case LOWPOWER_MODE:
    CCx.Strobe(CCx_SIDLE);
    break;
  default:
    break;
  }
  return OK;
}

// put the rfbee into sleep
int setSleepMode()
{
  DEBUGPRINT("going to sleep")
  CCx.Strobe(CCx_SIDLE);
  CCx.Strobe(CCx_SPWD);
  sleepNow(SLEEP_MODE_IDLE);
  //sleepNow(SLEEP_MODE_PWR_DOWN);
  DEBUGPRINT("just woke up")
  setRFBeeMode();
  setSerialDataMode();
  return NOTHING;
}


