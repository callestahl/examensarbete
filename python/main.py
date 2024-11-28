import asyncio
import os
from bleak import BleakScanner, BleakClient
from tkinter import Tk, Label, Button
from tkinterdnd2 import TkinterDnD, DND_FILES
import threading

#define 

class BLEApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Test")
        self.root.geometry("500x300")
        self.root.configure(bg="#1A1A1A")

        self.device_address = None
        self.current_file = None

        self.drop_label = Label(
            root,
            text="Drop File",
            font=("Arial", 24),
            fg="#A0A0A0",
            bg="#1A1A1A",
            relief="solid",
            bd=2,
            padx=20,
            pady=20,
        )
        self.drop_label.place(relx=0.5, rely=0.3, anchor="center")

        self.send_button = Button(
            root,
            text="Send File",
            font=("Arial", 14),
            #state="disabled",
            command=self.transfer_file_to_device,
        )
        self.send_button.place(relx=0.5, rely=0.6, anchor="center")

        self.sound_buffer = self.file_to_buffer("../processedAudio.txt")

        root.drop_target_register(DND_FILES)
        root.dnd_bind("<<Drop>>", self.on_drag_dropped)

        self.start_discovery()

    def on_drag_dropped(self, event):
        file_path = event.data.strip()
        if os.path.isfile(file_path):
            self.current_file = file_path
            self.drop_label.config(text=f"File: {os.path.basename(file_path)}")
            self.send_button.config(state="normal")
        else:
            self.drop_label.config(text="Invalid file dropped")

    def file_to_buffer(self, file_path):
        try:
            with open(file_path, 'rb') as file:
                file_content = bytearray(file.read())
            return file_content 
        except FileNotFoundError:
            print(f"Error: File not found at path: {file_path}")
        except Exception as e:
            print(f"An error occurred: {e}")

    def start_discovery(self):
        print("Looking for devices...")
        threading.Thread(target=self.run_ble_discovery, daemon=True).start()

    def run_ble_discovery(self):
        asyncio.run(self.discover_bluetooth_devices())

    async def discover_bluetooth_devices(self):
        devices = await BleakScanner.discover()
        for device in devices:
            print(f"Found device: {device.name} ({device.address})")
            if device.name == "WaveTablePP":  
                self.device_address = device.address
                print(f"Connected to: {device.name} ({device.address})")
                return
        print("Device not found")

    def transfer_file_to_device(self):
        #if not self.device_address or not self.current_file:
        if not self.device_address:
            print("No file or device to send to")
            return
        threading.Thread(target=self.start_sending_file, daemon=True).start()

    def start_sending_file(self):
        asyncio.run(self.send_file())


    async def send_file(self):
        try:
            print("start")
            async with BleakClient(self.device_address) as client:
                print("connected")
                #mtu_size = client.mtu_size
                #chunk_size = min(mtu_size - 3, 512) # -3 ATT header, 512 max Bluetooth spec p. 1485
                #print(f"Chunk Size: {chunk_size}")

                #characteristic_uuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
                #sound_buffer_len = len(self.sound_buffer);
                #if sound_buffer_len % 2 != 0:
                #    print("Error: Sound file are not even");
                #    return;

                #chunks = sound_buffer_len // chunk_size;
                #remainder = sound_buffer_len % chunk_size;

                #for i in range(chunks):
                #    offset = chunk_size * i
                #    await client.write_gatt_char(
                #        characteristic_uuid,
                #        self.sound_buffer[offset:offset + chunk_size],
                #        response=True 
                #    )

                #if remainder > 0:
                #    await client.write_gatt_char(
                #        characteristic_uuid,
                #        self.sound_buffer[chunks * chunk_size:],
                #        response=True
                #    )

                #done_message = "DONE"
                #await client.write_gatt_char(
                #        characteristic_uuid,
                #        done_message.encode('utf-8')
                #)


                print("File transferred successfully!")

        except Exception as e:
            print(f"Failed to send file: {e}")


if __name__ == "__main__":
    root = TkinterDnD.Tk() 
    app = BLEApp(root)
    root.mainloop()

