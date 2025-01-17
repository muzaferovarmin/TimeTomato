#include <Arduino.h>
#include "RotaryEncoder.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HttpClient.h>
//#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FirebaseClient.h>
#include "arduino_secrets.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <ArduinoBLE.h>
#define DATABASE_URL "https://timecube-d8fe0-default-rtdb.europe-west1.firebasedatabase.app/"
u8g2_uint_t offset;			// current offset for the scrolling text
u8g2_uint_t width;
bool ssidSet = false;
bool pwSet = false;
String ssid = "";
String password = "";
int wificounter = 1;
BLEService wifiService("19B10010-E8F2-537E-4F6C-D104768A1214"); // create service
// Create BLE characteristics
BLEStringCharacteristic bleSSID("19B10011-E8F2-537E-4F6C-D104768A1214", BLERead | BLEWrite, 25);
BLEStringCharacteristic blepassword("19B10012-E8F2-537E-4F6C-D104768A1216", BLERead | BLEWrite, 25);
BLEBooleanCharacteristic bleconnect("19B10013-E8F2-537E-4F6C-D104768A1215", BLERead | BLEWrite);
bool trackDone = false;
const uint8_t pinA = 4;
const uint8_t pinB = 5;
const uint8_t pinSW = 6;
const uint8_t pinGO = 2;
const uint8_t pinSTOP = 1;
RotaryEncoder encoder(pinA, pinB);
//const char* ssid = SECRET_SSID;
//const char* password = SECRET_PASSWORD;
//const char* firestore_host = "https://firestore.googleapis.com/v1/projects/YOUR_PROJECT_ID/databases/(default)/documents";
//const char* api_key = API_KEY;
Firestore::Documents Docs;
int counter;
String activity;
WiFiSSLClient ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));
FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
NoAuth noAuth;
String TimeAccount;
String StartTime;
String EndTime;
unsigned long TrackedTimeInMinutes;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/12, /* data=*/11);  // ESP32 Thing, HW I2C with pin remapping
std::vector<String> TimeAccountsINI;
std::vector<String> TimeTasksINI;
volatile bool risingEdge = false;
volatile bool risingEdgeStop = false;
volatile bool risingEdgeGo = false;
// Function prototypes
std::vector<String> GetTimeAccounts(String Type, String FieldName);
// Bitmap data for the tomato (this will be generated from Image2cpp or similar tool)
// 'Screenshot 2024-12-19 224210', 40x40px
const unsigned char tomato_bitmap [] PROGMEM = {
	0x3E, 0x7F, 0x77, 0x7F, 0x77, 0x77, 0x3E, 0x00,  0x00, 0x3E, 0x77, 0x77, 0x7F, 0x7F, 0x3E, 0x00 
};




void setup() {
  
  Serial.begin(115200);
  while(!Serial);
  Serial.println("arduino started");
  //start BLE
  if (!BLE.begin()) {
    Serial.println("Starting Bluetooth® Low Energy module failed!");
    while (1);
  }
  BLE.setLocalName("TimeTomato WiFi");
  BLE.setAdvertisedService(wifiService);
  wifiService.addCharacteristic(bleSSID);
  wifiService.addCharacteristic(blepassword);
  wifiService.addCharacteristic(bleconnect);
  BLE.addService(wifiService);
  bleconnect.writeValue(0);
  BLE.advertise();
  Serial.println("Bluetooth® device active, waiting for connections...");
  // BLE DONE
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(pinGO, INPUT_PULLUP);
  pinMode(pinSTOP, INPUT_PULLUP);
  u8g2.begin();
  u8g2.clearBuffer();                            // clear the internal memory
  u8g2.setFont(u8g2_font_tiny5_t_all);            // choose a suitable font
  u8g2.drawStr(30, 30, "Welcome to Timecube!");  // write something to the internal memory
  u8g2.sendBuffer();
                           // transfer internal memory to the display
  encoder.begin();

  //Wifi Provisioning
  WiFi.begin(ssid.c_str(),password.c_str());
  attemptWiFiConnection();

  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  initializeApp(client, app, getAuth(noAuth));
  app.getApp<Firestore::Documents>(Docs);
  //Database.url(DATABASE_URL);
  client.setAsyncResult(result);
  TimeAccountsINI = GetTimeAccounts("TimeAccount", "Name");
  TimeTasksINI = GetTimeAccounts("Tasks", "TaskName");
  counter = 0;
  Serial.println("alles bis zum interrupt done");
  //delay(5000);
  Serial.println("Hab sogar gewartet");
  attachInterrupt(digitalPinToInterrupt(pinSW), handleRisingEdge, RISING);
  attachInterrupt(digitalPinToInterrupt(pinSTOP), handleRisingEdgeGo, RISING);
  attachInterrupt(digitalPinToInterrupt(pinGO), handleRisingEdgeStop, RISING);
  Serial.println("after interrupt");
}

