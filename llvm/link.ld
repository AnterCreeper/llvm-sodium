ENTRY (_start)
SECTIONS
{
  . = 0x0000;
  .vector :
  {
    KEEP(*(SORT_NONE(.vector)))
	. = ALIGN(16);
  }
  .text : {
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata*)
  }
  . = 0x8000;
  .stack : {
    PROVIDE (_stack = .);
  }
  .data : {*(.data)}
  .bss : {*(.bss)}
}
