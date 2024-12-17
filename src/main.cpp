#include <TFT_eSPI.h>
#include "../.pio/libdeps/esp-wrover-kit/lvgl/examples/lv_examples.h"
#include "../.pio/libdeps/esp-wrover-kit/lv_conf.h"
#include <lvgl.h>

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include "html/web_content.h"


const char* ssid = "LUMI_TEST";
const char* password = "lumivn274!";

// #define USE_WEB_SERVER
#define USE_MQTT
#define TAG "QR_CODE"
/*Set to your screen resolution and rotation*/
#define TFT_HOR_RES   320
#define TFT_VER_RES   480
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))

struct QRCode {
    int id;
    String name;
    String account;
    String bank;
    String qrText;
};

// uint32_t draw_buf[DRAW_BUF_SIZE / 4];
static uint32_t *draw_buf = NULL;
lv_display_t * disp = NULL;
Preferences preferences;

#ifdef USE_WEB_SERVER
AsyncWebServer server(80);
bool ledState = false;
#endif

#ifdef USE_MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char* mqtt_server = "10.10.30.194";  // Hoặc broker của bạn
const int mqtt_port = 1883;
const char* mqtt_topic_config = "lumi/qr/config";
const char* mqtt_topic_status = "lumi/qr/status";
#endif

std::vector<QRCode> qrList;
int activeQRId = -1;
// Thêm biến để track ID
static int nextQRId = 1;  // Thêm ở phần khai báo global

void saveQRListToFlash() {
    preferences.begin("qr-storage", false);
    
    // Lưu số lượng QR
    preferences.putInt("qr_count", qrList.size());
    
    // Lưu từng QR
    for (size_t i = 0; i < qrList.size(); i++) {
        String prefix = "qr_" + String(i) + "_";
        preferences.putInt((prefix + "id").c_str(), qrList[i].id);
        preferences.putString((prefix + "name").c_str(), qrList[i].name);
        preferences.putString((prefix + "account").c_str(), qrList[i].account);
        preferences.putString((prefix + "bank").c_str(), qrList[i].bank);
        preferences.putString((prefix + "text").c_str(), qrList[i].qrText);
    }
    
    // Lưu active QR ID
    preferences.putInt("active_qr", activeQRId);
    preferences.end();
}

void loadQRListFromFlash() {
    preferences.begin("qr-storage", true);
    
    // Đọc số lượng QR
    int qrCount = preferences.getInt("qr_count", 0);
    
    // Đọc từng QR
    qrList.clear();
    for (int i = 0; i < qrCount; i++) {
        String prefix = "qr_" + String(i) + "_";
        QRCode qr;
        qr.id = preferences.getInt((prefix + "id").c_str(), 0);
        qr.name = preferences.getString((prefix + "name").c_str(), "");
        qr.account = preferences.getString((prefix + "account").c_str(), "");
        qr.bank = preferences.getString((prefix + "bank").c_str(), "");
        qr.qrText = preferences.getString((prefix + "text").c_str(), "");
        qrList.push_back(qr);
    }
    
    // Đọc active QR ID
    activeQRId = preferences.getInt("active_qr", -1);
    
    // Cập nhật nextQRId
    nextQRId = 1;
    for(const auto& qr : qrList) {
        if(qr.id >= nextQRId) {
            nextQRId = qr.id + 1;
        }
    }
    
    Serial.print("Next QR ID will be: ");
    Serial.println(nextQRId);

    preferences.end();
}

#ifdef LV_USE_QRCODE
static lv_obj_t * new_screen = NULL;
static lv_obj_t * qr = NULL;
static lv_obj_t * bank_label = NULL;
static lv_obj_t * name_label = NULL;
static lv_obj_t * account_label = NULL;

