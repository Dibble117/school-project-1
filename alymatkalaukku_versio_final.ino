
/* Älymatkalaukun koodi
 *  Hedi Cherif - IR-Remote
 *  Jussi Mäki - Acceleration sensor, Global Time, GSM
 *  Tiia Nuutinen - RFID
 *  Roosa Lampinen - GPS
 *  Niko Kolehmainen - Sound
 */


#include <TinyGPS.h>  //GPS

#include <SPI.h>  //RFID
#include <RFID.h>

#include <IRremote.h> //IR-remote käyttää muokattua kirjastoa, ota eri timeri käyttöön

#define SS_PIN 9  //Sound
#define RST_PIN 8


/* Timer - GLOBAL VARIABLES STARTS */
int sekunnit = 0;
int minuutit = 0;
int tunnit = 0;
long int globalTime = 0;
int paivitys = 0;
/* Timer - GLOBAL VARIABLES STOP */


/* GPS - GLOBAL VARIABLES STARTS */
//TX -> TX1 19, RX -> RX1 18
//#include <TinyGPS.h> 
//long   lat,lon; // muuttujat piireille
float lat, lon;
TinyGPS gps; // GPS-kutsu
/* GPS - GLOBAL VARIABLES STOP */

/* ACCELERATION SENSOR GLOBAL VARIABLES STARTS */
const int analogInPinX = A0; // X-kanavan kytkentä
const int analogInPinY = A1; // Y-kanavan kytkentä
const int analogInPinZ = A2; // Z-kanavan kytkentä

int alarmMaxTime = 5; //aika joka odotetaan hälytysten välillä jonka ajan tarkkaillaan jatkuuko liike. Periaatteessa 5 sekunttia, käytännössä enemmän.
long int alarmTime = 0;
int accelerationHalytys = 0; //Hälytys"boolean"
/* ACCELERATION SENSOR GLOBAL VARIABLES STOP */

/* RFID GLOBAL VARIABLES STARTS */

//#include <SPI.h>
//#include <RFID.h>

//#define SS_PIN 9
//#define RST_PIN 8

long int rfidReadTimeLimit = 0;

RFID rfid(SS_PIN,RST_PIN);

int serNum[5];
int cards[][5] = {{189,86,72,89,250}};  // hyväksytyn kortin ID
bool access = false;
/* RFID - GLOBAL VARIABLES STOP */

/* IR-Remote GLOBAL VARIABLES STARTS */
//#include <IRremote.h>

int RECV_PIN = 7;  //IR-receiverin pin
IRrecv irrecv(RECV_PIN); //asetetaan

decode_results results;
/* IR-Remote - GLOBAL VARIABLES STOP */

/* GSM GLOBAL VARIABLES STARTS */
int messagesent = 0;
int gsmTime = 0;
int gsmSet = 0;
String SMSData = "tyhja";
/* GSM - GLOBAL VARIABLES STOP */

int timeLimit = 5;  //Kuinka kauan hälytys soi
bool alarmSet = false;


void timerSetup(){
  cli(); //Pysäytetään interruptit
  TCCR1A = 0;  //Nollataan timerit
  TCCR1B = 0;
  TCCR1B |= B00000100; //Asetetaan prescaalaaja 256 or-tavalla
      
  TCNT1 = 0;  //nollataan laskuri
    
  TIMSK1 |= B00000010;  //otetaan compare match käyttöön
    
  OCR1A = 62500;  //asetetaan interrupt kohtaan 62500 16mhz kelloa, jolloin saadaan 256 prescaalaajalla 1000ms eli yhden sekunnin väli.
  sei(); //otetaan taas interruptit käyttöön
}

void Serialcom(){
  while(Serial2.available()){
    Serial.write(Serial2.read());
  }
}
  
void gsmSetup(){ 
  Serial2.begin(115200); //Ota yhteyttä GSM-moduuliin
  Serialcom();
  Serial2.println("AT");  //Tarkista yhteys
  delay(250);
  Serialcom();
  Serial2.println("AT+IPR=115200");  //aseta yhteyden nopeus gsm-moduulille
  delay(250);
  Serialcom();
  Serial2.println("AT+CPIN=0000");  //Syötä pin-koodi
  delay(250);
  Serialcom();
  Serial2.println("AT+CPIN?");  //Tarkista pin-koodin tila
  delay(250);
  Serialcom();    
}
    
void setup() {
  delay(3000); //annetaan moduulien käynnistyä varmasti
  Serial.begin(9600);
  Serial1.begin(9600); //Ota yhteyttä GPS-moduuliin
  
  gsmSetup();  //GSM, katso funktio
  
  SPI.begin();
  rfid.init();  //Käynnistä RFID-lukija
  
  irrecv.enableIRIn(); // Start the IR-receiver
  
  timerSetup(); //käynnistetään juokseva kello, katso funktio
}


