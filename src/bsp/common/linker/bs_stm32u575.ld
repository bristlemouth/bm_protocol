/*
******************************************************************************
**
**  File        : LinkerScript.ld
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32U575xI Device from STM32U5 series
**                      2048Kbytes FLASH
**                      784Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2022 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
*****************************************************************************
*/

#ifndef FLASH_START
#error FLASH_START not defined!
#endif

#ifndef FLASH__
#error FLASH__ not defined!
#endif

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);	/* end of "RAM" Ram type memory */

_Min_Stack_Size = 0x400 ;	/* required amount of stack */

/* Generate a link error if heap and stack don't fit into RAM */
_Stack_Guard_Size = 0x400;      /* required amount of no-write zone!  */
_Min_Stack_Size = 0x400; /* required amount of stack */

_ram_start = 0x20000000;
_ram_size = 768K;
_noinit_size = 512;

/* Highest address of the user mode stack */
_estack = 0x20050000 - _noinit_size;    /* end of RAM minus a bit */

/* TODO - change this for 1MB parts*/
_flash_size = 2048K;
_memfault_coredump_size = _ram_size + 4K;

/* Memories definition */
MEMORY
{
  RAM	(xrw)	: ORIGIN = _ram_start,	LENGTH = _ram_size - _noinit_size
  SRAM4	(xrw)	: ORIGIN = 0x28000000,	LENGTH = 16K
  NOINIT (xrw)        : ORIGIN = _ram_start + _ram_size - _noinit_size, LENGTH = _noinit_size
  BOOT_FLASH (rx)     : ORIGIN = FLASH_START, LENGTH = BOOTLOADER_SIZE
  APP_FLASH (rx)      : ORIGIN = FLASH_START + BOOTLOADER_SIZE, LENGTH = APP_SIZE
  COREDUMP_FLASH (rw) : ORIGIN = (FLASH_START + _flash_size - _memfault_coredump_size), LENGTH = _memfault_coredump_size
}
__MemfaultCoreStorageStart = ORIGIN(COREDUMP_FLASH);
__MemfaultCoreStorageEnd = ORIGIN(COREDUMP_FLASH) + LENGTH(COREDUMP_FLASH);

/* Sections */
SECTIONS
{

#if MCUBOOT_HEADER_SIZE > 0
/* Take into account MCUBoot signature, which comes before the isr vector */
  .mcuboot_sig :
  {
    . = ALIGN(8);
    . = . + MCUBOOT_HEADER_SIZE;
    . = ALIGN(8);
  } >FLASH__
#endif

  /* The startup code into "FLASH" Rom type memory */
  .isr_vector :
  {
    /* STM32U575 HAS 141 interrupt vectors (16 + 125) = 141 so we need to align to the next power
        of 2 for the vector table to work */
    . = ALIGN(256);
    _vtable = .;
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(8);
  } >FLASH__

  .note.sofar.version :
  {
   __start_sofar_version_start = .;
   KEEP(*(.note.sofar.version))
  } > FLASH__

  /* The program code and other data into "FLASH" Rom type memory */
  .text :
  {
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    _etext = .;        /* define a global symbols at end of code */
  } >FLASH__

  /* Constant data into "FLASH" Rom type memory */
  .rodata :
  {
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
  } >FLASH__

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH__

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH__

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH__

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH__

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH__

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections into "RAM" Ram type memory */
  .data :
  {
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> FLASH__

  /* Uninitialized data section into "RAM" Ram type memory */
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    *(.bss)
    *(.bss*)
    *(COMMON)

    _ebss = .;         /* define a global symbol at bss end */
  } >RAM

  /* User_heap_stack section, used to check that there is enough "RAM" Ram type memory left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
  .note.gnu.build-id :
  {
   __start_gnu_build_id_start = .;
   KEEP(*(.note.gnu.build-id))
  } > FLASH__

  .mflt_coredump_symbols :
  {
    __MfltCoredumpRamStart = ORIGIN(RAM);
    __MfltCoredumpRamEnd = ORIGIN(RAM) + LENGTH(RAM);
  } > FLASH__

  .noinit (NOLOAD):
  {
    /* place all symbols in input sections that start with .noinit */
    KEEP(*(*.noinit*))
  } > NOINIT
}