void lv_print_qrcode(const char* qr_text, const char* bank_name, const char* name, const char* account)
{
    Serial.println("----- Print_qrcode: " + String(name));
    
    // Tạo màn hình mới
    if(new_screen == NULL) {
        new_screen = lv_obj_create(NULL);
        // lv_obj_set_style_bg_color(new_screen, lv_color_hex(0x003a57), LV_PART_MAIN);
        lv_obj_set_style_bg_color(new_screen, lv_color_white(), LV_PART_MAIN);

        lv_obj_set_style_bg_opa(new_screen, LV_OPA_COVER, LV_PART_MAIN);
    }else {
        Serial.println("----- Clean screen: ");
        lv_obj_clean(lv_scr_act());
        Serial.println("----- Clean screen Done ");
    }

    // Load màn hình mới
    lv_scr_load(new_screen);
    

    lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
    lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

    // Tạo QR code trên màn hình mới
    if(qr != NULL) {
        lv_obj_delete(qr);
        Serial.println(" delete qr: ");

    }
    qr = lv_qrcode_create(lv_scr_act());
    lv_qrcode_set_size(qr, 250);
    lv_qrcode_set_dark_color(qr, fg_color);
    lv_qrcode_set_light_color(qr, bg_color);

    /*Set data*/
    lv_qrcode_update(qr, qr_text, strlen(qr_text));
    lv_obj_center(qr);

    /*Add a border with bg_color*/
    lv_obj_set_style_border_color(qr, bg_color, 0);
    lv_obj_set_style_border_width(qr, 5, 0);

    // Tạo các label trên màn hình mới
    if(bank_label != NULL) {
        lv_obj_delete(bank_label);
        Serial.println(" delete bank_label: ");
    }
    bank_label = lv_label_create(lv_scr_act());
    lv_label_set_text(bank_label, bank_name);
    lv_obj_set_style_text_font(bank_label, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(bank_label, lv_color_black(), 0);  
    lv_obj_align_to(bank_label, qr, LV_ALIGN_OUT_TOP_MID, 0, -10);

    if(name_label != NULL) {
        lv_obj_delete(name_label);
        Serial.println(" delete name_label: ");
    }
    name_label = lv_label_create(lv_scr_act());
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(name_label, lv_color_black(), 0);
    lv_obj_align_to(name_label, qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    if(account_label != NULL) {
        lv_obj_delete(account_label);
        Serial.println(" delete account_label: ");
    }
    account_label = lv_label_create(lv_scr_act());
    lv_label_set_text(account_label, account);
    lv_obj_set_style_text_font(account_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(account_label, lv_color_black(), 0);  
    lv_obj_align_to(account_label, name_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
}
#endif

#ifdef USE_WEB_SERVER
String processor(const String& var) {
    if(var == "LED_STATE") {
        return ledState ? "ON" : "OFF";
    }
    if(var == "FREE_DRAM") {
        return String(ESP.getFreeHeap());
    }
    if(var == "FREE_PSRAM") {
        return String(ESP.getFreePsram());
    }
    return String();
}
#endif

#ifdef USE_MQTT
void publishQRList() {
    DynamicJsonDocument doc(8192);
    doc["activeQRId"] = activeQRId;
    
    JsonArray array = doc.createNestedArray("qrList");
    for(const auto& qr : qrList) {
        JsonObject obj = array.createNestedObject();
        obj["id"] = qr.id;
        obj["name"] = qr.name;
        obj["account"] = qr.account;
        obj["bank"] = qr.bank;
        obj["qrText"] = qr.qrText;
    }
    
    String response;
    serializeJson(doc, response);
    Serial.println("publishQRList: " + response);
    mqttClient.publish(mqtt_topic_status, response.c_str());
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    if (String(topic) == mqtt_topic_config) {
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, payload, length);
        
        String command = doc["command"].as<String>();
        
        if (command == "activate") {
            int id = doc["id"];
            auto it = std::find_if(qrList.begin(), qrList.end(),
                                  [id](const QRCode& qr) { return qr.id == id; });
            
            if (it != qrList.end()) {
                activeQRId = id;
                saveQRListToFlash();
                
                // Phản hồi thành công
                DynamicJsonDocument responseDoc(256);
                responseDoc["status"] = "success";
                responseDoc["message"] = "QR code activated";
                String response;
                serializeJson(responseDoc, response);
                mqttClient.publish(mqtt_topic_status, response.c_str());

                lv_print_qrcode(it->qrText.c_str(), it->bank.c_str(), 
                                it->name.c_str(), it->account.c_str());
            } else {
                // Phản hồi lỗi
                DynamicJsonDocument responseDoc(256);
                responseDoc["status"] = "error";
                responseDoc["message"] = "QR code ID not found";
                String response;
                serializeJson(responseDoc, response);
                mqttClient.publish(mqtt_topic_status, response.c_str());
            }
        }
        else if (command == "add") {
            QRCode qr;
            qr.id = nextQRId++;
            qr.name = doc["name"].as<String>();
            qr.account = doc["account"].as<String>();
            qr.bank = doc["bank"].as<String>();
            qr.qrText = doc["qrText"].as<String>();
            
            qrList.push_back(qr);
            saveQRListToFlash();
            
            // Publish updated list
            publishQRList();
            
            // Phản hồi thành công
            DynamicJsonDocument responseDoc(256);
            responseDoc["status"] = "success";
            responseDoc["message"] = "QR code added";
            String response;
            serializeJson(responseDoc, response);
            mqttClient.publish(mqtt_topic_status, response.c_str());
        }
        else if (command == "delete") {
            int idToDelete = doc["id"];
            auto it = std::find_if(qrList.begin(), qrList.end(),
                                  [idToDelete](const QRCode& qr) { return qr.id == idToDelete; });
            
            if (it != qrList.end()) {
                if (idToDelete == activeQRId) {
                    activeQRId = -1;
                    lv_obj_clean(lv_scr_act());
                }
                qrList.erase(it);
                saveQRListToFlash();
                publishQRList();
                
                // Phản hồi thành công
                DynamicJsonDocument responseDoc(256);
                responseDoc["status"] = "success";
                responseDoc["message"] = "QR code deleted";
                String response;
                serializeJson(responseDoc, response);
                mqttClient.publish(mqtt_topic_status, response.c_str());
            } else {
                // Phản hồi lỗi
                DynamicJsonDocument responseDoc(256);
                responseDoc["status"] = "error";
                responseDoc["message"] = "QR code ID not found";
                String response;
                serializeJson(responseDoc, response);
                mqttClient.publish(mqtt_topic_status, response.c_str());
            }
        }
        else if (command == "get_list") {
            Serial.println("Received get_list command");
            // Publish the current QR list
            publishQRList();
        }
    }
}

void mqtt_start(void) {
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqtt_callback);
}

void reconnect_mqtt() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection to ");
        Serial.print(mqtt_server);
        Serial.print("...");
        
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("connected");
            mqttClient.subscribe(mqtt_topic_config);  // Subscribe to the config topic
            publishQRList();
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}
#endif
void start_ap() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
}

