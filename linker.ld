/* linker.ld */
ENTRY(_start)

SECTIONS
{
	/* המערכת נטענת ב-1MB */
	. = 1M;

	.text BLOCK(4K) : ALIGN(4K)
	{
		/* ה-KEEP מבטיח שההדר לא יימחק, והמיקום מבטיח שהוא ראשון */
		KEEP(*(.multiboot))
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}
}