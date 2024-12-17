#include <TFT_eSPI.h>
#include "../.pio/libdeps/esp-wrover-kit/lvgl/examples/lv_examples.h"
#include "../.pio/libdeps/esp-wrover-kit/lv_conf.h"
#include <lvgl.h>

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "html/web_content.h"

const char* ssid = "LUMI_TEST";
const char* password = "lumivn274!";

#define TAG "QR_CODE"
/*Set to your screen resolution and rotation*/
#define TFT_HOR_RES   320
#define TFT_VER_RES   480
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
// uint32_t draw_buf[DRAW_BUF_SIZE / 4];

uint32_t *draw_buf = NULL;
AsyncWebServer server(80);
bool ledState = false;
Preferences preferences;

// Struct để lưu thông tin QR
struct QRCode {
    int id;
    String name;
    String account;
    String bank;
    String qrText;
};

// Vector để lưu danh sách QR
std::vector<QRCode> qrList;
int activeQRId = -1;

// Thêm biến để track ID
static int nextQRId = 1;  // Thêm ở phần khai báo global

// Hàm helper để lưu QR list vào flash
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

// Hàm helper để đọc QR list từ flash
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

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    /*For example ("my_..." functions needs to be implemented by you)
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    my_set_window(area->x1, area->y1, w, h);
    my_draw_bitmaps(px_map, w * h);
     */

    // Báo cho LVGL biết đã vẽ xong
    lv_display_flush_ready(disp);
}

#ifdef LV_USE_QRCODE
void lv_print_qrcode(const char* qr_text, const char* bank_name, const char* name, const char* account)
{
    lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
    lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

    lv_obj_t * qr = lv_qrcode_create(lv_scr_act());
    lv_qrcode_set_size(qr, 250);
    lv_qrcode_set_dark_color(qr, fg_color);
    lv_qrcode_set_light_color(qr, bg_color);

    /*Set data*/
    lv_qrcode_update(qr, qr_text, strlen(qr_text));
    lv_obj_center(qr);

    /*Add a border with bg_color*/
    lv_obj_set_style_border_color(qr, bg_color, 0);
    lv_obj_set_style_border_width(qr, 5, 0);

    // Tạo label cho số tài khoản (giữ nguyên style mặc định)
    lv_obj_t * bank_label = lv_label_create(lv_scr_act());
    lv_label_set_text(bank_label, bank_name);
    lv_obj_set_style_text_font(bank_label, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(bank_label, lv_color_black(), 0);  
    lv_obj_align_to(bank_label, qr, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // Tạo label cho tên
    lv_obj_t * name_label = lv_label_create(lv_scr_act());
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(name_label, lv_color_black(), 0);            // Màu đen
    lv_obj_align_to(name_label, qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    // Tạo label cho số tài khoản (giữ nguyên style mặc định)
    lv_obj_t * account_label = lv_label_create(lv_scr_act());
    lv_label_set_text(account_label, account);
    lv_obj_set_style_text_font(account_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(account_label, lv_color_black(), 0);  
    lv_obj_align_to(account_label, name_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
}
#endif


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
    lv_display_t * disp;
    // Cấp phát bộ nhớ cho draw buffer từ PSRAM
    draw_buf = (uint32_t *)heap_caps_malloc(DRAW_BUF_SIZE/4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(draw_buf == NULL) {
    } else {
        ESP_LOGI(TAG, "Allocated draw buffer in PSRAM");
    }
    disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, DRAW_BUF_SIZE/4);
    lv_display_set_rotation(disp, TFT_ROTATION);

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
        
        activeQRId = id;
        saveQRListToFlash();
        
        // TODO: Cập nhật hiển thị QR trên màn hình
        if(request!=NULL){
            request->send(200, "text/plain", "OK");
        }
        delay(50);
        lv_print_qrcode(qrList[id].qrText.c_str(),qrList[id].bank.c_str(), qrList[id].name.c_str(), qrList[id].account.c_str());
    });
    
    // Xóa QR
    server.on("^\\/qr\\/([0-9]+)$", HTTP_DELETE, [](AsyncWebServerRequest *request){
        String idStr = request->pathArg(0);
        int id = idStr.toInt();
        
        // Tìm và xóa QR
        for(auto it = qrList.begin(); it != qrList.end(); ++it) {
            if(it->id == id) {
                if(id == activeQRId) {
                    activeQRId = -1;
                }
                qrList.erase(it);
                break;
            }
        }
        
        saveQRListToFlash();
        if(request!=NULL){
            request->send(200, "text/plain", "OK");
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
}

void loop() {
    lv_timer_handler(); /* let the GUI do its work */
    delay(500); /* let this time pass */
    // Kiểm tra kết nối WiFi
    // if(WiFi.status() != WL_CONNECTED) {
    //     Serial.println("Mất kết nối WiFi! Đang kết nối lại...");
    //     WiFi.reconnect();
    // }
}
