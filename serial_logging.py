import serial
import csv
import time

# --- CONFIGURATION ---
SERIAL_PORT = "COM9"
BAUD_RATE = 115200
CSV_FILENAME = "logged_data_overheat2.csv"
SAMPLES_PER_BATCH = 100

sample_logged = 0

# Increase timeout to 3 seconds to handle the "Long Line" pauses
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=3)

# --- HEADER GENERATION ---
header = ["temp", "light"]
for i in range(1, SAMPLES_PER_BATCH + 1):
    header.extend([f"acc_x{i}", f"acc_y{i}", f"acc_z{i}"])

# Create CSV
with open(CSV_FILENAME, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(header)

print(f"Connected to {SERIAL_PORT}. buffering data...")

# --- THE SMART BUFFER LOOP ---
buffer_string = ""

try:
    while True:
        if ser.in_waiting > 0:
            # 1. Read whatever is available as a raw chunk
            chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            
            # 2. Add it to our "holding area"
            buffer_string += chunk
            
            # 3. Check if we have a full line (look for newline character)
            if '\n' in buffer_string:
                # Split the buffer into potential multiple lines
                lines = buffer_string.split('\n')
                
                # The last part might be an incomplete line for the NEXT batch, 
                # so we save it back to buffer_string
                buffer_string = lines.pop()
                
                # Process all complete lines found
                for line in lines:
                    line = line.strip()
                    if not line: continue
                    
                    # Skip status messages
                    if "LoRa" in line or "Init" in line: continue

                    values = line.split(',')

                    # 4. SAVE TO CSV
                    if len(values) == len(header):
                        with open(CSV_FILENAME, "a", newline="") as f:
                            writer = csv.writer(f)
                            writer.writerow(values)
                            sample_logged += 1
                        print(f"[{sample_logged}]Saved Batch! Temp: {values[0]}, Light: {values[1]}") 
                    else:
                        # If mismatch, it usually means we are syncing up. Ignore it.
                        pass

        # Small sleep to save CPU usage
        time.sleep(0.01)

except KeyboardInterrupt:
    print("\nLogging stopped.")
    ser.close()