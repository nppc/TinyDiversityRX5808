//#define DEBUG


#include <EEPROM.h>

//#define MIN_TUNE_TIME 25 // update RSSI values every 25ms

// RSSI strength should be greater than the value below (percent) over the
// other receiver before we switch. This pervents flicker when RSSI values
// are close and delays diversity checks counter.
#define DIVERSITY_HYSTERESIS 2

// How long (ms) the RSSI strength has to have a greater difference than the
// above before switching.
#define DIVERSITY_HYSTERESIS_PERIOD 10


#define PIN_SWITCH_VIDEO 	0	//PB0
#define PIN_LED_A 			1	//PB1
#define PIN_LED_B 			3 	//PB3
#define PIN_RSSI_A 			2	//PB2
#define PIN_RSSI_B 			4	//PB4

// don't use first byte of EEPROM. It can be corrupted
#define EEPROM_RESET_FLAG_ADDR	5	// RESET Flag (byte)
#define EEPROM_MIN_RSSI_A_ADDR	7	// RSSI RAW Calibration value (int)
#define EEPROM_MAX_RSSI_A_ADDR	9	// RSSI RAW Calibration value (int)
#define EEPROM_MIN_RSSI_B_ADDR	11	// RSSI RAW Calibration value (int)
#define EEPROM_MAX_RSSI_B_ADDR	13	// RSSI RAW Calibration value (int)

uint8_t activeReceiver;
uint8_t diversityTargetReceiver;

typedef struct {
  uint16_t min;
  uint16_t max;
  uint16_t raw = 0;
  uint8_t mapped = 0;
} RSSI;

RSSI rssiA;
RSSI rssiB;

unsigned long diversityHysteresisTimer;


enum ReceiverSelect: byte {RX_A, RX_B};

// some macros
#define LED_A_ON bitClear(PORTB, PIN_LED_A) // LED A ON
#define LED_B_ON bitClear(PORTB, PIN_LED_B) // LED B ON
#define LED_A_OFF bitSet(PORTB, PIN_LED_A) // LED A OFF
#define LED_B_OFF bitSet(PORTB, PIN_LED_B) // LED B OFF


void setupPins() {

  bitSet(DDRB, PIN_SWITCH_VIDEO);
	bitSet(DDRB, PIN_LED_A);
	bitSet(DDRB, PIN_LED_B);
	LED_A_OFF;
	LED_B_OFF;
	bitSet(PORTB, PIN_LED_B);
	bitSet(PORTB, PIN_RSSI_A);
	bitSet(PORTB, PIN_RSSI_B);
	bitClear(DDRB,PIN_RSSI_A);
	bitClear(DDRB,PIN_RSSI_B);

/*
	pinMode(PIN_SWITCH_VIDEO, OUTPUT);
    pinMode(PIN_LED_A, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
	digitalWrite(PIN_LED_A, HIGH); // Off
	digitalWrite(PIN_LED_B, HIGH); // Off
    pinMode(PIN_RSSI_A, INPUT_PULLUP);
    pinMode(PIN_RSSI_B, INPUT_PULLUP);
*/
}

void setup()
{
	// First lets read RESET Flag; Power (bit0), Reset (bit 1), etc
	byte RESET_value = MCUSR;
	MCUSR = 0;	// reset flag for nect use
	
	setupPins();

	byte last_RESET_value = EEPROM.read(EEPROM_RESET_FLAG_ADDR);
	EEPROM.update(EEPROM_RESET_FLAG_ADDR, RESET_value);	// Only update if value is changed

	// should we enter calibration mode
	if(bitRead(last_RESET_value,1)!=1 && bitRead(RESET_value,1)==1){
		// enter Calibration Mode
		doCalibration();
		EEPROM.update(EEPROM_RESET_FLAG_ADDR, 1); //clear RESET flag to enter calibration mode after new RESET
	}
	// should we abort calibration mode
	if(bitRead(last_RESET_value,1)==1 && bitRead(RESET_value,1)==1){
		// abort Calibration Mode
		EEPROM.update(EEPROM_RESET_FLAG_ADDR, 1); //clear RESET flag to enter calibration mode after new RESET
	}

    setActiveReceiver(RX_A);
	diversityTargetReceiver = activeReceiver;	// Initialize variable for diversity switching
	
	readEEPROMSettings();

}

void setActiveReceiver(ReceiverSelect Set_RX){
	activeReceiver = Set_RX;
	if(activeReceiver == RX_A){
		bitClear(PORTB, PIN_SWITCH_VIDEO);
		LED_A_ON;
		LED_B_OFF;
	}else{
		bitSet(PORTB, PIN_SWITCH_VIDEO);
		LED_A_OFF;
		LED_B_ON;
	}
}

uint8_t remapRawRssiValue(RSSI rssi) {
  return constrain(
    map(
      rssi.raw,
      rssi.min,
      rssi.max,
      0,
      100
    ),
    0,
    100
  );
}

void updateRssi() {
	analogRead(PIN_RSSI_A); // Fake read to let ADC settle.
	rssiA.raw = analogRead(PIN_RSSI_A);
  rssiA.mapped = remapRawRssiValue(rssiA);
 
	analogRead(PIN_RSSI_B);
	rssiB.raw = analogRead(PIN_RSSI_B);
  rssiB.mapped = remapRawRssiValue(rssiA);
}
	
