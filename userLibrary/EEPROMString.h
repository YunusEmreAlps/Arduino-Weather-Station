#ifndef EEPROMString_h
#define EEPROMString_h

#include <Arduino.h>

class EEPROMString
{
public:
  EEPROMString(int address);
  void writing(String text);
  String reading();
private:
  int _address;
  String _text;  
  int volatile i;                                           // Loop variable
  int volatile txtSize = 0;                                 // String length
  String volatile data;                                     // String
};
#endif
