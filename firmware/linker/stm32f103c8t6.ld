/*
 *  STM32F103C8T6 链接脚本
 *  Flash: 64 KB @ 0x08000000
 *  RAM  : 20 KB @ 0x20000000
 *  （注意：C8T6 实际出厂可能带 128KB Flash，这里按官方标称 64KB 约束，保证兼容）
 */
ENTRY(Reset_Handler)

_Min_Heap_Size  = 0x200;   /* 512 B */
_Min_Stack_Size = 0x400;   /* 1 KB  */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 栈顶地址：RAM 末尾 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    /* 中断向量表必须放在 Flash 起始处 */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* 程序代码与常量 */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* 只读数据 */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* C++ 构造/析构（本工程未用，保留兼容性） */
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    .preinit_array : { KEEP(*(.preinit_array*)) } >FLASH
    .init_array : { KEEP(*(.init_array*)) } >FLASH
    .fini_array : { KEEP(*(.fini_array*)) } >FLASH

    /* 已初始化全局变量：运行时从 Flash 拷贝到 RAM */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT>FLASH

    /* 未初始化全局变量 */
    . = ALIGN(4);
    .bss :
    {
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    /* 检查 RAM 是否够用，留堆空间 */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /* 注意：这里不要 DISCARD libm.a/libc.a，否则姿态解算用到的 sqrtf/atan2f
       会被整段丢弃导致链接失败。未用到的库代码由 --gc-sections 自动裁剪。 */

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
