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

#define DATABASE_URL "https://timecube-d8fe0-default-rtdb.europe-west1.firebasedatabase.app/"
bool trackDone = false;
const uint8_t pinA = 4;
const uint8_t pinB = 5;
const uint8_t pinSW = 3;
const uint8_t pinGO = 2;
const uint8_t pinSTOP = 1;
RotaryEncoder encoder(pinA, pinB);
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASSWORD;
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

// Function prototypes
std::vector<String> GetTimeAccounts(String Type, String FieldName);


void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("arduino started");
  
  pinMode(pinSW, INPUT);
  pinMode(pinGO, INPUT);
  pinMode(pinSTOP, INPUT);
  u8g2.begin();
  u8g2.clearBuffer();                            // clear the internal memory
  u8g2.setFont(u8g2_font_helvB12_tf);            // choose a suitable font
  u8g2.drawStr(30, 30, "Welcome to Timecube!");  // write something to the internal memory
  u8g2.sendBuffer();                             // transfer internal memory to the display
  encoder.begin();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

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
}

void loop() {
  app.loop();
  Docs.loop();
  while (digitalRead(pinSW)&&digitalRead(pinGO)) {
    counter = (encoder.getPosition() / 2) % (sizeof(TimeTasksINI) / sizeof(TimeTasksINI[0]) + 1);
    drawWord(TimeTasksINI[counter].c_str());
  }
  delay(450);
  activity = TimeTasksINI[counter];
  if (activity == "Tracking") {
    while (digitalRead(pinSW)&&digitalRead(pinGO)) {
      counter = (encoder.getPosition() / 2) % (sizeof(TimeAccountsINI) / sizeof(TimeAccountsINI[0]) + 1);
      drawWord(TimeAccountsINI[counter].c_str());
    }
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
    while (digitalRead(pinSW)&&digitalRead(pinGO)) {
      drawWord("Waiting for Button....");
    }
    drawWord("Time started!");
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
      Serial.println(payload);
      int idStartIndex = payload.indexOf("\"name\":") + 8;
      int idEndIndex = payload.indexOf("\",", idStartIndex);
      String documentId = payload.substring(idStartIndex, idEndIndex);
      int lastSlashIndex = documentId.lastIndexOf('/');
      lastPart = documentId.substring(lastSlashIndex);
      //Serial.print("The last part of the string is: ");
      //Serial.println(lastPart);
      //Serial.print("Document created with ID: ");
      //Serial.println(documentId);
      //documentPath = documentId;
    } else {
      printError(client.lastError().code(), client.lastError().message());
    }

    Serial.println("Press button again to stop tracking!");
    while (digitalRead(pinSW)&&digitalRead(pinSTOP)) {
      drawWord("Waiting for Button....");
    }
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

void drawWord(const char* word1) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB12_tr); // Select a clear, bold font
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(word1)/2, 30, word1); // Centered text
  //u8g2.drawStr(0, 60, word1); // Left aligned   
  //u8g2.setFont(u8g2_font_helvB12_tf);  // choose a suitable font
  //u8g2.drawStr(10, 10, word1);         // write something to the internal memory
  u8g2.sendBuffer();                   // transfer internal memory to the display
  delay(500);
}
void doPomodoro(){
  int minutesp=5;
  encoder.setPosition(0);
  drawPomodoro(minutesp,"Minutes");
  while(digitalRead(pinSW)){
    delay(50);
    minutesp += encoder.getPosition();
    drawPomodoro(minutesp,"Minutes Pause");
    encoder.setPosition(0);
  }
  int minutesw = 25;
  while(digitalRead(pinSW)){
    delay(50);
    minutesw += encoder.getPosition();
    drawPomodoro(minutesw,"Minutes Work");
    encoder.setPosition(0);
  }
  
    String documentPath = "PomodoroHead";
    Values::TimestampValue accV2("1970-01-01T00:00:00Z");
    Values::IntegerValue accV3(minutesw);
    Values::IntegerValue accV3(minutesp);
    Document<Values::Value> doc;
    doc.add("TimeAccount", Values::Value(accV));
    doc.add("CreationTime",Values::Value(accV2));
    doc.add("Time",Values::Value(accV3));
    String payload = Docs.createDocument(client, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask("name"), doc);
}
void drawPomodoro(int minutes, const char* word){
  u8g2.clearBuffer();
  char str[8];
  itoa(minutes,str,10);
  u8g2.setFont(u8g2_font_helvB12_tr); // Select a clear, bold font
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(word)/2, 50, str); // Centered text
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(word)/2, 30, word); // Centered text
  //u8g2.drawStr(0, 60, word1); // Left aligned   
  //u8g2.setFont(u8g2_font_helvB12_tf);  // choose a suitable font
  //u8g2.drawStr(10, 10, word1);         // write something to the internal memory
  u8g2.sendBuffer();                   // transfer internal memory to the display
  delay(500);
}

