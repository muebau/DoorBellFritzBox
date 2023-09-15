#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#ifdef ESP32
#include <SPIFFS.h>
#endif

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <tr064.h>
#include <ESPmDNS.h>

#define BUTTON_PIN  16
#define LED_PIN     2

//define your default values here, if there are different values in config.json, they are overwritten.
char fb_user[32] = "admin";
char fb_pass[32] = "admin";
char fb_ip[32] = "192.168.178.1";
int FRITZBOX_PORT = 49000;

#define WM_SSID "doorbell"
#define WM_PASS "do0R6311"

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback()
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println();

    //read configuration from FS json
    Serial.println("mounting FS...");

    if (SPIFFS.begin())
    {
        Serial.println("mounted file system");
        if (SPIFFS.exists("/config.json"))
        {
            //file exists, reading and loading
            Serial.println("reading config file");
            File configFile = SPIFFS.open("/config.json", "r");
            if (configFile)
            {
                Serial.println("opened config file");
                size_t size = configFile.size();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf(new char[size]);

                configFile.readBytes(buf.get(), size);

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
                DynamicJsonDocument json(1024);
                auto deserializeError = deserializeJson(json, buf.get());
                serializeJson(json, Serial);
                if (!deserializeError)
                {
#else
                DynamicJsonBuffer jsonBuffer;
                JsonObject& json = jsonBuffer.parseObject(buf.get());
                json.printTo(Serial);
                if (json.success())
                {
#endif
                    Serial.println("\nparsed json");
                    strcpy(fb_user, json["fb_user"]);
                    strcpy(fb_pass, json["fb_pass"]);
                    strcpy(fb_ip, json["fb_ip"]);
                }
                else
                {
                    Serial.println("failed to load json config");
                }
                configFile.close();
            }
            else
            {
                Serial.println("json config not found");
            }
        }
    }
    else
    {
        Serial.println("failed to mount FS");
        Serial.println("==================");
        Serial.println("====  FORMAT  ====");
        Serial.println("==================");
        //clean FS, for testing
        SPIFFS.format();
    }
    //end read

    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_fb_user("fbuser", "FritzBox Username", fb_user, 32);
    WiFiManagerParameter custom_fb_pass("fbpass", "FritzBox Password", fb_pass, 32);
    WiFiManagerParameter custom_fb_ip("fbip", "FritzBox IP", fb_ip, 32);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    //add all your parameters here
    wifiManager.addParameter(&custom_fb_user);
    wifiManager.addParameter(&custom_fb_pass);
    wifiManager.addParameter(&custom_fb_ip);

    Serial.println("SSID: " WM_SSID ", PASS: " WM_PASS);
    if (!wifiManager.autoConnect(WM_SSID, WM_PASS))
    {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        Serial.println("restart");
        ESP.restart();
        delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    MDNS.begin(WM_SSID);

    //read updated parameters
    strcpy(fb_user, custom_fb_user.getValue());
    strcpy(fb_pass, custom_fb_pass.getValue());
    strcpy(fb_ip, custom_fb_ip.getValue());
    Serial.println("The values in the file are: ");
    Serial.println("\tfb_user : " + String(fb_user));
    Serial.println("\tfb_pass : " + String(fb_pass));
    Serial.println("\tfb_ip : " + String(fb_ip));

    //save the custom parameters to FS
    if (shouldSaveConfig)
    {
        Serial.println("saving config");
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();
#endif
        json["fb_user"] = fb_user;
        json["fb_pass"] = fb_pass;
        json["fb_ip"] = fb_ip;

        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile)
        {
            Serial.println("failed to open config file for writing");
        }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
        serializeJson(json, Serial);
        serializeJson(json, configFile);
#else
        json.printTo(Serial);
        json.printTo(configFile);
#endif
        configFile.close();
        //end save
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());
    // Define button port as input
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    wifiManager.setConfigPortalTimeout(30);
    wifiManager.startConfigPortal(WM_SSID, WM_PASS);
}

void doCall()
{
    Serial.println("\n====\nInfo:\nhttps://www.heise.de/select/ct/2017/17/1502995489716437\r\nhttps://www.reichelt.de/magazin/projekte/smarte-tuerklingel/\nhttps://github.com/belzetrigger/esp-FritzKlingel\n====");
    Serial.println("try to start a call");
    TR064 tr064_connection(FRITZBOX_PORT, fb_ip, fb_user, fb_pass);
    tr064_connection.init();
    Serial.println("tr064 initialized");

    String tr064_service = "urn:dslforum-org:service:X_VoIP:1";

    // Die Telefonnummer **9 ist der Fritzbox-Rundruf.
    String call_params[][2] = { {"NewX_AVM-DE_PhoneNumber", "**9"} };
    tr064_connection.action(tr064_service, "X_AVM-DE_DialNumber", call_params, 1);
    Serial.println("started call waiting ...");

    // Warte vier Sekunden bis zum auflegen
    delay(1000);
    Serial.println("waiting ...");
    delay(1000);
    Serial.println("waiting ...");
    delay(1000);
    Serial.println("waiting ...");
    delay(1000);

    tr064_connection.action(tr064_service, "X_AVM-DE_DialHangup");
    Serial.println("ended call");
}

void loop()
{
    if (digitalRead(BUTTON_PIN) == LOW)
    {
        doCall();
    }
}