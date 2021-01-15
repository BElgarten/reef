#include <stddef.h>
#include "uefi.h"
#define PAGESIZE 4096

EFI_SYSTEM_TABLE *system_table;
EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
EFI_HANDLE image_handle;

EFI_STATUS print(CHAR16 *str) {
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out;
	out = system_table->ConOut;
	return out->OutputString(out, str);
}

EFI_STATUS putchar(CHAR16 c) {
	CHAR16 buf[2];
	buf[0] = c;
	buf[1] = 0;
	return print(buf);
}

EFI_STATUS print_number(INT64 n) {
	CHAR16 buf[41];
	CHAR16 *bufend;

	if (n < 0) {
		putchar('-');
		n = -n;
	}

	bufend = buf + 40;
	*bufend = 0;

	do {
		bufend--;
		*bufend = '0' + n % 10;
		n = n / 10;
	} while (n > 0);

	return print(bufend);
}

CHAR16 *hexdigits = L"0123456789abcdef";
EFI_STATUS print_hex(UINT64 n) {
	CHAR16 buf[17];
	unsigned i;

	buf[16] = 0;
	for (i = 0; i < 16; i++)
		buf[i] = hexdigits[(n >> (60 - 4 * i)) & 0xf];
	return print(buf);
}

EFI_STATUS log(CHAR16 *str) {
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *err;
	err = system_table->StdErr;
	return err->OutputString(err, str);
}

void hang(void) {
	for (;;);
}

const CHAR16 *const efi_error_strings[] = {
	L"Success",
	L"Imaged Failed to Load",
	L"Incorrect Parameter",
	L"Unsupported Operation",
	L"Improper buffer size for operation",
	L"Buffer too small",
	L"No data pending",
	L"Device Error",
	L"Device cannot be written to",
	L"Out of Resources",
	L"The file system was corrupted",
	L"File System Full",
	L"No Media",
	L"The media has changed since last access",
	L"The item was not found",
	L"Access denied",
	L"Server not found or did not respond",
	L"Device has no mapping",
	L"The device has timed out",
	L"The protocol has not started",
	L"The protocol has already started",
	L"The operation was aborted",
	L"An ICMP error occured during the network operation",
	L"A TFTP error occured during the network operation",
	L"A protocol error occured during the network operation",
	L"The function's internal version is incompatable with the requested version",
	L"The function was not performed due to a security violation",
	L"CRC error detected",
	L"Beginning or end of media reached",
	L"Invalid Error",
	L"Invalid Error",
	L"End of file reached",
	L"Invalid languaged specified",
	L"Security status of data compromised",
	L"Address conflict address allocation",
	L"An HTTP error occured during the network operation"
};

void check_error_and_panic(EFI_STATUS code,  CHAR16 *msg) {
	const CHAR16 *errstr;

	if (!(code & EFI_ERROR_BIT_MASK))
		return;
	code ^= EFI_ERROR_BIT_MASK;
	if (code > 35)
		errstr = L"";
	else
		errstr = efi_error_strings[code];
	log(msg);
	log(L": ");
	log((CHAR16 *)errstr);
	print(msg);
	print(L": ");
	print((CHAR16 *)errstr);
	hang();
}

struct memory_map {
	UINTN npages_allocated;
	UINTN map_size;
	UINTN map_key;
	UINTN descriptor_size;
	UINT32 descriptor_version;
	EFI_MEMORY_DESCRIPTOR map[1];
};

struct memory_map *get_memory_map(void) {
	EFI_STATUS e;
	UINTN npages;
	UINTN allocation_size;
	UINTN map_size = 0;
	struct memory_map *map;

	/* get map size */
	e = system_table->BootServices->GetMemoryMap(&map_size, NULL, NULL, NULL, NULL);
	if (e != EFI_BUFFER_TOO_SMALL)
		check_error_and_panic(e, L"GetMemoryMap() getting size");

	/* This allocation will cause memory fragmentation, meaning the map */
	/* may be larger following this allocation. Therefore, we pad the size */
	/* of the allocation by 128 bytes. This specific number is arbitrary. */
	map_size += 0x80;

	allocation_size = map_size + sizeof(struct memory_map);
	npages = (allocation_size + PAGESIZE - 1) / PAGESIZE;

	e = system_table->BootServices->AllocatePages(
		AllocateAnyPages, EfiLoaderData, npages,
		(EFI_PHYSICAL_ADDRESS *) &map);
	check_error_and_panic(e, L"AllocatePages() for Memory Map");

	map->npages_allocated = npages;
	map->map_size = map_size;
	e = system_table->BootServices->GetMemoryMap(&map->map_size, &map->map[0],
		&map->map_key, &map->descriptor_size, &map->descriptor_version);
	check_error_and_panic(e, L"GetMemoryMap() getting map");


	return map;
}

CHAR16 *memory_type_strings[] = {
	L"Reserved",
	L"Loader Code",
	L"Loader Data",
	L"Boot Services Code",
	L"Boot Services Data",
	L"Runtime Services Code",
	L"Runtime Services Data",
	L"Conventional",
	L"Unusable",
	L"ACPI Reclaimable",
	L"ACPI NVS",
	L"MMIO",
	L"MMIO Port Space",
	L"Pal Code",
	L"Persistent"
};

