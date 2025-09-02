#!/usr/bin/env python3
"""
ESP32 ST3215 TCP客户端测试程序
通过WiFi控制ESP32上的ST3215舵机
"""

import socket
import json
import time
import sys

class ESP32ServoClient:
    def __init__(self, host, port=8888):
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False
    
    def connect(self):
        """连接到ESP32 TCP服务器"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            self.connected = True
            
            # 接收欢迎消息
            welcome = self.socket.recv(1024).decode().strip()
            print(f"服务器响应: {welcome}")
            return True
            
        except Exception as e:
            print(f"连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开连接"""
        if self.socket:
            self.socket.close()
            self.connected = False
    
    def send_command(self, func, **kwargs):
        """发送命令到ESP32"""
        if not self.connected:
            print("未连接到服务器")
            return None
        
        # 构建JSON命令
        command = {"func": func}
        command.update(kwargs)
        
        try:
            # 发送命令
            command_str = json.dumps(command) + '\n'
            self.socket.send(command_str.encode())
            print(f"发送命令: {command}")
            
            # 接收响应
            response = self.socket.recv(1024).decode().strip()
            result = json.loads(response)
            print(f"服务器响应: {result}")
            return result
            
        except Exception as e:
            print(f"命令发送失败: {e}")
            return None
    
    def ping(self, servo_id):
        """测试舵机连接"""
        return self.send_command("ping", id=servo_id)
    
    def move_to_position(self, servo_id, position, velocity=800, acceleration=100):
        """移动单个舵机到指定位置"""
        return self.send_command("move2Posi", 
                                id=servo_id, 
                                position=position, 
                                velocity=velocity, 
                                acceleration=acceleration)
    
    def move_multiple_to_positions(self, servo_ids, positions, velocity=800, acceleration=100):
        """移动多个舵机到指定位置"""
        return self.send_command("move2Posi", 
                                ids=servo_ids, 
                                positions=positions, 
                                velocity=velocity, 
                                acceleration=acceleration)
    
    def read_position(self, servo_id):
        """读取单个舵机位置"""
        return self.send_command("readPosi", id=servo_id)
    
    def read_multiple_positions(self, servo_ids):
        """读取多个舵机位置"""
        return self.send_command("readPosi", ids=servo_ids)
    
    def set_mode(self, servo_id, mode="posi"):
        """设置舵机工作模式"""
        return self.send_command("setMode", id=servo_id, mode=mode)
    
    def set_torque_mode(self, servo_id, mode="free"):
        """设置舵机力矩模式"""
        return self.send_command("setTorqueMode", id=servo_id, mode=mode)

def main():
    if len(sys.argv) < 2:
        print("用法: python test_tcp_client.py <ESP32_IP地址>")
        print("例如: python test_tcp_client.py 192.168.1.100")
        return
    
    esp32_ip = sys.argv[1]
    client = ESP32ServoClient(esp32_ip)
    
    print(f"连接到ESP32 ({esp32_ip}:8888)...")
    if not client.connect():
        return
    
    try:
        # 测试序列
        print("\n=== 开始测试序列 ===")
        
        # 1. 测试Ping
        print("\n1. 测试舵机连接...")
        for servo_id in [3, 4]:
            result = client.ping(servo_id)
            if result and result.get("success"):
                print(f"舵机 {servo_id}: 连接成功")
            else:
                print(f"舵机 {servo_id}: 连接失败")
        
        # 2. 设置模式
        print("\n2. 设置舵机模式...")
        for servo_id in [3, 4]:
            client.set_mode(servo_id, "posi")
            client.set_torque_mode(servo_id, "torque")
        
        # 3. 读取当前位置
        print("\n3. 读取当前位置...")
        result = client.read_multiple_positions([3, 4])
        if result and result.get("success"):
            print("当前位置:")
            for servo_id, data in result.get("data", {}).items():
                print(f"  舵机 {servo_id}: 位置={data['position']}, 速度={data['velocity']}, 负载={data['load']}")
        
        # 4. 单舵机移动测试
        print("\n4. 单舵机移动测试...")
        positions = [512, 1024, 1536, 2048, 2560, 3072, 3584, 4095]
        for pos in positions[:3]:  # 测试前3个位置
            print(f"移动舵机3到位置 {pos}")
            client.move_to_position(3, pos, velocity=1000)
            time.sleep(1.5)
            
            # 读取位置确认
            result = client.read_position(3)
            if result and result.get("success"):
                actual_pos = result.get("position", 0)
                print(f"  实际位置: {actual_pos}")
        
        # 5. 多舵机同步移动测试
        print("\n5. 多舵机同步移动测试...")
        test_positions = [
            [1024, 3072],  # 舵机3到1024, 舵机4到3072
            [2048, 2048],  # 两个舵机都到中间位置
            [3072, 1024],  # 交换位置
        ]
        
        for positions in test_positions:
            print(f"移动舵机[3,4]到位置 {positions}")
            client.move_multiple_to_positions([3, 4], positions, velocity=800)
            time.sleep(2)
            
            # 读取位置确认
            result = client.read_multiple_positions([3, 4])
            if result and result.get("success"):
                print("  实际位置:")
                for servo_id, data in result.get("data", {}).items():
                    print(f"    舵机 {servo_id}: {data['position']}")
        
        print("\n=== 测试完成 ===")
        
        # 交互模式
        print("\n进入交互模式，输入 'exit' 退出")
        while True:
            try:
                cmd = input("\n请输入命令 (ping/move/read/exit): ").strip().lower()
                
                if cmd == "exit":
                    break
                elif cmd == "ping":
                    servo_id = int(input("舵机ID: "))
                    client.ping(servo_id)
                elif cmd == "move":
                    servo_id = int(input("舵机ID: "))
                    position = int(input("目标位置 (0-4095): "))
                    client.move_to_position(servo_id, position)
                elif cmd == "read":
                    servo_id = int(input("舵机ID (或输入0读取所有): "))
                    if servo_id == 0:
                        client.read_multiple_positions([3, 4])
                    else:
                        client.read_position(servo_id)
                else:
                    print("未知命令")
                    
            except ValueError:
                print("输入格式错误")
            except KeyboardInterrupt:
                print("\n用户中断")
                break
    
    finally:
        client.disconnect()
        print("已断开连接")

if __name__ == "__main__":
    main()
