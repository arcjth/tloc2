stty -F /dev/ttyUSB0 115200 raw
echo "min,max,ch0,ch1,ch2,ch3" > saida.csv
timeout 10s cat /dev/ttyUSB0 >> saida.csv
