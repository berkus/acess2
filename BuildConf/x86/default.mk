
MODULES += Storage/ATA
MODULES += Storage/FDDv2
MODULES += Network/NE2000 Network/RTL8139
MODULES += Network/VIARhineII
MODULES += Display/VESA
MODULES += Display/BochsGA
#MODULES += Display/VIAVideo
MODULES += Input/PS2KbMouse
MODULES += x86/ISADMA x86/VGAText

MODULES += USB/Core USB/UHCI
#USB/OHCI
MODULES += USB/HID USB/MSC
#MODULES += Interfaces/UDI

DYNMODS += Filesystems/InitRD