void switchDiversity() {

	int8_t rssiDiff = (int8_t) rssiA.mapped - (int8_t) rssiB.mapped;
	uint8_t rssiDiffAbs = abs(rssiDiff);
	uint8_t currentBestReceiver;
	uint8_t nextReceiver;
	
	nextReceiver = activeReceiver;

	if (rssiDiff > 0) {
		currentBestReceiver = RX_A;
	} else if (rssiDiff < 0) {
		currentBestReceiver = RX_B;
	} else {
		currentBestReceiver = activeReceiver;
	}

	if (rssiDiffAbs >= DIVERSITY_HYSTERESIS) {
		if (currentBestReceiver == diversityTargetReceiver) {
			if ((diversityHysteresisTimer+DIVERSITY_HYSTERESIS_PERIOD)<millis()) {
				nextReceiver = diversityTargetReceiver;
			}
		} else {
			diversityTargetReceiver = currentBestReceiver;
			diversityHysteresisTimer=millis();
		}
	} else {
		diversityHysteresisTimer=millis();
	}

	setActiveReceiver(nextReceiver);

}
	
void loop() {
    // Update RSSI values as fast as possible.
	updateRssi();
	switchDiversity(); // Will switch only if signals are significally different and difference stays for a while.
}

void readEEPROMSettings(){
	rssiA.min=readEEPROMint(EEPROM_MIN_RSSI_A_ADDR);
	rssiA.max=readEEPROMint(EEPROM_MAX_RSSI_A_ADDR);
	rssiB.min=readEEPROMint(EEPROM_MIN_RSSI_B_ADDR);
	rssiB.max=readEEPROMint(EEPROM_MAX_RSSI_B_ADDR);
}

int readEEPROMint(uint8_t addr){
	return EEPROM.read(addr)+EEPROM.read(addr+1)*256;
}

void writeEEPROMint(uint8_t addr, uint16_t value){
	EEPROM.update(addr, lowByte(value));
	EEPROM.update(addr+1, highByte(value));
}

void doCalibration(){

#if defined(DEBUG)
	// switch video source every second in DEBUG mode
	while(1==1){
		//delay(1000);
		mDelay(1000);
		setActiveReceiver(RX_A);
		//delay(1000);
		mDelay(1000);
		setActiveReceiver(RX_B);
	}

#else
	// Calibration routine
	// 1. Wait for lowest RSSI while some period (short blinks). Record min value.
	// 2. If RSSI changed drammatically, then wait a bit (2 seconds)
	// 3. Wait for highrst RSSI while some period (long blinks). Record max value.
	// 4. Store calibration data in EEPROM
	unsigned long tmr_tmp;
	uint16_t curMinRSSIA=1023; // max 10 bit ADC value is 1023
	uint16_t curMinRSSIB=1023; // max 10 bit ADC value is 1023
	uint16_t curMaxRSSIA=0;
	uint16_t curMaxRSSIB=0;
	// about 10 blinks for capturing low signal
	for(uint8_t i=0;i<10;i++){
		LED_A_ON;
		LED_B_ON;
		mDelay(100);
		LED_A_OFF;
		LED_B_OFF;
		tmr_tmp = millis();
		while((tmr_tmp+700)>millis()){
			updateRssi();	// refresh RSSI RAW values
			if(rssiA.raw<curMinRSSIA)curMinRSSIA = rssiA.raw;
			if(rssiB.raw<curMinRSSIB)curMinRSSIB = rssiB.raw;
		}
	}
	// Now wait until RSSI will jump for a good degree (100 ADC values) for both receivers
	while((rssiA.raw<curMinRSSIA+100) && (rssiB.raw<curMinRSSIB+100)){
		LED_A_ON;
		LED_B_ON;
		mDelay(100);
		LED_A_OFF;
		LED_B_OFF;
		mDelay(1500);
		updateRssi();
	}
	LED_A_ON;
	LED_B_ON;
	// Now just a delay for 2 seconds
	mDelay(2000);
	// about 10 blinks for capturing high signal
	for(uint8_t i=0;i<10;i++){
		LED_A_ON;
		LED_B_ON;
		tmr_tmp = millis();
		while((tmr_tmp+700)>millis()){
			updateRssi();	// refresh RSSI RAW values
			if(rssiA.raw>curMaxRSSIA)curMaxRSSIA = rssiA.raw;
			if(rssiB.raw>curMaxRSSIB)curMaxRSSIB = rssiB.raw;
		}
		LED_A_OFF;
		LED_B_OFF;
		mDelay(100);
	}
	// Now it is time to store values to EEPROM
	LED_A_ON;
	LED_B_ON;
	writeEEPROMint(EEPROM_MIN_RSSI_A_ADDR,curMinRSSIA);
	writeEEPROMint(EEPROM_MAX_RSSI_A_ADDR,curMaxRSSIA);
	writeEEPROMint(EEPROM_MIN_RSSI_B_ADDR,curMinRSSIB);
	writeEEPROMint(EEPROM_MAX_RSSI_B_ADDR,curMaxRSSIB);
	LED_A_OFF;
	LED_B_OFF;

#endif
}

void mDelay(int dtmp){
	unsigned long tmp = millis();
	while((tmp+dtmp)>millis()){}
}