void sendSMS(){
  Serial.println("sending SMS...");
  delay(250);
  Serialcom();
  Serial2.println("AT+CMGF=1\r");  //ota tekstimode käyttöön
  delay(1000);
  Serialcom();
  Serial2.println("AT+CMGS=\"+358456797986\"\r");  //lähetä SMS numeroon xxx
  delay(1000);
  Serialcom();
  Serial2.print(lat);  //lähetä GPS latitude ja longitude
  Serial2.print(" ");
  Serial2.println(lon);
  delay(1000);
  Serialcom();
  Serial2.println((char)26);  //syötä loppumerkki
  delay(250);
  Serialcom();
  Serial.println("SMS Sent");
  Serialcom();
}

void irremote(){
  if (irrecv.decode(&results)) {
    Serial.println(results.value);
        
    if(results.value == 16753245){
      //digitalWrite(led, HIGH);
      alarmSet = true;
      Serial.println("Hälytys otettu käyttöön IR-lukijalla. Kello: ");
      showTime();
    }else if(results.value == 16736925){
      //digitalWrite (led,LOW);
      Serial.println("Hälytys otettu pois käytöstä IR-lukijalla. Kello: ");
      showTime();
      alarmSet = false;
      accelerationHalytys = 0;
      gsmSet = 0;
      messagesent = 0;
    }
    irrecv.resume(); // Receive the next value
  }
}

void readCard(){
  if(rfid.isCard()){ //boolean: onko kortti lukijassa?
    if(rfid.readCardSerial()){ //luetaan kortin ID
      for(int x = 0; x < sizeof(cards); x++){
        for(int i = 0; i < sizeof(rfid.serNum); i++ ){ 
          if(rfid.serNum[i] != cards[x][i]) { //jos luettu kortti ei ole hyväksytty=> access = false
            access = false;
              if(rfidReadTimeLimit < globalTime){  //Mikäli korttia ei ole luettu vähään aikaan
                rfidReadTimeLimit = globalTime + timeLimit; //asetetaan uusi 5 sekunnin aikalimitti seuraavaan lukuun
                Serial.println("Pääsy kielletty!");
              }
              break;
            } else {
              if(rfidReadTimeLimit < globalTime){ //Mikäli korttia ei ole luettu vähään aikaan
                if(alarmSet == true){
                  alarmSet = false;
                  accelerationHalytys = 0;
                  Serial.print("Hälytys otettu pois käytöstä RFID-lukijalla. Kello: ");
                  showTime();
                  gsmSet = 0;
                  messagesent = 0;
                } else{
                  alarmSet = true;
                  Serial.print("Hälytys otettu käyttöön RFID-lukijalla. Kello: ");
                  showTime();
                }
                rfidReadTimeLimit = globalTime + timeLimit;
                access = true;
              }
            }  
          }
        if(access) break;
      }
    }
  }
}
 

void accelerationLoop(){  /*tarkastaa acceleration-sensorin tilanteen. Hälytys pysyy päällä 5 sekunttia sensorin rauhottumisen jälkeen */
  int sVX = analogRead(analogInPinX);   //Luetaan pinnit, X-kiihtyvyyttä vastaava sensoriarvo 10 bittisessä järjestelmässä (0 - 1023)
  int sVY = analogRead(analogInPinY);   // Y-kiihtyvyyttä vastaava sensoriarvo 10 bittisessä järjestelmässä (0 - 1023)
  int sVZ = analogRead(analogInPinZ);   // Z-kiihtyvyyttä vastaava sensoriarvo 10 bittisessä järjestelmässä (0 - 1023)
    
  float liikkeenRajaArvo = 15.0; //Liikkentunnistamisen raja-arvo 15 on reipas liike. 9.81 on perus painovoiman määrä, joten hyvä arvo on varmasti jossain tässä välillä.
  float KorjausKerroinX = 0.1431;  //nämä täytyy varmistaa/syöttää sensorin calibroinnista, nämä toimii omalla sensorillani
  float KorjausKerroinY = 0.1431;
  float KorjausKerroinZ = 0.1453;
  float KorjausLukemaX = 48.336;
  float KorjausLukemaY = 47.859;
  float KorjausLukemaZ = 50.02;
  
  double AccX = KorjausKerroinX * sVX - KorjausLukemaX; //korjataan kiihtyvyysarvot
  double AccY = KorjausKerroinY * sVY - KorjausLukemaY;
  double AccZ = KorjausKerroinZ * sVZ - KorjausLukemaZ;
  
  double acceleration = sqrt((AccX*AccX) + (AccY*AccY) + (AccZ*AccZ)); //Lasketaan 3-ulotteisen pytagoraan lauseen avulla kiihtyvyyden vektorin magnitudi
    
  if(acceleration > liikkeenRajaArvo){  //mikäli magnitudi ylittää liikkeen raja-arvon, hälytys kytketään päälle
    accelerationHalytys = 1;
    alarmTime = timeLimit + globalTime;  //Mikäli liike jatkuu, nollataan hälytysaika (nyt toimii niin että ottaa uptimestä + 5 sekunttia eteenpäin kohdan johon odotetaan).
    if(gsmSet == 0){
      gsmTime = globalTime + 10;
      gsmSet = 1;
    }
  }
    
  if(accelerationHalytys == 1){
    if(alarmTime<=globalTime){  //Jos aika pääsee saavuttamaan hälytyksen maksimipituuden, kytketään hälytys pois
      accelerationHalytys = 0;
    }
  }
  //Serial.println(alarmTime);
  //tulostaArvot(acceleration);  //Lähetetään laskettu acceleration-arvo tulosta-funktiolle. Testausta varten
  //return acceleration;
}

