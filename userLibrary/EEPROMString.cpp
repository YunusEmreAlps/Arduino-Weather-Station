#include <Arduino.h>
#include <EEPROM.h>
#include <EEPROMString.h>

EEPROMString::EEPROMString(int address){
  _address = address;
}

void EEPROMString::writing(String text){
  _text = text;
  data = _text;
  txtSize = data.length();
  for(i=0; i<txtSize; i++)
  {
    EEPROM.write(_address+i, data[i]);
  }
  EEPROM.write(_address+txtSize, '\0');
}

String EEPROMString::reading(){
  char resultText[100];
  unsigned char letter = EEPROM.read(_address); // First character
  while((letter != '\0') && (txtSize < 100))
  {
    letter = EEPROM.read(_address+txtSize);
    resultText[txtSize] = letter;
    txtSize++;
  }
  resultText[txtSize] = '\0';
  return resultText;
}
