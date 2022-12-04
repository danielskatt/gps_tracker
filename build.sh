#nRF9160 SiP
west build -b thingy91_nrf9160_ns -p -- -DOVERLAY_CONFIG=./project.conf

# RF52840 SoC
# west build -b thingy91_nrf52840

# Flash board
# west flash