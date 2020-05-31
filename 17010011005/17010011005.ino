// Libraries
#include <LCD5110_Basic.h>          // Nokia 5110 LCD library
#include <Adafruit_BMP085.h>        // Barometric Temperature/Altitude/Pressure Sensor library
#include <EEPROMString.h>           // User Defined library
#include <EEPROM.h>
#include <dht11.h>                  // Humidity/Temperature Sensor library
#include <Wire.h>                   // I2C library


// Declare Which Fonts we will be using
extern uint8_t SmallFont[];         // 6x8 Pixels
extern uint8_t MediumNumbers[];     // 12x16 Pixels
extern uint8_t BigNumbers[];        // 14x24 Pixels


// Weather Condition Icon
extern uint8_t sunny[];
extern uint8_t rainy[];
extern uint8_t snowy[];
extern uint8_t cloudy[];
extern uint8_t partly_cloudy[];


// Variables
volatile boolean buttonState = false;
volatile boolean systemState = false;   

// Button  -> System
// False   -> ON  
// True    -> OFF

// Pin            
int ldrPin = A0;                     // Light Dependent Resistor (Analog Pin)
int waterSensorPin = A1;
int buttonPin = 2;                   // Push Button (Digital Pin)
int redPin = 4;
int greenPin = 5;
int bluePin = 6;
int dht11Pin = 7;                    // Temperature/Humidity Sensor (Digital Pin)
int backlightPin = 13;               // Nokia 5110 LCD Back Light (Digital Pin)

// Measurement
volatile int ldrValue = 0;
volatile int counter = 1;
int waterValue = 0;
                                                 
int forecast[5] = {0, 0, 0, 0, 0};   // Pressure

// forecast[0]  ->  Current Pressure 
// forecast[1]  ->  Difference Max-Min
// forecast[2]  ->  Max Pressure
// forecast[3]  ->  Min Pressure  
// forecast[4]  ->  Pressure Dynamic (1 - Up, 2 - Down, 0 - Equal)                                        
  
volatile float datatypef;                   
volatile float norma = 0;            // Pressure Calculation Value
volatile float altitudeValue = 0;    // (BMP180)
volatile float pressureValue = 0;    // (BMP180) 
volatile float dht11Value = 0;       // (DHT11)
volatile float humidityValue = 0;    // (DHT11) Humidity    -> %
volatile float temperatureValue = 0; // (DHT11) Temperature -> Celcius
                                          
String networkName = "TurkTelekom_TB74B";
String networkPassword = "*****************";  // Your Network Password
String ip = "184.106.153.149";      //Thingspeak IP
String conditionName;
    
// EEPROM Address
volatile int dateAddress = 150;
volatile int timeAddress = 200;
volatile int humidityAddress = 250;
volatile int temperatureAddress = 300;
volatile int altitudeAddress = 350;                                                    
volatile int pressureAddress = 400;  
volatile int pressureCalculationAddress = 450;

                                       
// LCD                                  
LCD5110 myGLCD(8, 9, 10, 11, 12);    // Nokia 5110 LCD pins

// BMP180  
Adafruit_BMP085 bmp; // SCL(21), SDA(20)

// EEPROMString (User Defined library)               
EEPROMString dateValue (dateAddress);
EEPROMString timeValue (timeAddress);
        
