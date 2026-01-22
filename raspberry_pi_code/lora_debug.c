#include <stdio.h>
#include <stdlib.h>
#include <pigpio.h>
#include <unistd.h>
#include <string.h> 
#include "LoRa.c"   

// --- GLOBALS ---
int spid = -1;
LoRa_ctl modem; 

// --- HARDWARE CONFIG ---
#define LORA_DIO0_GPIO 25 
#define LORA_RESET_GPIO 22 

// --- RESET FUNCTION (Crucial: Was present in the working code) ---
void LoRa_reset() {
    gpioSetMode(LORA_RESET_GPIO, PI_OUTPUT);
    gpioWrite(LORA_RESET_GPIO, 0);
    usleep(100);
    gpioWrite(LORA_RESET_GPIO, 1);
    usleep(10000);
}

// --- MANUAL CONFIG (Exact copy from working Diagnostic) ---
void LoRa_manual_config() {
    // 1. Sleep
    lora_reg_write_byte(spid, 0x01, 0x80); 
    usleep(10000);
    
    // 2. Frequency 433MHz (0x6C4000)
    lora_reg_write_byte(spid, 0x06, 0x6C); 
    lora_reg_write_byte(spid, 0x07, 0x40); 
    lora_reg_write_byte(spid, 0x08, 0x00); 

    // 3. Config (SF7, BW125, Explicit Header)
    lora_reg_write_byte(spid, 0x1D, 0x72); 
    lora_reg_write_byte(spid, 0x1E, 0x74); 

    // 4. Sync Word
    lora_reg_write_byte(spid, 0x39, 0x12);

    // 5. RX Continuous Mode
    lora_reg_write_byte(spid, 0x01, 0x8D); 
}

int main() {
    setbuf(stdout, NULL); // Disable buffering for Node.js

    gpioCfgInterfaces(PI_DISABLE_SOCK_IF | PI_DISABLE_FIFO_IF);
    if (gpioInitialise() < 0) return 1;

    // 1. OPEN SPI
    spid = spiOpen(0, 500000, 0); 
    if (spid < 0) return 1;

    // 2. HARD RESET (This might be why the other one crashed!)
    LoRa_reset();
    
    unsigned char version[1];
    lora_reg_read_bytes(spid, 0x42, version, 1); // Read Register 0x42 (Version)
    printf("Radio Version: 0x%02X\n", version[0]);
    
    if (version[0] != 0x12) {
        printf("ERROR: Radio not found! Check wiring.\n");
        //return 1;
    }

    // 3. CONFIGURE
    LoRa_manual_config();
    
    // 4. MAIN LOOP (Polling)
    while(1) {
        // A. Check Flags
        unsigned char irq_flags[1];
        lora_reg_read_bytes(spid, 0x12, irq_flags, 1);
        int flags = irq_flags[0];

        // B. Packet Received? (0x40)
        if ((flags & 0x40) == 0x40) {
            
            // CRC OK? (No 0x20 flag)
            if ((flags & 0x20) == 0) {
                
                // Get Length
                unsigned char len_buf[1];
                lora_reg_read_bytes(spid, 0x13, len_buf, 1);
                int bytes = len_buf[0];

                // Reset FIFO
                unsigned char ptr_buf[1];
                lora_reg_read_bytes(spid, 0x10, ptr_buf, 1);
                lora_reg_write_byte(spid, 0x0D, ptr_buf[0]);

                if (bytes > 0) {
                    // Use LOCAL buffer (Safer than struct pointer)
                    char buffer[256];
                    lora_reg_read_bytes(spid, 0x00, buffer, bytes);
                    
                    if(bytes < 256) buffer[bytes] = '\0';
		    else buffer[255] = '\0';
                    
		    char *token = strtok(buffer, ",");
		    char *result_str = strtok(NULL,",");
		    char *voltage_str = strtok(NULL,",");

		    if(token != NULL && result_str != NULL && voltage_str != NULL){
			int result = atoi(result_str); //Convert String to Int
			float voltage = atof(voltage_str); //Convert String to Float
		
                    	// PRINT TO NODE.JS
                    	printf("%s\n", token);
                    	printf("%d\n", result);
		    	printf("%.2f\n", voltage);
                    	fflush(stdout);
		    } 
                }
            }
            // Clear Flags
            lora_reg_write_byte(spid, 0x12, 0xFF); 
        }

        usleep(10000); // 10ms sleep
    }

    spiClose(spid);
    gpioTerminate();
    return 0;
}
