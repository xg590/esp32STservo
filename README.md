# ESP32 based Driver Board of STServo

## 刷机 / Flashing

* 将驱动板置于刷机模式的方法：先按下BOOT按钮，再按下EN按钮，再松开EN按钮，最后松开BOOT按钮。
```sh
CURRENT: upload_protocol = esptool
Looking for upload port...
Auto-detected: COM14
Uploading .pio\build\esp32dev\firmware.bin
esptool.py v4.9.0
Serial port COM14
Connecting..........
```

## Feetech ST3215

| Leader-Arm Axis     | Motor | Gear Ratio |     Code     |
| ------------------- | :---: | :--------: | :----------: |
| Base / Shoulder Pan |   1   |  1 / 191   | ST-3215-C044 |
| Shoulder Lift       |   2   |  1 / 345   | ST-3215-C001 |
| Elbow Flex          |   3   |  1 / 191   | ST-3215-C044 |
| Wrist Flex          |   4   |  1 / 147   | ST-3215-C046 |
| Wrist Roll          |   5   |  1 / 147   | ST-3215-C046 |
| Gripper             |   6   |  1 / 147   | ST-3215-C046 |
