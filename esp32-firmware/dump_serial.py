import serial
import time
import sys
import os

# Build 42: Persistent Logger
PORT = sys.argv[1] if len(sys.argv) > 1 else 'COM3'
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
LOG_FILE = "esp_scan_log.txt"

def main():
    print(f"Connecting to {PORT} at {BAUD}...")
    with open(LOG_FILE, "a") as f:
        f.write(f"\n--- SESSION START: {time.ctime()} ---\n")
        f.flush()
        
        while True:
            try:
                ser = serial.Serial(PORT, BAUD, timeout=0.1)
                print(f"Connected to {PORT}. Streaming to {LOG_FILE}...")
                while True:
                    line = ser.readline()
                    if line:
                        decoded = line.decode('utf-8', errors='ignore').strip()
                        if decoded:
                            # Echo to terminal so agent can see it in status
                            print(decoded)
                            f.write(decoded + "\n")
                            f.flush()
                    
                    # Check for stop signal
                    if os.path.exists("STOP_LOGGING"):
                        print("Stop signal detected.")
                        return
                        
            except serial.SerialException:
                # Port might be busy (e.g. during upload)
                time.sleep(1)
            except KeyboardInterrupt:
                return
            except Exception as e:
                print(f"Error: {e}")
                time.sleep(1)

if __name__ == "__main__":
    main()