// DHT11 
dht11 DHT11;

        
void setup()
{
  Serial.begin(9600);                  // Serial Data Transmission
 
  pinMode(buttonPin, INPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(backlightPin, OUTPUT);       // Nokia 5110 LCD Back Light

  // Red (Dangerous)
  analogWrite(redPin, 0);
  analogWrite(greenPin, 255);
  analogWrite(bluePin, 255);

  // ESP8266
  Serial1.begin(115200);
  Serial1.println("AT");                                          
  Serial.println(" - AT");
  while(!Serial1.find("OK")){                              
    Serial1.println("AT");
    Serial.println(" - ESP8266 Not Found.");
  }
  Serial.println(" - OK");
  Serial1.println("AT+CWMODE=1");                                 
  while(!Serial1.find("OK")){                                   
    Serial1.println("AT+CWMODE=1");
    Serial.println(" - Setting the Mode ...");
  }
  Serial.println(" - Client");
  Serial.println(" - Connection Internet...");
  Serial1.println("AT+CWJAP=\""+networkName+"\",\""+networkPassword+"\"");    
  while(!Serial1.find("OK"));                                     
  Serial.println(" - Connection success");

   // Green
   analogWrite(redPin, 255);
   analogWrite(greenPin, 0);
   analogWrite(bluePin, 255); 
   delay (3000);
   analogWrite(redPin, 255);
   analogWrite(greenPin, 255);
   analogWrite(bluePin, 255);    
  
  // Nokia 5110 LCD
  myGLCD.InitLCD();                    // Initialize the display
  myGLCD.disableSleep();               // Sleep Mode OFF
  myGLCD.setContrast(70);

  // BMP180 Sensor Control
  if (!bmp.begin())                 
  {                               
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    while (1) {}                    
  }   
  
  // External Interrupt
  attachInterrupt(digitalPinToInterrupt(buttonPin), systemCheck, CHANGE); // System Status

  // Timer Interrupt
  cli();
  TCNT1  = 0;
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = 15624; 
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  sei();

   if(systemState == false)
  {   
    // Last Measurement Date and Time 
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);                                    
    myGLCD.print("Last", CENTER, 0);
    myGLCD.print("Measurement", CENTER, 8);
    
    // EEPROM reading
    myGLCD.print(dateValue.reading() ,CENTER, 24);
    myGLCD.print(timeValue.reading() ,CENTER, 32);
    delay(4000);

    showLastMeasurement();

    // Upload Date and Time
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Measuring...", CENTER, 8);
    myGLCD.print(String(__DATE__), CENTER, 24);
    myGLCD.print(String(__TIME__), CENTER, 32);
    delay(4000);
  }
}


