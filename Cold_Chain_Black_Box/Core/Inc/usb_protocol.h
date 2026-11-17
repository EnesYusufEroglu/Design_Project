#ifndef INC_USB_PROTOCOL_H_
#define INC_USB_PROTOCOL_H_

#include <stdint.h>

void USB_Proto_OnReceive(uint8_t *buf, uint32_t len);
void USB_Proto_Process(void);

#endif /* INC_USB_PROTOCOL_H_ */
