# List of the ChibiOS/RT ARMv8-M-ML-ALT RP2 SMP port files.
PORTSRC = $(CHIBIOS)/os/common/ports/ARMv8-M-ML-ALT/chcore.c \
          $(CHIBIOS)/os/common/ports/ARMv8-M-ML-ALT/smp/rp2/chcoresmp.c

PORTASM = $(CHIBIOS)/os/common/ports/ARMv8-M-ML-ALT/compilers/GCC/chcoreasm.S

PORTINC = $(CHIBIOS)/os/common/portability/GCC \
          $(CHIBIOS)/os/common/ports/ARM-common \
          $(CHIBIOS)/os/common/ports/ARMv8-M-ML-ALT \
          $(CHIBIOS)/os/common/ports/ARMv8-M-ML-ALT/smp/rp2

# Shared variables
ALLXASMSRC += $(PORTASM)
ALLCSRC    += $(PORTSRC)
ALLINC     += $(PORTINC)
