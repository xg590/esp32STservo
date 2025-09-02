# ESP32刷机经验

* PlatformIO编译结果：三个文件

  | 文件名                   | 写入位置 | 含义 |
  | - | - | - |
  | **bootloader.bin**      |0x1000 | 引导程序：加载分区表和应用 |
  | **partition-table.bin** |0x8000 | 分区表：定义 Flash 的分区布局 |
  | **firmware.bin**        |0x10000| 应用程序 |

* 固件上传前按住BOOT触点开关，上传过程中按下板子上的使能EN触点开关，然后先后释放EN和BOOT。

  ```sh
  esptool --port COM14 erase-flash
  
  cd .pio\build\esp32dev
  
  esptool --chip esp32 --port COM14 --baud 460800 write-flash -z 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
  ```
