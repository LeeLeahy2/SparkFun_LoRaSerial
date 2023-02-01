#!/bin/bash
######################################################################
# Copyright 2022 Lee Leahy (lpleahyjr@gmail.com)
# All rights reserved
#
# SprinklerServer.sh
#
# Start the sprinkler server and weather station monitor service
# This script needs to be run as root
#
# Use the following steps to install the SprinklerServer service:
#
#    cd ~/SparkFun/LoRaSerial/Firmware/Tools
#    ls -al
#
#    # Stop the previous service
#    sudo systemctl stop SprinklerServer
#    sudo systemctl disable SprinklerServer
#
#    # Remove the old images and scripts
#
#    # Get the new images and scripts
#
#    # Make the image and scripts executable
#    chmod +x SprinklerServer*
#    ls -al
#
#    # Start the service
#    sudo cp SprinklerServer.service /lib/systemd/system/
#    sudo systemctl enable SprinklerServer
#    sudo systemctl start SprinklerServer
#    sudo systemctl status SprinklerServer
#
#    # Reboot the system to verify that the service starts upon reboot
#    sudo reboot
#
######################################################################

cd /home/lee/SparkFun/LoRaSerial/Firmware/Tools

# Loop forever running the measurement program
while [ 1 -ne 0 ]; do

    # Run the program
    ./SprinklerServer   /dev/ttyACM0  web_data  No-Rem0te_Acce\$s  Makakilo_Hale_2  -2
done