void loop()
{
  if(systemState == false)
  {  
    if (counter == 0)
    {
      myGLCD.clrScr();
      myGLCD.setFont(SmallFont);                                    
      myGLCD.print("Last", CENTER, 8);
      myGLCD.print("Measurement", CENTER, 16);
      delay(4000);

      showLastMeasurement();
      
      myGLCD.clrScr();
      myGLCD.setFont(SmallFont);
      myGLCD.print("Measuring...", CENTER, 16);
      delay(4000);
      counter++;
    }
    
    dht11Value = DHT11.read(dht11Pin);
    
    // Humidity
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Humidity", CENTER, 0);
    myGLCD.print("%", 6, 27);
    
    myGLCD.setFont(BigNumbers);
    humidityValue = DHT11.humidity;
    Serial.print(" - Humidity: % ");
    Serial.println(humidityValue);
    myGLCD.printNumF(humidityValue, 1, CENTER, 18, '.');
    delay(4000);
    
    
    // Temperature
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Temperature", CENTER,0);
    
    myGLCD.setFont(BigNumbers);
    temperatureValue = DHT11.temperature;
    Serial.print(" - Temperature: ");
    Serial.print(temperatureValue);
    Serial.println(" Celcius");
    myGLCD.printNumF(temperatureValue, 1, CENTER, 18, '.');
    myGLCD.setFont(SmallFont);
    myGLCD.print("o", 72, 22);
    myGLCD.print("C", 75, 27);
    delay(4000);
    
    
    // BMP180 Altitude
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Altitude", CENTER,0);
    
    myGLCD.setFont(BigNumbers);
    altitudeValue = bmp.readAltitude();
    Serial.print(" - Altitude: ");
    Serial.print(altitudeValue);
    Serial.println(" meters");
    myGLCD.printNumF(altitudeValue, 1, CENTER, 18, '.');
    delay(4000);
    
    
    norma = getNormalPressure(altitudeValue, temperatureValue-5);

    // Pressure Calculation 
    Serial.print(" - Pressure Calculation Value: "); 
    Serial.print(norma);
    Serial.println(" milibar");
      
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Pressure", CENTER, 0);
    myGLCD.print("Calculation", CENTER, 8);
    myGLCD.setFont(BigNumbers); 
    myGLCD.printNumF(norma, 1, CENTER, 20, '.');
    delay(4000);
    
    
    // BMP180 Pressure
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Pressure", CENTER,0);
    
    myGLCD.setFont(BigNumbers);
    pressureValue = bmp.readPressure();
    Serial.print(" - Pressure Sensor: ");
    Serial.print(pressureValue/100);
    Serial.println(" milibar");
    myGLCD.printNumF(pressureValue/100, 1, CENTER, 18, '.');
    delay(4000);

    pressureStore();
    
    // Pressure Dynamic
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Pressure", CENTER,0);
    myGLCD.print("Dynamic", CENTER,8);
    Serial.print(" - Pressure Dynamic: ");
    if(forecast[4] == 1)
    {
      Serial.println(" Up"); // Up
      myGLCD.print("UP", CENTER, 24);
    }
    else if(forecast[0] < pressureValue)
    {
      Serial.println(" Down"); // Down
      myGLCD.print("DOWN", CENTER, 24);
    }
    else
    {
      Serial.println(" Equal"); // Equal
       myGLCD.print("EQUAL", CENTER, 24);
    }
    delay(4000);
       
    // Weather Condition 
    // Low Pressure 
    if((pressureValue/100 <= 1013) || (norma < 1013)) // sunny, cloudy, etc...
    {
      // Sunny
      if((forecast[0] > norma+7) && (forecast[1] < 2)&&(forecast[4] <= 1))
      {
        // Sunny Icon (Yellow) 
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Sunny", CENTER, 0);
        myGLCD.drawBitmap(20,8, sunny, 48, 48);
        conditionName = "Sunny";
        delay(4000);

        if(temperatureValue > 35)
        {
          // Red (Dangerous)
          analogWrite(redPin, 0);
          analogWrite(greenPin, 255);
          analogWrite(bluePin, 255);
        }
        else
        {
          // Yellow
          analogWrite(redPin, 0);
          analogWrite(greenPin, 0);
          analogWrite(bluePin, 255);
        }
      }
      // Rainy
      else if((forecast[0] < norma+15) && (forecast[1] > 2))
      {
        // Rainy Icon
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Rainy", CENTER, 0);
        myGLCD.drawBitmap(20,8, rainy, 48, 48);
        conditionName = "Rainy";
        delay(4000);

        // Blue
        analogWrite(redPin, 255);
        analogWrite(greenPin,255);
        analogWrite(bluePin, 0);
      }
      // Rainy
      else if((forecast[0] > (norma-30)) && (forecast[0] < (norma-3)) && (forecast[4] == 2))
      {
        // Rainy Icon
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Rainy", CENTER, 0);
        myGLCD.drawBitmap(20,8, rainy, 48, 48);
        conditionName = "Rainy";
        delay(4000);

        // Blue
        analogWrite(redPin, 255);
        analogWrite(greenPin,255);
        analogWrite(bluePin, 0);
      }
      else
      {
        if(temperatureValue > 20)
        {
          // Partly Cloudy Icon 
          myGLCD.clrScr(); 
          myGLCD.setFont(SmallFont);
          myGLCD.print("Partly Cloudy", CENTER, 0);
          myGLCD.drawBitmap(20,8, partly_cloudy, 48, 48);
          conditionName = "Partly Cloudy";
          delay(4000); 
        }
        
        // OR
        else
        {
          // Cloudy Icon
          myGLCD.clrScr();
          myGLCD.setFont(SmallFont);
          myGLCD.print("Cloudy", CENTER, 0);
          myGLCD.drawBitmap(20,8, cloudy, 48, 48);
          conditionName = "Cloudy";
          delay(4000);
        }
        // Green
        analogWrite(redPin, 255);
        analogWrite(greenPin, 0);
        analogWrite(bluePin, 255); 
       }
     }
     // High Pressure
     else if((pressureValue/100 > 1013) ||(norma > 1013)) // rainy, snowy, etc...
     {
      // Rainy
      if((forecast[0] < norma+15) && (forecast[1] > 2))
      {
        // Rainy Icon
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Rainy", CENTER, 0);
        myGLCD.drawBitmap(20,8, rainy, 48, 48);
        conditionName = "Rainy";
        delay(4000);
        
        // Blue
        analogWrite(redPin, 255);
        analogWrite(greenPin,255);
        analogWrite(bluePin, 0);
      }
      // Rainy
      else if((forecast[0] > (norma-30)) && (forecast[0] < (norma-3)) && (forecast[4] == 2))
      {
        // Rainy Icon
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Rainy", CENTER, 0);
        myGLCD.drawBitmap(20,8, rainy, 48, 48);
        conditionName = "Rainy";
        delay(4000);

        // Blue
        analogWrite(redPin, 255);
        analogWrite(greenPin,255);
        analogWrite(bluePin, 0);
      }
      // Snowy
      else if((temperatureValue < 0) && (humidityValue > 70) && (altitudeValue > 900)) 
      {
        // Snowy Icon
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Snowy", CENTER, 0);
        myGLCD.drawBitmap(20,8, snowy, 48, 48);
        conditionName = "Snowy";
        delay(4000);
        
        analogWrite(redPin, 0);
        analogWrite(greenPin, 0);
        analogWrite(bluePin, 0);
      }
    }

    if(conditionName == "Rainy")
    {
      waterValue = analogRead(waterSensorPin);
      if(waterValue <= 100)
      {
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Water Level", CENTER, 0);
        myGLCD.print("LOW", CENTER, 18);
        delay(4000);
      }
      else if(waterValue > 100 &&  waterValue <=330)
      {
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Water Level", CENTER, 0);
        myGLCD.print("MEDIUM", CENTER, 18);
        delay(4000);
      }
      else
      {
        myGLCD.clrScr();
        myGLCD.setFont(SmallFont);
        myGLCD.print("Water Level", CENTER, 0);
        myGLCD.print("HIGH", CENTER, 18);
        delay(4000);
        // Red (Dangerous)
        analogWrite(redPin, 0);
        analogWrite(greenPin, 255);
        analogWrite(bluePin, 255);
      }
    }

    Serial1.println("AT+CIPSTART=\"TCP\",\""+ip+"\",80");           //Thingspeak
    if(Serial1.find("Error"))                                   
      Serial.println("AT+CIPSTART Error");

    
    String data = "GET https://api.thingspeak.com/update?api_key=your_api_key"; 
    data += "&field3=";
    data += String(humidityValue);
    data += "&field4=";
    data += String(temperatureValue);
    data += "&field5=";
    data += String(altitudeValue);
    data += "&field6=";
    data += String(norma);
    data += "&field7=";
    data += String(pressureValue/100);
    data += "&field8=";
    data += conditionName;
    data += "\r\n\r\n";
    
    Serial1.print("AT+CIPSEND=");                                   
    Serial1.println(data.length()+2);
    delay(2000);
    if(Serial1.find(">"))
    {                                         
      Serial1.println(data);                  
      Serial.println(" - Data send successfully");
      delay(1000);
    }
    Serial1.println("AT+CIPCLOSE");     
  }
}

