#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <HTTPClient.h>

const char* ssid = "Deke";
const char* password = "tgyo3978";
const char* FIRMWARE_VERSION = "1.0.4"; 
const char* versionFileUrl   = "https://raw.githubusercontent.com/Deke1604/Raw2/refs/heads/main/version.txt";
const char* firmwareURL      = "https://github.com/Deke1604/Raw2/raw/main/firmware.bin";

WebServer server(80);

const char* host = "esp32";

unsigned long previousMillis = 0;
const long interval = 500; // blink interval (1 sec)
bool ledState = LOW;
const int led = 2; 

// HTML pages
const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr><td colspan=2><center><font size=4><b>ESP32 Login Page</b></font></center><br></td></tr>"
  "<tr><td>Username:</td><td><input type='text' name='userid'></td></tr>"
  "<tr><td>Password:</td><td><input type='password' name='pwd'></td></tr>"
  "<tr><td><input type='submit' onclick='check(this.form)' value='Login'></td></tr>"
  "</table></form>"
  "<script>"
  "function check(form) {"
  "if(form.userid.value=='admin' && form.pwd.value=='admin') {"
  "window.open('/serverIndex');"
  "} else {"
  " alert('Error: Invalid credentials');"
  "}"
  "}"
  "</script>";

const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) { console.log('success!') },"
  "error: function (a, b, c) { }"
  "});"
  "});"
  "</script>";

// Connect to WiFi with timeout
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi connection failed, running offline...");
  }
}

// Perform OTA update check (GitHub-hosted firmware)
void performOTA() {
  HTTPClient http;
  http.setTimeout(1000); 

  Serial.println("Checking for firmware updates...");

  http.begin(versionFileUrl);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String serverVersion = http.getString();
    serverVersion.trim();
    Serial.printf("Current: %s | Server: %s\n", FIRMWARE_VERSION, serverVersion.c_str());

    if (serverVersion != FIRMWARE_VERSION) {
      Serial.println("New firmware available! Starting OTA...");
      http.end();

      http.begin(firmwareURL);
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      int resp = http.GET();
      if (resp == 200) {
        int contentLength = http.getSize();
        WiFiClient * stream = http.getStreamPtr();

        if (Update.begin(contentLength)) {
          size_t written = Update.writeStream(*stream);
          if (written == contentLength) {
            Serial.println("OTA written successfully. Rebooting...");
            if (Update.end(true)) {
              ESP.restart();
            } else {
              Serial.printf("Update.end() failed. Error: %s\n", Update.errorString());
            }
          } else {
            Serial.printf("Written only %d/%d bytes. OTA failed!\n", written, contentLength);
          }
        } else {
          Serial.println("Not enough space for OTA.");
        }
      } else {
        Serial.printf("Failed to fetch firmware. HTTP code: %d\n", resp);
      }
    } else {
      Serial.println("Already up-to-date.");
    }
  } else {
    Serial.printf("Failed to fetch version file. HTTP code: %d\n", httpCode);
  }
  http.end();
}

  // http.begin("https://github.com/Deke1604/Raw/raw/main/firmware.bin");
  

void setup(void) {
  pinMode(led, OUTPUT);
  Serial.begin(115200);

  connectToWiFi();
  /*
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
*/
  if (WiFi.status() == WL_CONNECTED) {
    performOTA();
  }

  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started");
  }

  // Web server endpoints
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send((Update.hasError()) ? 500 : 200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(led, ledState);
  }
}