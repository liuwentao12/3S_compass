import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
import serial.tools.list_ports
import threading
import time
import asyncio
from bleak import BleakClient, BleakScanner
import os

# BLE GATT Characteristics UUIDs
CMD_CHAR_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"
OTA_CHAR_UUID = "0000ff02-0000-1000-8000-00805f9b34fb"

class CompassHostApp:
    def __init__(self, root):
        self.root = root
        self.root.title("3S_Compass 上位机控制台 (支持 BLE OTA)")
        self.root.geometry("450x550")

        self.serial_port = None
        self.is_connected = False
        self.ota_file_path = ""

        self.create_widgets()
        self.refresh_ports()

    def create_widgets(self):
        # 串口配置区
        frame_port = ttk.LabelFrame(self.root, text="设备连接 (串口)", padding=10)
        frame_port.pack(fill=tk.X, padx=10, pady=5)

        ttk.Label(frame_port, text="串口:").grid(row=0, column=0, padx=5, pady=5)
        self.port_combobox = ttk.Combobox(frame_port, width=15)
        self.port_combobox.grid(row=0, column=1, padx=5, pady=5)

        self.btn_refresh = ttk.Button(frame_port, text="刷新", command=self.refresh_ports, width=6)
        self.btn_refresh.grid(row=0, column=2, padx=5, pady=5)

        self.btn_connect = ttk.Button(frame_port, text="连接", command=self.toggle_connection)
        self.btn_connect.grid(row=0, column=3, padx=5, pady=5)

        # 目标设置区
        frame_dest = ttk.LabelFrame(self.root, text="目标地点发送", padding=10)
        frame_dest.pack(fill=tk.X, padx=10, pady=5)

        ttk.Label(frame_dest, text="纬度 (Lat):").grid(row=0, column=0, sticky=tk.W, pady=2)
        self.entry_lat = ttk.Entry(frame_dest, width=20)
        self.entry_lat.insert(0, "39.916345")
        self.entry_lat.grid(row=0, column=1, pady=2)

        ttk.Label(frame_dest, text="经度 (Lon):").grid(row=1, column=0, sticky=tk.W, pady=2)
        self.entry_lon = ttk.Entry(frame_dest, width=20)
        self.entry_lon.insert(0, "116.397155")
        self.entry_lon.grid(row=1, column=1, pady=2)

        self.btn_send_dest = ttk.Button(frame_dest, text="发送目标并导航", command=self.send_destination)
        self.btn_send_dest.grid(row=2, column=0, columnspan=2, pady=10, sticky=tk.EW)

        # 模式与校准区
        frame_cmd = ttk.LabelFrame(self.root, text="控制命令", padding=10)
        frame_cmd.pack(fill=tk.X, padx=10, pady=5)

        self.btn_compass_mode = ttk.Button(frame_cmd, text="返回指南针模式", command=self.set_compass_mode)
        self.btn_compass_mode.grid(row=0, column=0, padx=5, pady=5, sticky=tk.EW)

        self.btn_calibrate = ttk.Button(frame_cmd, text="罗盘8字校准", command=self.start_calibration)
        self.btn_calibrate.grid(row=0, column=1, padx=5, pady=5, sticky=tk.EW)

        frame_cmd.columnconfigure(0, weight=1)
        frame_cmd.columnconfigure(1, weight=1)

        # BLE OTA 升级区
        frame_ota = ttk.LabelFrame(self.root, text="BLE 无感 OTA 升级", padding=10)
        frame_ota.pack(fill=tk.X, padx=10, pady=5)
        
        self.lbl_ota_file = ttk.Label(frame_ota, text="未选择固件 (.bin)")
        self.lbl_ota_file.pack(fill=tk.X, pady=2)
        
        btn_frame = ttk.Frame(frame_ota)
        btn_frame.pack(fill=tk.X, pady=5)
        
        self.btn_select_file = ttk.Button(btn_frame, text="选择固件", command=self.select_ota_file)
        self.btn_select_file.pack(side=tk.LEFT, padx=5, expand=True, fill=tk.X)
        
        self.btn_start_ota = ttk.Button(btn_frame, text="开始蓝牙 OTA", command=self.start_ota_thread)
        self.btn_start_ota.pack(side=tk.LEFT, padx=5, expand=True, fill=tk.X)
        
        self.ota_progress = ttk.Progressbar(frame_ota, orient=tk.HORIZONTAL, mode='determinate')
        self.ota_progress.pack(fill=tk.X, pady=5)
        
        self.lbl_ota_status = ttk.Label(frame_ota, text="就绪")
        self.lbl_ota_status.pack(fill=tk.X, pady=2)

        # 状态栏
        self.status_var = tk.StringVar()
        self.status_var.set("状态: 未连接")
        status_bar = ttk.Label(self.root, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W)
        status_bar.pack(side=tk.BOTTOM, fill=tk.X)

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_list = [port.device for port in ports]
        self.port_combobox['values'] = port_list
        if port_list:
            self.port_combobox.current(0)

    def toggle_connection(self):
        if not self.is_connected:
            port = self.port_combobox.get()
            if not port:
                messagebox.showwarning("提示", "请选择串口!")
                return
            try:
                self.serial_port = serial.Serial(port, 115200, timeout=1)
                self.is_connected = True
                self.btn_connect.config(text="断开")
                self.status_var.set(f"状态: 已连接至 {port}")
            except Exception as e:
                messagebox.showerror("错误", f"无法打开串口: {str(e)}")
        else:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            self.is_connected = False
            self.btn_connect.config(text="连接")
            self.status_var.set("状态: 未连接")

    def send_command(self, cmd_str):
        if self.is_connected and self.serial_port:
            try:
                full_cmd = f"{cmd_str}\r\n"
                self.serial_port.write(full_cmd.encode('utf-8'))
                self.status_var.set(f"已发送: {cmd_str}")
            except Exception as e:
                messagebox.showerror("错误", f"发送失败: {str(e)}")
        else:
            messagebox.showwarning("提示", "请先连接串口!")

    def send_destination(self):
        lat = self.entry_lat.get().strip()
        lon = self.entry_lon.get().strip()
        if not lat or not lon:
            messagebox.showwarning("提示", "经纬度不能为空")
            return
        cmd = f"DEST:{lat},{lon}"
        self.send_command(cmd)

    def set_compass_mode(self):
        self.send_command("MODE:COMPASS")

    def start_calibration(self):
        if messagebox.askyesno("校准罗盘", "点击确认后，请拿起设备在空中画'8'字持续约10秒钟。"):
            self.send_command("CALI:START")

    def select_ota_file(self):
        filepath = filedialog.askopenfilename(
            title="选择固件文件", 
            filetypes=[("Bin Files", "*.bin"), ("All Files", "*.*")]
        )
        if filepath:
            self.ota_file_path = filepath
            filename = os.path.basename(filepath)
            self.lbl_ota_file.config(text=f"已选择: {filename}")

    def start_ota_thread(self):
        if not self.ota_file_path or not os.path.exists(self.ota_file_path):
            messagebox.showwarning("提示", "请先选择有效的 .bin 固件文件")
            return
        
        self.btn_start_ota.config(state=tk.DISABLED)
        self.lbl_ota_status.config(text="搜索设备中...")
        self.ota_progress['value'] = 0
        
        # 启动后台异步线程
        threading.Thread(target=self.run_async_ota, daemon=True).start()

    def run_async_ota(self):
        asyncio.run(self.ble_ota_task())

    async def ble_ota_task(self):
        target_name = "3S_Compass"
        device = None
        
        try:
            # 1. 扫描设备
            devices = await BleakScanner.discover(timeout=5.0)
            for d in devices:
                if d.name and target_name in d.name:
                    device = d
                    break
            
            if not device:
                self.root.after(0, lambda: self.lbl_ota_status.config(text="未找到名为 3S_Compass 的蓝牙设备"))
                self.root.after(0, lambda: self.btn_start_ota.config(state=tk.NORMAL))
                return
            
            self.root.after(0, lambda: self.lbl_ota_status.config(text=f"找到设备 {device.address}，正在连接..."))
            
            # 2. 连接设备
            async with BleakClient(device) as client:
                self.root.after(0, lambda: self.lbl_ota_status.config(text="已连接，请求 MTU 并准备升级..."))
                
                # 发送 OTA:START 指令
                await client.write_gatt_char(CMD_CHAR_UUID, b"OTA:START")
                await asyncio.sleep(0.5)
                
                # 3. 读取固件文件
                with open(self.ota_file_path, "rb") as f:
                    firmware_data = f.read()
                
                total_size = len(firmware_data)
                self.root.after(0, lambda: self.ota_progress.config(maximum=total_size))
                
                # 4. 分块传输 (ESP32 GATT 通常支持 MTU 512，这里我们保守采用 200 字节)
                chunk_size = 200
                sent = 0
                
                self.root.after(0, lambda: self.lbl_ota_status.config(text=f"开始传输固件 ({total_size} 字节)..."))
                
                for i in range(0, total_size, chunk_size):
                    chunk = firmware_data[i:i+chunk_size]
                    await client.write_gatt_char(OTA_CHAR_UUID, chunk, response=False)
                    sent += len(chunk)
                    
                    # 更新进度条 (每 10 个 chunk 刷新一次 UI 以防卡顿)
                    if (i // chunk_size) % 10 == 0 or sent == total_size:
                        self.root.after(0, lambda s=sent: self.update_ota_progress(s, total_size))
                        
                    # 给 ESP32 一点处理时间，防止队列堵塞
                    await asyncio.sleep(0.02)
                
                # 5. 发送结束指令
                self.root.after(0, lambda: self.lbl_ota_status.config(text="传输完成，发送重启指令..."))
                await client.write_gatt_char(CMD_CHAR_UUID, b"OTA:END")
                
                self.root.after(0, lambda: self.lbl_ota_status.config(text="✅ OTA 升级成功！设备将自动重启。"))
                self.root.after(0, lambda: messagebox.showinfo("成功", "OTA 升级成功！"))
                
        except Exception as e:
            self.root.after(0, lambda err=str(e): self.lbl_ota_status.config(text=f"❌ OTA 失败: {err}"))
        finally:
            self.root.after(0, lambda: self.btn_start_ota.config(state=tk.NORMAL))

    def update_ota_progress(self, sent, total):
        self.ota_progress['value'] = sent
        percent = int((sent / total) * 100)
        self.lbl_ota_status.config(text=f"传输中... {percent}% ({sent}/{total} 字节)")

if __name__ == "__main__":
    root = tk.Tk()
    app = CompassHostApp(root)
    root.mainloop()
