/* Stage 0: boot2 at 0x10000000, the vector table at 0x10000100 where boot2
   enters, then code and constants. Must fit flash sector 0 (FW_STAGE0_SIZE)
   and use no RAM sections: there is no start-up code to initialise them. */

MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 4k
    RAM(rwx)  : ORIGIN = 0x20000000, LENGTH = 264k
}

ENTRY(stage0_main)

SECTIONS
{
    .boot2 : {
        __boot2_start__ = .;
        KEEP (*(.boot2))
        __boot2_end__ = .;
    } > FLASH
    ASSERT(__boot2_end__ - __boot2_start__ == 256, "boot2 must be 256 bytes")

    .text : {
        __vectors_start__ = .;
        KEEP (*(.vectors))
        *(.text*)
        *(.rodata*)
    } > FLASH
    ASSERT(__vectors_start__ == 0x10000100, "vectors must sit at 0x10000100")

    .ram : {
        *(.data*)
        *(.bss*)
        *(COMMON)
    } > RAM
    ASSERT(SIZEOF(.ram) == 0, "stage 0 has no start-up code: no .data or .bss")

    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.ARM.attributes) }
}
