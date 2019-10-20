


## PROGRAMMING NRF52 stlink
openocd -f interface/stlink.cfg -f target/nrf52.cfg

telnet localhost 4444
 > reset 
 > program zephyr.hex verify 