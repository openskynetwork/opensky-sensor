#!/bin/sh

KERNEL=$(uname -r)
KMAJOR=${KERNEL%%.*}

if [ ${KMAJOR} -eq 4 ]; then
  COMPAT=/sys/firmware/devicetree/base/compatible
  if grep "bone-black" ${COMPAT} >/dev/null 2>&1; then
    PARAM="--black"
  elif grep -v "bone" ${COMPAT} >/dev/null 2>&1; then
    echo "Unkown base board"
    exit 1
  else
    PARAM=""
  fi
else
  BOARD=$(dmesg | grep compatible-baseboard | cut -d',' -f2)
  case ${BOARD} in
    beaglebone-black) OVERLAY=B; PARAM="--black";;
    beaglebone) OVERLAY=W; PARAM="";;
    *) echo "Non supported board ${BOARD}"
       exit 1;;
  esac

  SLOTS=$(find /sys/devices -name slots)
  UART2=$(grep "BB-UART2" ${SLOTS} | wc -l)
  UART5=$(grep "BB-UART5" ${SLOTS} | wc -l)
  CAPE=$(grep "BB-${OVERLAY}-Radarcape" ${SLOTS} | wc -l)

  if [ ${UART2} -eq 0 ]; then
    echo BB-UART2 > ${SLOTS}
  fi

  if [ ${UART5} -eq 0 ]; then
    echo BB-UART5 > ${SLOTS}
  fi

  if [ ${CAPE} -eq 0 ]; then
    echo BB-${OVERLAY}-Radarcape > ${SLOTS}
  fi
fi

exec /usr/bin/openskyd ${PARAM}

