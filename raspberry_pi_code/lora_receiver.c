#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pigpio.h>
#include "LoRa.c"

#define LORA_DIO0_GPIO 25

LoRa_ctl modem;

int spid = -1;
char temp_buf[1];

void LoRa_safe_start(LoRa_ctl *modem) {
    // 1. Sleep Mode (Required to change settings)
    lora_reg_write_byte(spid, 0x01, 0x80); 
    usleep(10000);
    
    lora_reg_write_byte(spid, 0x06, 0x6C); 
    lora_reg_write_byte(spid, 0x07, 0x40); 
    lora_reg_write_byte(spid, 0x08, 0x00);
    
    lora_reg_write_byte(spid, 0x1D, 0x72); 

    // 4. Configure SF & CRC
    // SF=7 (0x70) | CRC=On (0x04) => 0x74
    lora_reg_write_byte(spid, 0x1E, 0x74); 

    // 5. Sync Word
    lora_reg_write_byte(spid, 0x39, modem->eth.syncWord);

    // 6. Wake up into Receive Mode
    lora_reg_write_byte(spid, 0x01, 0x8D);
}

void handle_packet(){
		
	unsigned char irq_flags[1];
	lora_reg_read_bytes(spid, 0x12, irq_flags, 1);

	if((irq_flags[0] & 0x20) == 0){

		//Get Packet Length
		unsigned char len_buf[1];
		lora_reg_read_bytes(spid,0x13, len_buf,1);
		int bytes = len_buf[0];

		//Set FIFO Pointer to Start of Packet
		unsigned char ptr_buf[1];
		lora_reg_read_bytes(spid, 0x10, ptr_buf, 1);
		lora_reg_write_byte(spid, 0x0D, ptr_buf[0]);

		//Read Data
		if(bytes > 0){
			lora_reg_read_bytes(spid, 0x00, modem.rx.data.buf, bytes);
			int result = modem.rx.data.buf[bytes-1];
			modem.rx.data.buf[bytes-1] = '\0';
			printf("%s\n",modem.rx.data.buf);
			printf("%d\n",result);
			
			fflush(stdout);	
		}
	}
	
	//Clear Interrupt Flags
	lora_reg_write_byte(spid, 0x12, 0xFF);

	//Reset Mode to Continouous Receive
	LoRa_receive(&modem);
}

	
int main() {
	setbuf(stdout, NULL);

	if(gpioInitialise() < 0) return 1;

	spid = spiOpen(0, 500000, 0);
	if(spid < 0) {
		printf("SPI Open Failed\n");
		return 1;
	}

	modem.eth.sf = 7;
	modem.eth.bw = 125000;
	modem.eth.ecr = 5;	
	modem.eth.freq = 433000000;
	modem.eth.syncWord = 0x12;

	LoRa_safe_start(&modem);

	while(1){
		handle_packet();
		usleep(10000);
	}
	spiClose(spid);
	gpioTerminate();
	return 0;
}
