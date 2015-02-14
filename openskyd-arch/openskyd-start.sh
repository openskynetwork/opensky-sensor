#!/bin/sh

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
    echo BB-W-Radarcape > ${SLOTS}
fi

cat $SLOTS

exec /usr/bin/openskyd

