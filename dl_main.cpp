#include <Homie.h>

// http://www.internetoflego.com/rfid-scanner-wemos-rc522-mqtt/
// https://github.com/Jorgen-VikingGod/ESP8266-MFRC522

// RFID
#include "MFRC522.h"
#define RST_PIN 15 // RST-PIN for RC522 - RFID - SPI - Modul GPIO15
#define SS_PIN  2  // SDA-PIN for RC522 - RFID - SPI - Modul GPIO2
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance

// Global Timer
unsigned long previousMillis = 0;
int interval = 2000;


const int PIN_RELAY = 5;
const int PIN_LED   = 13;
const int PIN_BUTTON = 0;

unsigned long aliveTime = 0;
unsigned long buttonDownTime = 0;
byte lastButtonState = 1;
byte buttonPressHandled = 0;
int incr=0;
bool enableFlag = true; // enable RFID access
int  unlockTime = 3000; // time in ms

static char jsonData[80];
#define MAX_TOKEN_STRING	80

#define MAX_TOKENS  10
#define MAX_UID_LEN 8
const char* aRFIDNames[] =  { "RFID1", "RFID2", "RFID3", "RFID4", "RFID5",
                              "RFID6", "RFID7", "RFID8", "RFID9","RFID10"};
char* aRFIDTokens[MAX_TOKENS] = { "10000001", "10000002",  "10000003", "10000004",  "10000005",
                                  "10000006", "10000007",  "10000008", "10000009",  "10000010"};

HomieNode switchNode      ("switchDoor" , "switch"); // ID, type
HomieNode readNode        ("RFID" , "json"); // ID, type

void readPN532();

bool json_lookup_char ( const char* cSource,  const char* cToken, char cReply[]);
// ---------------------------------------------------------------------------------------------------
void publishRFID(String cUID, int iRFIDNo, String strAccess) { // send the current RFID data as JSOPN to OpenHAB

  String msg="{RFID:";
  msg += cUID;
  msg += ",IDNUM:";
  msg += String(iRFIDNo);
  msg += ",ACCESS:";
  msg += strAccess;
  msg += "}";
  Homie.getLogger() << "publishRFID: " << msg << endl;
  readNode.setProperty("read").send(msg);
  readNode.setProperty("reply").send(String("RFID send"));
} // end of function

// --------------------------------------------------------------------------------------- enableHandler
bool enableHandler(HomieRange range, String value) {// enable/disable doorlock, i.e. ignore RFID if disabled

  if ( value == "true" )
    enableFlag=true;
  else
    enableFlag=false;
  readNode.setProperty("reply").send(String("enable received"));
  Homie.getLogger() << "tokenlist: " << value << " (MQTT)" << endl;

  return true;
} // end of function


// -------------------------------------------------------------------------------------- unlocktimeHandler
bool unlocktimeHandler(HomieRange range, String value) { // define how many seconds the relay will be opened

  readNode.setProperty("reply").send(String("unlocktime received"));
  Homie.getLogger() << "tokenlist: " << value << " (MQTT)" << endl;

  return true;
} // end of function

// -------------------------------------------------------------------------------------- tokenlistHandler
bool tokenlistHandler(HomieRange range, String value) { // OpenHAB will provide all valid RFIDs
  // { "RFID1":"345439", "RFID2":"765449","RFID3":"9345643"}
  static char cValue		[MAX_TOKEN_STRING];
  static char cToken		[MAX_TOKEN_STRING];
  int iCnt=0;

  Homie.getLogger() << endl << "tokenlist: " << value <<endl;
  value.toCharArray(cValue,MAX_TOKEN_STRING);

  for(int i=0;i<MAX_TOKENS;i++){
    if(json_lookup_char(cValue,aRFIDNames[i], cToken)==true){
      strncpy(aRFIDTokens[i], cToken, MAX_UID_LEN);
      Homie.getLogger() << "token: " << i+1 << ": " << aRFIDNames[i] << " : "<< cToken << endl;
      iCnt++;
    }
    else
      Homie.getLogger() << "token: " << i+1 << ": " << aRFIDNames[i] << " skipped " << endl;
  }
  readNode.setProperty("reply").send(String(iCnt));

  return true;
} // end of function

// ---------------------------------------------------------------------------------------- readNodeSetup
void readNodeSetup(){ // attach topic to node
  readNode.advertise("reply"); // add topic garage/GarageESP1/RFID/reply
  readNode.advertise("enable").settable(enableHandler); // add topic garage/GarageESP1/RFID/enable
  readNode.advertise("unlocktime").settable(unlocktimeHandler); // add topic garage/GarageESP1/RFID/unlocktime
  readNode.advertise("tokenlist").settable(tokenlistHandler); // add topic garage/GarageESP1/RFID/tokenlistHandler
} // end of function

// ---------------------------------------------------------------------------------------- switchOnHandler
bool switchOnHandler(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;

  bool on = (value == "true");
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
  switchNode.setProperty("on").send(value);
  Homie.getLogger() << "Light is " << (on ? "on" : "off") << endl;

  return true;
}// end of function

void dumpTokens(){
  Homie.getLogger() << "dump Tokens > ";
  for(int i=0; i < MAX_TOKENS; i++) {
    Homie.getLogger() << aRFIDTokens[i] << ", ";
  }
  Homie.getLogger() << endl;
}// end of function

