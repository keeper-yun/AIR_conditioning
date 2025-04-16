// v1.0
#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// 定义串口引脚
#define IR_TX_PIN 10  // ESP32 TX --> 红外模块 RX
#define IR_RX_PIN 9   // ESP32 RX <-- 红外模块 TX

// 开关指令（根据实际空调型号调整 aircod）
uint8_t ac_on_cmd[]  = {0xAA, 0x55, 0x09, 0x00, 0x00, 0x00, 0x31, 0xE9, 0x10, 0x00, 0x00, 0x3E};
uint8_t ac_off_cmd[] = {0xAA, 0x55, 0x09, 0x00, 0x00, 0x00, 0x31, 0x69, 0x10, 0x00, 0x00, 0xBE};



// Wi-Fi 配置信息
const char* ssid = "wzc";
const char* password = "1104312167";

// OneNET MQTT 服务器
const char* mqtt_server = "mqtts.heclouds.com";
const int mqtt_port = 1883;

// OneNET 设备信息
const char* device_id = "air_condition";  
const char* product_id = "21jg79i7GP";
const char* access_key = "version=2018-10-31&res=products%2F21jg79i7GP%2Fdevices%2Fair_condition&et=3320404378&method=md5&sign=QKStruG4S3ABZp8BLhlOEg%3D%3D";

// NTP 配置信息
const char* timeServer = "pool.ntp.org";  // NTP 时间服务器
const long utcOffsetInSeconds = 28800;   // 中国时区，UTC+8

// 创建 WiFiUDP 对象
WiFiUDP ntpUDP;

// 创建 NTP 客户端对象
NTPClient timeClient(ntpUDP, timeServer, utcOffsetInSeconds);

// MQTT 客户端
WiFiClient espClient;
PubSubClient client(espClient);

// 记录上次连接时间
unsigned long lastReconnectTime = 0;
unsigned long lastConnectionAttempt = 0;
unsigned long reconnectInterval = 120000 ;  // 每30分钟重新连接一次    120000 2分钟    1800000 半小时


// 连接 Wi-Fi
void setup_wifi() {
    delay(10);
    Serial.println("连接WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi 连接成功");
}

// 处理接收到的 MQTT 消息
void callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (int i = 0; i < length; i++) {
        if ((char)payload[i] != '\"') {
            message += (char)payload[i];
        }
    }
    Serial.println(message);

    // 判断消息内容，控制小灯开关
    if (message == "on") {
        // digitalWrite(lightPin, HIGH);  // 打开小灯
        // Serial.println("小灯已打开");
      Serial.println("发送空调开启指令");
      Serial2.write(ac_on_cmd, sizeof(ac_on_cmd)); 
    } else if (message == "off") {
        // digitalWrite(lightPin, LOW);   // 关闭小灯
        // Serial.println("小灯已关闭");
      Serial.println("发送空调关闭指令");
      Serial2.write(ac_off_cmd, sizeof(ac_off_cmd));
    }
}

// 连接 MQTT 服务器，并订阅主题
void reconnect() {
    while (!client.connected()) {
        Serial.print("尝试连接 OneNET MQTT...");

        if (client.connect(device_id, product_id, access_key)) {
            Serial.println("连接成功！");

            // 订阅 OneNET 主题
            client.subscribe("$sys/21jg79i7GP/air_condition/cmd/#");
            client.subscribe("$sys/21jg79i7GP/air_condition/dp/post/json/+");
            Serial.println("已订阅: $sys/.../cmd/# 和 $sys/.../dp/post/json/+");
        } else {
            Serial.print("连接失败，错误码 = ");
            Serial.println(client.state());
            delay(5000);  // 重试连接
        }
    }
}

// 发送 JSON 数据到 OneNET
void sendData() {
    String payload = "{"
                     "\"id\":123,"
                     "\"dp\":{"
                     "\"temperature\":[{\"v\":30}],"
                     "\"humidity\":[{\"v\":4.5}]"
                     "}";

    Serial.println("发送数据: " + payload);
    client.publish("$sys/21jg79i7GP/air_condition/dp/post/json", " ");
}

// 检查当前时间是否在 18:00 到 22:00 之间
bool isTimeToConnect() {
    timeClient.update();
    int currentHour = timeClient.getHours();  // 获取当前小时
    return (currentHour > 10 && currentHour < 12);  // 判断是否在 18:00 - 22:00 之间
}

void setup() {
    Serial.begin(9600);
    Serial2.begin(9600, SERIAL_8N1, IR_RX_PIN, IR_TX_PIN);
    delay(1000);
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);  // 绑定回调函数

    timeClient.begin();  // 初始化 NTP 客户端
}

void loop() {
    unsigned long currentMillis = millis();
    
    // 每隔30分钟重新连接一次
    if (currentMillis - lastConnectionAttempt >= reconnectInterval) {
        lastConnectionAttempt = currentMillis;
        Serial.println("尝试重新连接 OneNET...");

        // 判断是否在 18:00 到 22:00 之间
        if (isTimeToConnect()) {
            if (!client.connected()) {
                reconnect();
            }
        } else {
            Serial.println("当前时间不在连接时间范围内，进入深度睡眠...");
            // 进入深度睡眠模式 (将 CPU 休眠，降低功耗)
            ESP.deepSleep(0);  // 使 ESP32 进入深度睡眠状态，0 表示永久睡眠
        }
    }

    // 确保 NTP 客户端每次都更新
    timeClient.update();

    if (client.connected()) {
        client.loop();  // 保持与 MQTT 服务器的连接
        sendData();  // 发送数据到 OneNET
    }

    delay(10000);  // 每 10 秒发送一次数据
}
