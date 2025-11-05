# openprinttag_nfcv

This module provides a NFCV backend for `openprinttag::OPTReader`. It translates the reader requests into NFCV commands and handles lower level tag management.
The backend is still on a higher abstraction layer - it requires `nfcv::ReaderWriterInterface` that is responsible for communicating with the NFC chip.

[See README](../openprinttag/README.md) in the `openprinttag` module for more information.
