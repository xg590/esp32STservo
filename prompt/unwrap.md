# 对话

```md
我的步进电机的磁编码器的角度读数范围是0-4095，均匀地对应于电机转动角度的0-360度。当顺时针多圈转动时，读数不断增大，直到超过一圈时从4095跳变回0，然后再次不断增大，当逆时针多圈转动时，读数不断减小，直到从0跳变为4095，然后再次不断减小。帮我写段python代码，让顺时针多圈转动时，读数连续增大可以超过4095，而逆时针转动时连续减小可以为负数
```

好的 👍
你遇到的情况是 **绝对角度传感器读数**（0–4095范围内循环），需要转换为 **连续角度值**（可以跨多圈）。
这其实就是一个 **unwrap（解包）角度** 的问题。

下面给你一段 Python 代码，能把磁编码器的原始读数映射到连续角度：

```python
class EncoderUnwrapper:
    def __init__(self, curr_posi=0, abs_max_posi=4095):
        self.last_raw     = None           # 上一次原始读数
        self.posi         = curr_posi      # 累计位置（单位：步）
        self.total_step = abs_max_posi + 1 # 编码器（0~4095）一圈共4096步

    def update(self, raw_value):
        """
        输入编码器原始读数 (0 ~ 4095)，返回连续的累积位置（可正可负）
        """
        if self.last_raw:
            # 计算相对变化量
            delta = raw_value - self.last_raw
    
            # 处理跳变：如果跨过了0点（顺时针4095->0 或 逆时针0->4095）
            if  delta  >  self.total_step // 2:  # 逆向跳变
                # 假设逆时针转，跳变前100，跳变后4000，delta为正3900，显著大于最大步数的一半
                delta -=  self.total_step
            elif delta < -self.total_step // 2:  # 顺向跳变
                # 假设顺时针转，跳变前4000，跳变后100，delta为负
                delta +=  self.total_step
    
            # 累加
            self.posi += delta
            self.last_raw = raw_value
        
        else:
            # 第一次调用，初始化
            self.last_raw = raw_value 

    def get_degrees(self):
        """返回累计角度，单位：度"""
        return self.posi * (360.0 / self.total_step)


# ==== 使用示例 ====
if __name__ == "__main__":
    encoder = EncoderUnwrapper()

    # 模拟原始读数变化（顺时针转过一圈多一些）
    raw_values = [1, 1000, 2500, 4000, 4090, 4092, 4095, 2, 10, 100, 500, 1000, 2000, 3000, 1]
    for raw in raw_values:
        encoder.update(raw)
        deg = encoder.get_degrees()
        print(f"raw={raw:4d},  posi={encoder.posi:6d},  deg={deg:8.2f}")
```
