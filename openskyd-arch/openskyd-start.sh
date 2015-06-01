#!/bin/sh

BOARD=$(dmesg | grep compatible-baseboard | cut -d',' -f2)
SLOTS=$(find /sys/devices -name slots)

BBUART5STATE=`grep BB-UART5 $SLOTS | wc -l`
BBUART2STATE=`grep BB-UART2 $SLOTS | wc -l`
BBRadarcapeSTATE=`grep BB-.-Radarcape $SLOTS | wc -l`

if [ $BBUART5STATE -eq 0 ]; then 
  echo BB-UART5 > ${SLOTS}
fi

if [ $BBUART2STATE == 0 ]; then
  echo BB-UART2 > ${SLOTS}
fi

if [ $BBRadarcapeSTATE == 0 ]; then
  if [ $BOARD == beaglebone ]; then
    echo BB-W-Radarcape > ${SLOTS}

  elif [ $BOARD == beaglebone-black ]; then
    echo BB-B-Radarcape > ${SLOTS}

  else
    echo "Non supported board $BOARD"
    exit 1
  fi
  
  cat $SLOTS
fi

if [ $BOARD == beaglebone-black ]; then
  arg=--black
else
  arg=
fi

exec /usr/bin/openskyd $arg