void loop() {
  app.loop();
  Docs.loop();
  risingEdge=false;
  offset=0;
  while (!risingEdge) {
    risingEdge=false;
    counter = (encoder.getPosition() / 2) % (sizeof(TimeTasksINI) / sizeof(TimeTasksINI[0]) + 1);
    drawWord(TimeTasksINI[counter].c_str(),"Select your activity");
    offset-=1;
    if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset = 0;}	
    delay(10);
  }
  risingEdge=false;
  risingEdgeGo=false;
  //delay(450);
  activity = TimeTasksINI[counter];
  if (activity == "Tracking") {
    while (!risingEdge&&digitalRead(pinGO)) {
      risingEdge = false;
      counter = (encoder.getPosition() / 2) % (sizeof(TimeAccountsINI) / sizeof(TimeAccountsINI[0]) + 1);
      offset=0;
      drawWord(TimeAccountsINI[counter].c_str(),"Select your TimeAccount");
      offset-=1;
      if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset = 0;}	
    }
    risingEdge=false;
    doTracking(TimeAccountsINI[counter]);
  }
  if(activity == "Pomodoro"){
      doPomodoro();
  }
}

std::vector<String> GetTimeAccounts(String Type, String FieldName) {
  String collectionPath1 = Type;
  std::vector<String> documentNames;

  String payloadt = Docs.get(client, Firestore::Parent(FIREBASE_PROJECT_ID), collectionPath1, GetDocumentOptions(DocumentMask(FieldName)));

  if (client.lastError().code() == 0) {
    // Parse the payload to extract document names
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payloadt);

    if (!error) {
      JsonArray documents = doc["documents"].as<JsonArray>();
      for (JsonObject document : documents) {
        String name = document["fields"][FieldName]["stringValue"];
        documentNames.push_back(name);
      }
    } else {
      Serial.print("Error parsing JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    printError(client.lastError().code(), client.lastError().message());
  }

  return documentNames;  // Return the array of document names
}

void doTracking(String Account) {
    TimeAccount = Account;
    Serial.println("Ok, you want to track " + TimeAccount);
    Serial.println("Press the button to start tracking!");
    risingEdge=false;
    offset=0;
    while (!risingEdge&&digitalRead(pinGO)) {
      risingEdge = false;
      drawWord("Waiting for Button....","Waiting to start tracking!");
      offset-=1;
      if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset=0;}
    }
    risingEdge=false;
    //drawWord("Time started!");
    //offset-=1;
    //if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset=0;}
    unsigned long startTime = millis();
    Values::StringValue accV(TimeAccount);
    String documentPath = "TimeTrackings";
    Values::TimestampValue accV2("1970-01-01T00:00:00Z");
    Values::IntegerValue accV3(0);
    Document<Values::Value> doc;
    doc.add("TimeAccount", Values::Value(accV));
    doc.add("CreationTime",Values::Value(accV2));
    doc.add("Time",Values::Value(accV3));
    String payload = Docs.createDocument(client, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask("name"), doc);
    String lastPart;
    if (client.lastError().code() == 0) {
      lastPart = extractId(payload);
      //int idStartIndex = payload.indexOf("\"name\":") + 8;
      //int idEndIndex = payload.indexOf("\",", idStartIndex);
      //String documentId = payload.substring(idStartIndex, idEndIndex);
      //int lastSlashIndex = documentId.lastIndexOf('/');
      //lastPart = documentId.substring(lastSlashIndex);
    } else {
      printError(client.lastError().code(), client.lastError().message());
    }

    Serial.println("Press button again to stop tracking!");
    risingEdge=false;
    while (!risingEdge&&digitalRead(pinSTOP)) {
      risingEdge = false;
      drawWord("Tracking time..","",true);
      offset-=1;
      if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset=0;}
    }
    risingEdge=false;
    documentPath= documentPath+lastPart;
    unsigned long endtime = millis();
    int trackedTime = int(round(float((endtime - startTime) / 60000)));
    String fieldPath = "CreationTime";
    FieldTransform::SetToServerValue setValue(FieldTransform::REQUEST_TIME); // set timestamp to "test_collection/test_document/server_time"
    FieldTransform::FieldTransform fieldTransforms(fieldPath, setValue);
    DocumentTransform transform(documentPath, fieldTransforms);
    Writes writes(Write(transform, Precondition()));
    Docs.commit(client, Firestore::Parent(FIREBASE_PROJECT_ID), writes);
    Values::IntegerValue time2(trackedTime);
    Document<Values::Value> doc2("Time", Values::Value(time2));
    PatchDocumentOptions patchOptions(DocumentMask("Time") /* updateMask */, DocumentMask() /* mask */, Precondition() /* precondition */);
    Docs.patch(client, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, patchOptions, doc2, result);
    //writes.add(Write(DocumentMask(),updateDoc,Precondition()));
    printError(client.lastError().code(), client.lastError().message());
    Serial.println(writes);
    Serial.println("Done tracking!");
    delay(1000);
  }


