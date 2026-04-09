# FIFO Coder

This folder contains the infamous modbus FIFO. The code was split into common and decoder and encoder modules, all available as separate libraries, so the FW can only include whatever they need. Also both `fifo_encoder` and `fifo_decoder` modules require the extra inclusion of logging library, so every FW can define their own.