void showLastMeasurement()
{
    // Humidity
    Serial.print(" - Measured Humidity Value: % ");                    
    Serial.println(EEPROM.get(humidityAddress, datatypef)); 
    
    myGLCD.clrScr();                                                              
    myGLCD.setFont(SmallFont);                                                      
    myGLCD.print("Humidity", CENTER, 0); 
    myGLCD.print("%", 6, 27);                                         
    myGLCD.setFont(BigNumbers);                                                   
    myGLCD.printNumF(EEPROM.get(humidityAddress, datatypef), 1, CENTER, 18, '.'); 
    delay(4000); 
    
    // Temperature
    Serial.print(" - Measured Temperature Value: "); 
    Serial.print(EEPROM.get(temperatureAddress, datatypef));
    Serial.println(" Celcius");
  
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Temperature", CENTER, 0);
    myGLCD.setFont(BigNumbers);
    myGLCD.printNumF(EEPROM.get(temperatureAddress, datatypef), 1, CENTER, 18, '.');
    myGLCD.setFont(SmallFont);
    myGLCD.print("o", 72, 22);
    myGLCD.print("C", 75, 27);
    delay(4000);

    // Altitude
    Serial.print(" - Measured Altitude Value: "); 
    Serial.print(EEPROM.get(altitudeAddress, datatypef));
    Serial.println(" meters");
      
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Altitude", CENTER, 0);
    myGLCD.setFont(BigNumbers); 
    myGLCD.printNumF(EEPROM.get(altitudeAddress, datatypef), 1, CENTER, 18, '.');
    delay(4000);

    // Pressure Calculation 
    Serial.print(" - Pressure Calculation Value: "); 
    Serial.print(EEPROM.get(pressureCalculationAddress, datatypef));
    Serial.println(" milibar");
      
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Pressure", CENTER, 0);
    myGLCD.print("Calculation", CENTER, 8);
    myGLCD.setFont(BigNumbers); 
    myGLCD.printNumF(EEPROM.get(pressureCalculationAddress, datatypef), 1, CENTER, 20, '.');
    delay(4000);

    // Pressure
    Serial.print(" - Measured Pressure Value : "); 
    Serial.print(EEPROM.get(pressureAddress, datatypef));
    Serial.println(" milibar");
      
    myGLCD.clrScr();
    myGLCD.setFont(SmallFont);
    myGLCD.print("Pressure", CENTER, 0);
    myGLCD.setFont(BigNumbers); 
    myGLCD.printNumF(EEPROM.get(pressureAddress, datatypef), 1, CENTER, 18, '.');
    delay(4000);
}


