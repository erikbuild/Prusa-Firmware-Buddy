# STM32C0 Guidelines
Ensure OpenOCD does support STM32C092 devices - use specific OpenOCD https://github.com/ivous/OpenOCD/tree/C092

[Official OpenOCD](https://github.com/openocd-org/openocd) nor [STMicroelectronics](https://github.com/STMicroelectronics/OpenOCD) versions does not support C092 up to date

Once official version of OpenOCD will support it, these readme and links to be removed.

Build instructions:


    git clone https://github.com/ivous/OpenOCD
    cd OpenOCD
    git checkout C092
    git submodule update --init --recursive
    ./bootstrap
    ./configure --enable-stlink --disable-werror
    make
    sudo make install