void start_sta() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Đang kết nối WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nKết nối WiFi thành công, IP: " + WiFi.localIP().toString());
}

void screen_init() {
    if(disp != NULL) {  
        lv_display_delete(disp);
    }

    // Cấp phát bộ nhớ cho draw buffer từ PSRAM
    if(draw_buf == NULL) {
        draw_buf = (uint32_t *)heap_caps_malloc(DRAW_BUF_SIZE/4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    } else {
        ESP_LOGI(TAG, "Allocated draw buffer in PSRAM");
    }
    disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, DRAW_BUF_SIZE/4);
    lv_display_set_rotation(disp, TFT_ROTATION);
}

void setup() {
    Serial.begin(115200);
    if(psramInit()){
        Serial.println("PSRAM đã được khởi tạo");
    }else{
        Serial.println("PSRAM không được khởi tạo");
    }

    pinMode(23, OUTPUT);
    digitalWrite(23, 128);
    // Load QR list từ flash
    loadQRListFromFlash();

    lv_init();
    screen_init();
    // // In ra thông tin QR
    Serial.print("activeQRId in Flash: ");
    Serial.println(activeQRId);
    if(activeQRId != -1) {
        for(const auto& qr : qrList) {
            if(qr.id == activeQRId) {
                Serial.println("Active QR: " + String(qr.id));
                lv_print_qrcode(qr.qrText.c_str(),qr.bank.c_str(), qr.name.c_str(), qr.account.c_str());
                break;
            }
        }
    }

    start_sta();

#ifdef USE_MQTT
    mqtt_start();
#endif

#ifdef USE_WEB_SERVER
    // Route cho HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request!=NULL){
            request->send_P(200, "text/html", index_html, processor);
        }
    });

