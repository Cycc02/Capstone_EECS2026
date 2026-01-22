compile:
	gcc lora_debug.c -o debug -lpigpio -lm

run:
	nodejs main_ctrl.js

killall:
	sudo killall pigpiod
	sudo killall debug
	sudo rm /var/run/pigpio.pid
