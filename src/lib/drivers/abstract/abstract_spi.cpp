#include "abstract_spi.h"

AbstractSPI::AbstractSPI(SPIInterface_t *interface, IOPinHandle_t *csPin):_interface(interface),_csPin(csPin){}

SPIResponse_t AbstractSPI::writeBytes(uint8_t * txBytes, size_t txLen, uint32_t timeout, bool nonblocking) {
    SPIResponse_t rval = SPI_OK;
    if (txLen && txBytes != nullptr) {
        if(nonblocking){
            rval = spiTxNonblocking(_interface, _csPin, txLen, txBytes, timeout);
        } else {
            rval = spiTx(_interface, _csPin, txLen, txBytes, timeout);
        }
    }
    return rval;
}

SPIResponse_t AbstractSPI::readBytes(uint8_t * rxBuff, size_t rxLen, uint32_t timeout, bool nonblocking) {
    SPIResponse_t rval = SPI_OK;
    if (rxLen && (rxBuff != nullptr)) {
        if(nonblocking){
            rval = spiRxNonblocking(_interface, _csPin, rxLen, rxBuff, timeout);
        }  else {
            rval = spiRx(_interface, _csPin, rxLen, rxBuff, timeout);
        }
    }
    return rval;
}

SPIResponse_t AbstractSPI::writeReadBytes(uint8_t * rxBuff, size_t len, uint8_t * txBytes, uint32_t timeout, bool nonblocking) {
    SPIResponse_t rval = SPI_OK;
    if (len && (rxBuff != nullptr) && txBytes != nullptr) {
        if(nonblocking){
            rval = spiTxRxNonblocking(_interface, _csPin, len, txBytes, rxBuff, timeout);
        } else {
            rval = spiTxRx(_interface, _csPin, len, txBytes, rxBuff, timeout);
        }
    }
    return rval;
}