void print_memory_map(struct memory_map *map) {
	size_t i;
	EFI_MEMORY_DESCRIPTOR *desc, *max;

	print(L"Map Size: ");
	print_number(map->map_size);

	print(L"\r\nDescriptor Size: ");
	print_number(map->descriptor_size);
	print(L"\r\nDescriptor Count: ");
	print_number(map->map_size / map->descriptor_size);

	print(L"\r\nDescriptor Version: ");
	print_number(map->descriptor_version);
	print(L"\r\nMap Key: 0x");
	print_hex(map->map_key);
	print(L"\r\n");


	print(L"Base             | Number of Bytes  | Type\r\n");
	max = (VOID *) map->map + map->map_size;
	for (desc = map->map; desc < max; desc = (VOID *) desc + map->descriptor_size) {
		print_hex(desc->PhysicalStart);
		print(L" | ");
		print_hex(desc->NumberOfPages * PAGESIZE);
		print(L" | ");
		print(memory_type_strings[desc->Type]);
		print(L"\r\n");
	}
}

CHAR16 *pixel_format_strings[] = {
	L"RGBX 8 Bit/Color",
	L"BGRX 8 Bit/Color",
	L"Bit Mask",
	L"Blt Only"
};
void set_video_mode(void) {
	EFI_STATUS e;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
	UINTN info_size;
	EFI_GUID gop_guid;
	UINT32 max_mode;
	size_t best_mode;
	uint64_t max_area, area;
	size_t i;

	print(L"Setting Video Mode...\r\n");

	gop_guid = (EFI_GUID) EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	e = system_table->BootServices->LocateProtocol(&gop_guid, NULL, (VOID **) &gop);
	check_error_and_panic(e, L"LocateProtocol(GOP)");
	print(L"    GOP acquired\r\n");

	max_mode = gop->Mode->MaxMode;
	print(L"    Number of Avaliable Modes: ");
	print_number(max_mode);
	print(L"\r\n");

	best_mode = max_mode;
	max_area = 0;
	for (i = 0; i < max_mode; i++) {
		e = gop->QueryMode(gop, i, &info_size, &info);
		check_error_and_panic(e, L"gop->QueryMode()");

		/* disregard non color modes */
		if (info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor &&
			info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) {
			continue;
		}

		area = (uint64_t) info->HorizontalResolution * info->VerticalResolution;
		if (area > max_area) {
			max_area = area;
			best_mode = i;
		}
	}

	if (best_mode == max_mode) {
		print(L"No Suitable Mode Detected");
		log(L"No Suitable Mode Detected");
		hang();
	}

	e = gop->QueryMode(gop, best_mode, &info_size, &info);
	check_error_and_panic(e, L"gop->QueryMode(best_mode)");

	print(L"    Mode Found\r\n");
	print(L"        Mode Number: ");
	print_number(best_mode);
	print(L"\r\n        Version: ");
	print_number(info->Version);
	print(L"\r\n        Resolution: ");
	print_number(info->HorizontalResolution);
	print(L" x ");
	print_number(info->VerticalResolution);
	print(L"\r\n        Pixel Format: ");
	print(pixel_format_strings[info->PixelFormat]);
	print(L"\r\n        Pitch: ");
	print_number(info->PixelsPerScanLine);
	print(L"\r\n");

	e = gop->SetMode(gop, best_mode);
	check_error_and_panic(e, L"gop->SetMode()");
}

static inline uint32_t create_color(uint8_t r, uint8_t g, uint8_t b) {
	/* uefi is strictly little endian so this is portable */
	uint32_t wr = r, wg = g, wb = b;
	if (gop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
		wr = b;
		wb = r;
	}
	return b << 16 | g << 8 | r;
}

void draw_test_image(void) {
	UINT32 *framebuffer;
	size_t x, y;
	size_t width, height;
	size_t pitch;
	size_t idx;
	uint32_t color;

	width = gop->Mode->Info->HorizontalResolution;
	height = gop->Mode->Info->VerticalResolution;
	pitch = gop->Mode->Info->PixelsPerScanLine;

	framebuffer = (UINT32 *) gop->Mode->FrameBufferBase;

	color = create_color(255, 0, 255);

	print(L"Drawing Test Image\r\n");
	print(L"    Framebuffer: 0x");
	print_hex((uintptr_t) framebuffer);
	print(L"\r\n");
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			idx = y * pitch + x;
			framebuffer[idx] = color;
		}
	}
}

void kentry(EFI_HANDLE handle, EFI_SYSTEM_TABLE *st) {
	image_handle = handle;
	system_table = st;
	struct memory_map *memmap;

	st->ConOut->ClearScreen(st->ConOut);
	print(L"Booted!\r\n");

	set_video_mode();
	draw_test_image();
	print(L"Detecting Memory...\r\n");

	memmap = get_memory_map();
	print_memory_map(memmap);
	system_table->BootServices->Stall(5000000);
	system_table->RuntimeServices->ResetSystem(EfiResetShutdown, 0, 0, NULL);
	for (;;);
}
