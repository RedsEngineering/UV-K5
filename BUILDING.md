
# Building Manually Using GNU Make


Tested on:

- Ubuntu 22.04
- Ubuntu 24.04
- Debian 12

Packages required:

- build-essential
- gcc-arm-none-eabi
- python3-crcmod


## Building

to build, simply run

    make <features>

e.g.

    make ENABLE_BOOT_BEEPS=1 ENABLE_TX1750=1 ENABLE_SCAN_RANGES=0

the resulting `firmware.packed.bin` can then be flashed


## Troubleshooting

```
/usr/lib/gcc/arm-none-eabi/12.2.1/../../../arm-none-eabi/bin/ld: firmware section `.text' will not fit in region `FLASH'
/usr/lib/gcc/arm-none-eabi/12.2.1/../../../arm-none-eabi/bin/ld: region `FLASH' overflowed by 736 bytes
collect2: error: ld returned 1 exit status
make: *** [Makefile:441: firmware] Error 1
```

... or something similar  
means the generated binary is too big.

To remediate this, you can try removing some features.

Also note that GCC 12 (on Debian 12) generates noticably larger binary compared to the GCC 10 (on Ubuntu 22.04).
GCC 13 (on Ubuntu 24.04) managed to generate smaller binary, although not as small.

