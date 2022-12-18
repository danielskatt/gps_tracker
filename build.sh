#nRF9160 SiP
west build -b thingy91_nrf9160_ns -p -- -DOVERLAY_CONFIG=./project.conf

# Flash board
# west flash