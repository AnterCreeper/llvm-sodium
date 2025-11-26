ENTRY (_start)
SECTIONS
{
  . = 0x0000;
  .init : {
    . = ALIGN(4);
    KEEP(*(SORT_NONE(.init)))
    . = ALIGN(4);
  }
  .vector :
  {
    *(.vector);
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
