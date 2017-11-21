
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
uint8_t rssiA = 0;
uint16_t rssiARaw = 0;
uint8_t rssiB = 0;
uint16_t rssiBRaw = 0;

uint16_t rssiAMin;
uint16_t rssiAMax;
uint16_t rssiBMin;
uint16_t rssiBMax;

unsigned long diversityHysteresisTimer;


enum ReceiverSelelct: byte {RX_A, RX_B};

void setupPins() {

    bitSet(DDRB, PIN_SWITCH_VIDEO);
	bitSet(DDRB, PIN_LED_A);
	bitSet(DDRB, PIN_LED_B);
	bitSet(PORTB, PIN_LED_A);
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

void setActiveReceiver(ReceiverSelelct Set_RX){
	activeReceiver = Set_RX;
	if(activeReceiver == RX_A){bitClear(PORTB, PIN_SWITCH_VIDEO);}else{bitSet(PORTB, PIN_SWITCH_VIDEO);};
	//digitalWrite(PIN_SWITCH_VIDEO, (activeReceiver == RX_A ? LOW : HIGH));
	// change status of leds
	if(activeReceiver == RX_A){bitClear(PORTB, PIN_LED_A);}else{bitSet(PORTB, PIN_LED_A);};
	if(activeReceiver == RX_B){bitClear(PORTB, PIN_LED_B);}else{bitSet(PORTB, PIN_LED_B);};
	//digitalWrite(PIN_LED_A,(activeReceiver == RX_A ? LOW : HIGH));
	//digitalWrite(PIN_LED_B,(activeReceiver == RX_B ? LOW : HIGH));
}


uint16_t updateRssi() {
	analogRead(PIN_RSSI_A); // Fake read to let ADC settle.
	rssiARaw = analogRead(PIN_RSSI_A);
	analogRead(PIN_RSSI_B);
	rssiBRaw = analogRead(PIN_RSSI_B);

	rssiA = constrain(
		map(
			rssiARaw,
			rssiAMin,
			rssiAMax,
			0,
			100
		),
		0,
		100
	);
	rssiB = constrain(
		map(
			rssiBRaw,
			rssiBMin,
			rssiBMax,
			0,
			100
		),
		0,
		100
	);
}
	
void switchDiversity() {

	int8_t rssiDiff = (int8_t) rssiA - (int8_t) rssiB;
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
	rssiAMin=readEEPROMint(EEPROM_MIN_RSSI_A_ADDR);
	rssiAMax=readEEPROMint(EEPROM_MAX_RSSI_A_ADDR);
	rssiBMin=readEEPROMint(EEPROM_MIN_RSSI_B_ADDR);
	rssiBMax=readEEPROMint(EEPROM_MAX_RSSI_B_ADDR);
}

int readEEPROMint(byte addr){
	return EEPROM.read(addr)+EEPROM.read(addr+1)*256;
}

void doCalibration(){
	// at the moment lets just eneter DEBUG mode (switch video source every second)
	while(1==1){
		//delay(1000);
		mDelay(1000);
		setActiveReceiver(RX_A);
		//delay(1000);
		mDelay(1000);
		setActiveReceiver(RX_B);
	}
}

void mDelay(int dtmp){
unsigned long tmp = millis();
while(tmp+dtmp>millis()){}
}