#include "serial2_link.h"

#include "wiring_private.h"  // pinPeripheral

Uart Serial2(&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);

void SERCOM1_Handler() {
  Serial2.IrqHandler();
}

void serial2Begin(unsigned long baud) {
  Serial2.begin(baud);
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);
}