// Pressure Calculation with altitude and temperature
float getNormalPressure(float alt, float temp)
{
  float L =  0.0065;
  float B =  L / (273.15+temp);
  return (1013.25*pow((1-B*alt),5.2556));
}


// Pressure Array
void pressureStore()
{
  // Current Pressure
  if(forecast[0] == 0)
  {
    for(int i=0; i<4; i++)
    {
      forecast[i] = pressureValue;
    }
    forecast[1] = 0; // Difference
  }
  else{
    if(forecast[0] > pressureValue)
      forecast[4] = 1; // Up
    else if(forecast[0] < pressureValue)
      forecast[4] = 2; // Down
    else
      forecast[4] = 0; // Equal
  }

  // Difference
  if(forecast[0] > forecast[2])
    forecast[2] = forecast[0];
  else if(forecast[0] < forecast[3])
    forecast[3] = forecast[0];
    
  forecast[1] = forecast[2] - forecast[3];
}


// Timer Interrupt
ISR(TIMER1_COMPA_vect)
{
  ldrValue = analogRead(ldrPin);
  if(systemState == false)
  {
    if(ldrValue <= 500)
      digitalWrite(backlightPin, HIGH);
    else
      digitalWrite(backlightPin, LOW);
  }
}


// System ON/OFF (External Interrupt)
void systemCheck()
{
  buttonState = digitalRead(buttonPin); 
  if(buttonState == HIGH)
  { 
    if(systemState == false) // OFF
    { 
      // EEPROM writing
      dateValue.writing(String(__DATE__));
      timeValue.writing(String(__TIME__));
      
      // EEPROM Memory Usage
      EEPROM.put(humidityAddress, humidityValue);
      EEPROM.put(temperatureAddress, temperatureValue);
      EEPROM.put(altitudeAddress, altitudeValue);
      EEPROM.put(pressureCalculationAddress, getNormalPressure(altitudeValue, temperatureValue-5));
      EEPROM.put(pressureAddress, pressureValue/100);
      
      systemState = true;
      myGLCD.enableSleep();
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
      digitalWrite(backlightPin, LOW);
      delayMicroseconds(200); // Wait 0.2 second
    }
    else if(systemState == true) // ON
    {
      counter = 0;
      systemState = false;
      myGLCD.disableSleep();    
      delayMicroseconds(200); // Wait 0.2 second
    }
    buttonState = LOW;
  }
}