void waitForInput() {
  while (Serial.available() == 0) {
    // Wait for input
  }
  return;
}

void printResult(AsyncResult& aResult) {
  if (aResult.isEvent()) {
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
  }

  if (aResult.isDebug()) {
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  }

  if (aResult.isError()) {
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  }

  if (aResult.available()) {
    Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
  }
}
void printError(int code, const String& msg) {
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}

void drawWord(const char* text,const char* text2) {
  u8g2.setFont(u8g2_font_inb19_mr);	// set the target font to calculate the pixel width
  width = u8g2.getUTF8Width(text);		// calculate the pixel width of the text
  u8g2_uint_t x;
  u8g2.firstPage();
  do {
    x = offset;
    u8g2.setFont(u8g2_font_inb19_mr);		// set the target font
    do {								// repeated drawing of the scrolling text...
      u8g2.drawUTF8(x, 30, text);			// draw the scolling text
      x += width;						// add the pixel width of the scrolling text
    } while( x < u8g2.getDisplayWidth() );		// draw again until the complete display is filled
    
    u8g2.setFont(u8g2_font_fur11_tr);		// draw the current pixel width
    u8g2.setCursor(2, 58);
    u8g2.print(text2);					// this value must be lesser than 128 unless U8G2_16BIT is set
    
  } while ( u8g2.nextPage() );

}
void drawWord(const char* text,const char* text2, bool override) {
  u8g2.setFont(u8g2_font_inb19_mr);	// set the target font to calculate the pixel width
  width = u8g2.getUTF8Width(text);		// calculate the pixel width of the text
  u8g2_uint_t x;
  u8g2.firstPage();
  do {
    x = offset;
    u8g2.setFont(u8g2_font_inb19_mr);		// set the target font
    do {								// repeated drawing of the scrolling text...
      u8g2.drawUTF8(x, 30, text);			// draw the scolling text
      x += width;						// add the pixel width of the scrolling text
    } while( x < u8g2.getDisplayWidth() );		// draw again until the complete display is filled
    
    u8g2.setFont(u8g2_font_open_iconic_app_4x_t);		// draw the current pixel width
    u8g2.setCursor(2, 58);
    u8g2.print(64);					// this value must be lesser than 128 unless U8G2_16BIT is set
    
  } while ( u8g2.nextPage() );

}
void doPomodoro(){
  int minutesp=5;
  encoder.setPosition(5);
  risingEdge=false;
  while(!risingEdge){
    risingEdge = false;
    minutesp = encoder.getPosition();
    drawPomodoro(minutesp,"Minutes Pause");

  }risingEdge = false;
  delay(100);
  int minutesw = 25;
  encoder.setPosition(25);
  while(!risingEdge){
    risingEdge = false;
    minutesw = encoder.getPosition();
    drawPomodoro(minutesw,"Minutes Work");
  }risingEdge = false;
  
    String documentPath = "PomodoroHead";
    Values::TimestampValue tsP("1970-01-01T00:00:00Z");
    Values::IntegerValue msw(minutesw);
    Values::IntegerValue msp(minutesp);
    Values::IntegerValue cycle(0);
    Document<Values::Value> doc;
    doc.add("StartTime", Values::Value(tsP));
    doc.add("MinutesPause",Values::Value(msp));
    doc.add("MinutesActive",Values::Value(msw));
    doc.add("CompletedCycles",Values::Value(cycle));


    String payload = Docs.createDocument(client, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask("name"), doc);
    String lastpart2 = extractId(payload);
    documentPath= documentPath+lastpart2;
    String fieldPath = "StartTime";
    FieldTransform::SetToServerValue setValue(FieldTransform::REQUEST_TIME); // set timestamp to "test_collection/test_document/server_time"
    FieldTransform::FieldTransform fieldTransforms(fieldPath, setValue);
    DocumentTransform transform(documentPath, fieldTransforms);
    Writes writes(Write(transform, Precondition()));
    Docs.commit(client, Firestore::Parent(FIREBASE_PROJECT_ID), writes);
    drawPomodoro("Active!","Exit[Button]");
    long activeMillis=60000*minutesw;
    long pauseMillis=60000*minutesp;
    bool isActive = true;
    long cyclepom = 1;
    long startmillispom=millis();
    Serial.println(risingEdge);
    risingEdge=false;
    while(!risingEdge&&digitalRead(pinSTOP)){
      Serial.println(risingEdge);
      risingEdge = false;
      while(millis()<(activeMillis*cyclepom+(cyclepom-1)*pauseMillis)){
      drawPomodoro("Active!","Exit[Button]");
      //delay(500);
      //drawWord("Working!");
      //delay(500);
      if(risingEdge){break;}  
      }
      Serial.println(risingEdge);
      while(millis()<cyclepom*(activeMillis+pauseMillis)){
        Serial.println(risingEdge);
      drawPomodoro("Pause!","Exit[Button]");
      //delay(500);
      //drawWord("Do a pause!"); 
      //delay(500);
      if(risingEdge){break;}    
      }
      cyclepom+=1;}
    Values::IntegerValue cycle2(cyclepom);
    Document<Values::Value> doc2("CompletedCycles", Values::Value(cycle2));
    PatchDocumentOptions patchOptions(DocumentMask("CompletedCycles") /* updateMask */, DocumentMask() /* mask */, Precondition() /* precondition */);
    Docs.patch(client, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, patchOptions, doc2, result);
    drawPomodoro(cyclepom,"Done cycles:");
    delay(1000);
}
void drawPomodoro(int minutes, const char* word){
  u8g2.clearBuffer();
  char str[8];
  itoa(minutes,str,10);
  u8g2.drawXBMP(112, 0, 16, 16, tomato_bitmap);  // Adjust coordinates and size based on your image
  u8g2.setFont(u8g2_font_helvB12_tr); // Select a clear, bold font
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(word)/2, 50, str); // Centered text
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(word)/2, 30, word); // Centered text
  u8g2.sendBuffer();                   // transfer internal memory to the display
  delay(500);
}
void drawPomodoro(const char* word1, const char* word2){
  
  u8g2.clearBuffer();
  u8g2.drawXBMP(112, 0, 16, 16, tomato_bitmap);  // Adjust coordinates and size based on your image
  u8g2.setFont(u8g2_font_helvB12_tr); // Select a clear, bold font
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(word1)/2, 50, word1); // Centered text
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(word2)/2, 30, word2); // Centered text
  delay(500);
}
String extractId(String payload){
      String lastPart1;
      int idStartIndex = payload.indexOf("\"name\":") + 8;
      int idEndIndex = payload.indexOf("\",", idStartIndex);
      String documentId = payload.substring(idStartIndex, idEndIndex);
      int lastSlashIndex = documentId.lastIndexOf('/');
      lastPart1 = documentId.substring(lastSlashIndex);
      return lastPart1;
}
void handleRisingEdge() {
        risingEdge = true;
}
void handleRisingEdgeGo() {
        risingEdgeGo = true;
}
void handleRisingEdgeStop() {
        risingEdgeStop = true;
}
void attemptWiFiConnection() {
  Serial.println("Trying to connect with: " + ssid + " " + password);
  offset=0;
  while (WiFi.status() != WL_CONNECTED) {
    
    Serial.println("Retrying...");
    wificounter++;
    delay(10);
    if (wificounter == 5) {

      while (!(ssidSet & pwSet)) {
        drawWord("Enter WiFi Details via BLE!","Initializing...");
        offset-=1;
        if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset=0;}
        BLE.poll();
        if (bleSSID.written()) {
          ssid = bleSSID.value(); // Directly assign the value to String
          ssidSet = true;
          offset=0;
          for(int i =0; i<200;i++){
            drawWord(("New SSID " + ssid).c_str(),"Good job!");
            offset -=1;
            if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset=0;}
            delay(10);
          }
          Serial.println("New SSID: " + ssid);
        }
        if (blepassword.written()) {
          password = blepassword.value(); // Directly assign the value to String
          pwSet = true;
          for(int i =0; i<200;i++){
            drawWord("New Password: *********", "Good job!");
            offset -=1;
            if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset=0;}
            delay(10);
          }
          //Serial.println("New PW: " + password);
        }
      }
      wificounter = 0;
      ssidSet = false;
      pwSet = false;
      WiFi.begin(ssid.c_str(), password.c_str()); // Use c_str() here as WiFi.begin expects const char*
      if (WiFi.status() == WL_CONNECTED) {
        break;
      }
    }
  }
  //Serial.println("Connected to WiFi","Let's get Tomating!");

  for(int i =0; i<200;i++){
            drawWord(("Connected to: " + ssid).c_str(),"NICE!");
            offset -=1;
            if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width ){offset=0;}
            delay(10);
          }
  Serial.println(WiFi.localIP());

}