// Route cho CSS
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request!=NULL){
            request->send_P(200, "text/css", styles_css);
        }
    });

    // Route cho JavaScript
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request!=NULL){
            request->send_P(200, "application/javascript", script_js);
        }
    });

    // Route cho API
    server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
        ledState = !ledState;
        if(request!=NULL){
            request->send(200, "text/plain", ledState ? "ON" : "OFF");
        }
    });

    
    server.on("/system-info", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"dram\":";
        json += ESP.getFreeHeap();
        json += ",\"psram\":";
        json += ESP.getFreePsram();
        json += "}";
        if(request!=NULL){
            request->send(200, "application/json", json);
        }
    });

    // Lấy danh sách QR
    server.on("/qr-list", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(8192);
        
        // Thêm activeQRId vào response
        doc["activeQRId"] = activeQRId;
        
        // Thêm danh sách QR
        JsonArray array = doc.createNestedArray("qrList");
        for(const auto& qr : qrList) {
            JsonObject obj = array.createNestedObject();
            obj["id"] = qr.id;
            obj["name"] = qr.name;
            obj["account"] = qr.account;
            obj["bank"] = qr.bank;
            obj["qrText"] = qr.qrText;
        }
        
        String response;
        serializeJson(doc, response);
        if(request!=NULL){
            request->send(200, "application/json", response);
        }
    });
    
    // Thêm QR mới
    server.on("/save-qr", HTTP_POST, [](AsyncWebServerRequest *request){
        if(request!=NULL){
            request->send(200); // Acknowledge
        }
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, (char*)data);
        
        QRCode qr;
        qr.id = nextQRId++;  // Tự tạo ID mới
        qr.name = doc["name"].as<String>();
        qr.account = doc["account"].as<String>();
        qr.bank = doc["bank"].as<String>();
        qr.qrText = doc["qrText"].as<String>();
        
        Serial.println("\n=== New QR Code Added ===");
        Serial.print("ID: ");
        Serial.println(qr.id);
        Serial.print("Name: ");
        Serial.println(qr.name);
        Serial.print("Account: ");
        Serial.println(qr.account);
        Serial.print("Bank: ");
        Serial.println(qr.bank);
        Serial.print("QR Text: ");
        Serial.println(qr.qrText);
        Serial.println("=====================\n");
        
        qrList.push_back(qr);
        saveQRListToFlash();
        
        if(request!=NULL){
            request->send(200, "text/plain", "OK");
        }
    });
    
    // Kích hoạt QR
    server.on("/activate-qr", HTTP_POST, [](AsyncWebServerRequest *request){
        if(request!=NULL){
            request->send(200); // Acknowledge
        }
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        String idStr((char*)data, len);
        int id = idStr.toInt();
        
        // Tìm QR theo ID thay vì dùng ID làm index
        auto it = std::find_if(qrList.begin(), qrList.end(),
                              [id](const QRCode& qr) { return qr.id == id; });
        
        if (it != qrList.end()) {
            activeQRId = id;
            saveQRListToFlash();
            request->send(200, "text/plain", "OK");
            delay(50);
            lv_print_qrcode(it->qrText.c_str(), it->bank.c_str(), 
                            it->name.c_str(), it->account.c_str());
        } else {
            request->send(404, "text/plain", "QR not found");
        }
    });
    
    // Xóa QR
    server.on("^\\/qr\\/([0-9]+)$", HTTP_DELETE, [](AsyncWebServerRequest *request){
        if (request != NULL) {
            String idStr = request->pathArg(0);
            int idToDelete = idStr.toInt();
            
            Serial.print("Attempting to delete QR with ID: ");
            Serial.println(idToDelete);

            // Tìm QR trong danh sách
            bool found = false;
            for (size_t i = 0; i < qrList.size(); i++) {
                if (qrList[i].id == idToDelete) {
                    // Nếu QR đang active thì clear màn hình
                    if (idToDelete == activeQRId) {
                        activeQRId = -1;
                        lv_obj_clean(lv_scr_act());
                    }
                    
                    // Xóa QR khỏi danh sách
                    qrList.erase(qrList.begin() + i);
                    found = true;
                    
                    // Lưu thay đổi vào flash
                    saveQRListToFlash();
                    
                    Serial.println("QR deleted successfully");
                    break;
                }
            }
            
            if (found) {
                request->send(200, "text/plain", "OK");
            } else {
                Serial.println("QR not found");
                request->send(404, "text/plain", "QR không tìm thấy");
            }
        }
    });
    
    // System info
    server.on("/system-info", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(512);
        doc["dram"] = ESP.getFreeHeap();
        doc["psram"] = ESP.getFreePsram();
        doc["activeQR"] = activeQRId;
        
        String response;
        serializeJson(doc, response);
        if(request!=NULL){
            request->send(200, "application/json", response);
        }
    });

    server.begin();
#endif
}

void loop() {
    lv_timer_handler(); /* let the GUI do its work */
    delay(5);
    // if(WiFi.status() != WL_CONNECTED) {
    //     Serial.println("Mất kết nối WiFi! Đang kết nối lại...");
    //     WiFi.reconnect();
    // }

    if (!mqttClient.connected()) {
        reconnect_mqtt();
    }
    mqttClient.loop();
}