void tulostaArvot(double kiihtyvyysArvo){   //testausta varten on kiva saada serial Monitorille tietoa. Ei käytössä tällä hetkellä
  Serial.print(kiihtyvyysArvo);
  Serial.print(" : ");
  Serial.println(accelerationHalytys);
}

int thisNote = 0;
bool noteOn = false;

void aani(){
  #define NOTE_A1 2540                                    // Nuottitaajuudet
  #define NOTE_B2 2560
  #define NOTE_C3 2570
  
  int melodia[]={                                         // Nuotit taulukossa
    NOTE_A1, NOTE_B2, NOTE_C3 
  };
  int noteDurations[]={                                   // Nuottien pituudet taulukossa, 1 = pitkä, 5 = lyhyt, jne.
    4, 5, 1 
  };
  //for (int thisNote = 0; thisNote < 3; thisNote++) {
  if(noteOn == false){
    int noteDuration = 1000 / noteDurations[thisNote];    // Nuottien pituus ms laskettuna: 1000 / (nuottipituusnumero)
    tone(11, melodia[thisNote], noteDuration);            // Hälytyksen aloitus, Arduino Output 11
      
    thisNote++;
    if(thisNote > 2){
      thisNote = 0;
    }
  }else{
    noTone(11);   
  }
  //int pauseBetweenNotes = noteDuration * 1.30;          // Nuottien välinen tauko = Nuottien pituus * 1.30
  //delay(pauseBetweenNotes);                             // Nuottien välinen tauko
  //noTone(11);                                         // Hälytyksen lopetus, Arduino Output 11
  //}
}

void etsiGPS(){
  while(Serial1.available()){ // hae GPS-data
    if(gps.encode(Serial1.read()))// muunna GPS-data
    { 
      gps.f_get_position(&lat,&lon); // hae pituus- ja leveyspiiri
    }
  }
}

void tulostaGPS(){
  Serial.print("Sijainti: ");
            
  //Latitude
  Serial.print("Leveyspiiri: ");
  Serial.print(lat,6);
            
  Serial.print(",");
            
  //Longitude
  Serial.print("Pituuspiiri: ");
  Serial.println(lon,6); 
}

void updateTime(){
  if(sekunnit>59){ //mikäli sekunnit tulee 60 asti
    minuutit++; //lisätään minuutti
    sekunnit = 0; //nollataan sekunnit
  } 
  if(minuutit>59){ //sama minuuteille/tunneille
    tunnit++;
    minuutit = 0;
  } 
  if(tunnit>23){ //ja lopulta tuntien overflow
    tunnit = 0;
  } 
}
  
void showTime(){ //luodaan logiikka jolla tulostetaan muodossa hh:mm:ss
  if(tunnit<10){
    Serial.print("0");
  }
  Serial.print(tunnit);
  Serial.print(":");
  if(minuutit<10){
    Serial.print("0");
  }
  Serial.print(minuutit);
  Serial.print(":");
  if(sekunnit<10){
    Serial.print("0");
  }
  Serial.println(sekunnit);
}
   

void loop() {  //Perustilanne:
  readCard();   //RFID tarkistetaan
  rfid.halt();  //RFID suljetaan
  irremote();  //IR-tarkistetaan
  etsiGPS();  //tarkistetaan GPS

  if(alarmSet == true){ //Hälytys kytketty päälle:
    accelerationLoop(); //Tarkistetaan liikesensori
    
    if(accelerationHalytys == 1){ //Hälyttää:
      aani();  //toista ääntä
      gsmReady();  //Tarkista tarviiko lähettää SMS
      tulostaGPS();  //tulosta GPS-tietoja serialiin
    }
  }
    
  updateTime();  //päivitä aika
  Serialcom();  //tarkista GSM-moduulin tilanne
}

void gsmReady(){
  if(gsmTime<=globalTime && messagesent == 0){  //Mikäli hälytystä on jo toistettu pitkään ja SMS viestiä ei vielä ole lähetetty
    sendSMS();  //Lähetä SMS
    messagesent = 1; //Aseta viesti lähetetyksi
  }
}

ISR(TIMER1_COMPA_vect){  //interruptin timer1-funktio käynnistyy aina kun timer1 saavuttaa kohdan 62500 prescaalauksen kanssa.
  TCNT1 = 0;  // nollataan laskuri
  sekunnit++; //lisätään sekuntti kelloon
  globalTime++; //lisätään sekuntti globaaliin aikaan
}