// validate card UID by sending to server via MQTT
void validate(String cardID){

  Homie.getLogger() << endl << "validate> " << cardID << " -> ";
  for(int i=0; i < MAX_TOKENS; i++) {
    Homie.getLogger() << aRFIDTokens[i] << ", ";
  	if( cardID == aRFIDTokens[i]) {
      Homie.getLogger() << " valid ID for " << aRFIDNames[i] << endl;
  		// Led_Blink(LED_STATUS,1,100);
  		// nfcOpenLatch();
      publishRFID(cardID,i+1,"true");
  		return;
  		}
  }
  Homie.getLogger() << " INVALID " << endl;
  //Led_Blink(LED_STATUS,4,50);
  publishRFID(cardID,0,"false");
}// end of function

// RFID: dump a byte array as hex values to Serial, then send to validation routine.
void dump_byte_array(byte *buffer, byte bufferSize) {
  String uid;

  for (byte i = 0; i < bufferSize; i++) {
    // Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    // Serial.print(buffer[i], HEX);
    uid = uid + String(buffer[i], HEX);
  }
  if(uid){
    validate(uid);
  }
}// end of function

// ---------------------------------------------------------------------------------------- loopHandler
void loopHandler() {

  byte buttonState = digitalRead(PIN_BUTTON);
  if ( buttonState != lastButtonState ) {
    if (buttonState == LOW) {
      buttonDownTime     = millis();
      buttonPressHandled = 0;
    }
    else {
      unsigned long dt = millis() - buttonDownTime;
      if ( dt >= 90 && dt <= 900 && buttonPressHandled == 0 ) {
        Serial << "loopHandler: button pushed" << endl;
        switchNode.setProperty("button").send("button pressed");
        if(incr==0) { // this for test only
            publishRFID("d07b501b",7,"true");
            incr=1;
        } else {
          publishRFID("007b50ff",8,"false");
          incr=0;
        }
        dumpTokens();
        buttonPressHandled = 1;
      }
    }
    lastButtonState = buttonState;
  }

  // -----------------------------------------------------  Look for new RFID cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    //Serial.print("scanning");
    delay(50);
    return;
  }

  // scan the cards. Put in a non-blocking delay to avoid duplicate readings
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    // do non-blocking thing here

    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial()) {
      Serial.print("found card...");
      delay(50);
      return;
    }

    // Process card
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    }


} // end of function

// ---------------------------------------------------------------------------------------- json_lookup_char
bool json_lookup_char ( const char* cSource,  const char* cToken, char cReply[]){

	char *ptr;

  /*
  Serial.print("0 json_lookup_char > ");
  Serial.println(cSource);
  Serial.print("0 json_lookup_char > ");
  Serial.println(cToken);
  */

	ptr = strstr (cSource, cToken); // set pointer to 1st char in cSource matching token
	if(!ptr){
		//Serial.print("1 json_lookup_char > ");
		//Serial.print(cToken);
		//Serial.println("< not found");
		return false;
	}

  // Serial.print("2 json_lookup_char > ");
  // Serial.println(ptr);

	ptr = strstr (ptr, ":"); // set pointer to :
	if(!ptr){
		Serial.println("3 json_lookup_char > : < not found");
    Serial.println(cToken);
		return false;
	}

	// move pointer past colon
	ptr++;

	// interleaving value string depth (first quotation mark hast to be first char of value)
	int quotMarkDepth = 0;
	// quotation mark of current depth (initialize with \0, because we'll set it later)
	char lastQuotMark = '\0';

	// discard all spaces incipient to the value and set the first quot mark if necessary
	for(;(*ptr == ' ' || *ptr == '"' || *ptr == '\'' || *ptr == '\t') && !quotMarkDepth; ptr++){
		if(*ptr == '"' || *ptr == '\''){
			lastQuotMark = *ptr;
			++quotMarkDepth;
		}
	} // for

	for (int i=0, x=0;i<MAX_TOKEN_STRING+1;i++, ptr++){
		switch(*ptr){
		// stop at end of String, or end of value/object
			case '\0':
			case '}':
			case ',':
				// value ended -> leave parsing
				i = MAX_TOKEN_STRING+1;
				break;

			// handle interleaving strings
			case '\'':
			case '"':
				// Was value not wrapped in quotation marks?
				if(lastQuotMark == '\0'){
					cReply[x ] = *ptr; // copy quotation mark to target
					x++;
				}
				else{
					// close last String layer?
					if(*ptr == lastQuotMark){
						// set lastQuotMark to the opposite
						lastQuotMark = lastQuotMark == '"' ? '\'' : '"';
						// decrease depth
						--quotMarkDepth;
						// was interleaving string?
						if(quotMarkDepth){
							cReply[x ] = *ptr; // copy quotation mark to target
							x++;
						}
						else{
							// value ended -> leave parsing
							i = MAX_TOKEN_STRING+1;
						}
					}
					else{
						// open new String Layer
						// set last quotation mark
						lastQuotMark = *ptr;
						// increase depth
						++quotMarkDepth;
					}
				}
				break;

				// copy src char to target
			default:
				cReply[x ] = *ptr; // copy single char to target
				x++;
		} // switch
		cReply[x] = '\0'; // terminate String
	} // for

	return true;
}


void setupHandler() {
  // this replaces the traditional "setup()" to ensure connecitvity and handle OTA
  // RFID and Console
  Serial.begin(115200);
  Serial << endl << endl;
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522

} // end of function

// ---------------------------------------------------------------------------------------- setup
void setup() {
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);

  Homie_setFirmware("my-garage", "1.0.0");

  switchNode.advertise("on").settable(switchOnHandler);
  switchNode.advertise("button");
  readNodeSetup();

  Homie.setSetupFunction(setupHandler);
  Homie.setLoopFunction(loopHandler);

  Homie.setup();
} // end of function

void loop() {
  Homie.loop();
} // end of function
