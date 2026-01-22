#include <stdio.h>
#include <pigpio.h>
#include <string.h>

int main() {
    if (gpioInitialise() < 0) {
        printf("GPIO Init Failed\n");
        return 1;
    }

    // Open SPI Channel 0 (CE0)
    int h = spiOpen(0, 500000, 0);
    if (h < 0) {
        printf("SPI Open Failed (Is SPI enabled in raspi-config?)\n");
        return 1;
    }

    char tx[] = "Hello World";
    char rx[12];

    // Send and Receive at the same time
    spiXfer(h, tx, rx, sizeof(tx));

    printf("Sent: %s\n", tx);
    printf("Received: %s\n", rx);

    if (strcmp(tx, rx) == 0) {
        printf("SUCCESS: SPI is working!\n");
    } else {
        printf("FAILURE: Received garbage or nothing.\n");
    }

    spiClose(h);
    gpioTerminate();
    return 0;
}
