#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define IR_TX_PIN 10
#define IR_RX_PIN 9

uint8_t ac_on_cmd[]  = {0xAA, 0x55, 0x09, 0x00, 0x00, 0x00, 0x31, 0xE9, 0x10, 0x00, 0x00, 0x3E};
uint8_t ac_off_cmd[] = {0xAA, 0x55, 0x09, 0x00, 0x00, 0x00, 0x31, 0x69, 0x10, 0x00, 0x00, 0xBE};

// Wi-Fi & OneNET
const char* ssid = "wzc";
const char* password = "1104312167";
const char* mqtt_server = "mqtts.heclouds.com";
const int mqtt_port = 1883;
const char* device_id = "air_condition";  
const char* product_id = "21jg79i7GP";
const char* access_key = "version=2018-10-31&res=products%2F21jg79i7GP%2Fdevices%2Fair_condition&et=3320404378&method=md5&sign=QKStruG4S3ABZp8BLhlOEg%3D%3D";

// NTP
const char* timeServer = "pool.ntp.org";
const long utcOffsetInSeconds = 28800;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, timeServer, utcOffsetInSeconds);
WiFiClient espClient;
PubSubClient client(espClient);

// 状态控制
bool ac_auto_on_sent = false;
bool ac_auto_off_sent = false;
bool mqtt_connected = false;

// 是否在活跃时间段（18:00-22:00）或（4:30-4:40）
bool isActiveTime(int h, int m) {
    return (h >= 18 && h < 22) || (h == 4 && m >= 30 && m < 40);
}
bool isAutoOnTime(int h) {
    return h >= 18 && h < 22;
}
bool isAutoOffTime(int h, int m) {
    return h == 4 && m >= 30 && m < 40;
}

void setup_wifi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi 已连接");
}

void callback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) {
        if ((char)payload[i] != '\"') msg += (char)payload[i];
    }
    Serial.println("MQTT消息: " + msg);
    if (msg == "on") {
        Serial.println("远程控制: 打开空调");
        Serial2.write(ac_on_cmd, sizeof(ac_on_cmd)); 
    } else if (msg == "off") {
        Serial.println("远程控制: 关闭空调");
        Serial2.write(ac_off_cmd, sizeof(ac_off_cmd)); 
    }
}

void connectMQTT() {
    if (!client.connected()) {
        if (client.connect(device_id, product_id, access_key)) {
            Serial.println("MQTT连接成功！");
            client.subscribe("$sys/21jg79i7GP/air_condition/cmd/#");
            mqtt_connected = true;
        } else {
            Serial.println("MQTT连接失败，状态码：" + String(client.state()));
            mqtt_connected = false;
        }
    }
}

void disconnectMQTT() {
    if (client.connected()) {
        client.disconnect();
        mqtt_connected = false;
        Serial.println("MQTT已断开");
    }
}

void setup() {
    Serial.begin(9600);
    Serial2.begin(9600, SERIAL_8N1, IR_RX_PIN, IR_TX_PIN);
    delay(1000);
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    timeClient.begin();
}

void loop() {
    timeClient.update();
    int hour = timeClient.getHours();
    int minute = timeClient.getMinutes();

    // 活跃时间段：保持MQTT连接并监听消息
    if (isActiveTime(hour, minute)) {
        if (WiFi.status() != WL_CONNECTED) setup_wifi();
        if (!mqtt_connected) connectMQTT();
        if (mqtt_connected) client.loop();

        // 自动开空调（只发一次）
        if (isAutoOnTime(hour) && !ac_auto_on_sent) {
            Serial.println("自动开启空调");
            Serial2.write(ac_on_cmd, sizeof(ac_on_cmd));
            ac_auto_on_sent = true;
            ac_auto_off_sent = false;
        }

        // 自动关空调（只发一次）
        if (isAutoOffTime(hour, minute) && !ac_auto_off_sent) {
            Serial.println("自动关闭空调");
            Serial2.write(ac_off_cmd, sizeof(ac_off_cmd));
            ac_auto_off_sent = true;
            ac_auto_on_sent = false;
        }

    } else {
        // 非活跃时段：断开 MQTT 并深度睡眠
        if (mqtt_connected) disconnectMQTT();
        Serial.println("非活跃时段，准备睡眠");
        ESP.deepSleep(60 * 1e6);  // 睡眠1分钟，单位是微秒
    }

    delay(5000);  // 等待 5 秒再次循环
}
