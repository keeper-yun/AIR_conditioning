

// 定义串口引脚
#define IR_TX_PIN 10  // ESP32 TX --> 红外模块 RX
#define IR_RX_PIN 9   // ESP32 RX <-- 红外模块 TX

// 开关指令（根据实际空调型号调整 aircod）
uint8_t ac_on_cmd[]  = {0xAA, 0x55, 0x09, 0x00, 0x00, 0x00, 0x31, 0xE9, 0x10, 0x00, 0x00, 0x3E};
uint8_t ac_off_cmd[] = {0xAA, 0x55, 0x09, 0x00, 0x00, 0x00, 0x31, 0x69, 0x10, 0x00, 0x00, 0xBE};

void setup() {
  Serial.begin(9600);  // 用于调试信息
  Serial2.begin(9600, SERIAL_8N1, IR_RX_PIN, IR_TX_PIN);  // 指定 GPIO9、10 为 RX、TX

  delay(1000);
  Serial.println("空调控制开始！（串口输入 1 开机,0 关机）");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();  // 从串口接收一个字符

    if (cmd == '1') {
      Serial.println("发送空调开启指令");
      Serial2.write(ac_on_cmd, sizeof(ac_on_cmd));  // 发送开机指令
    }
    else if (cmd == '0') {
      Serial.println("发送空调关闭指令");
      Serial2.write(ac_off_cmd, sizeof(ac_off_cmd));  // 发送关机指令
    }
  }

  delay(100); // 稍作延时
}


