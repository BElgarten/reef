#include <stddef.h>
#include "uefi.h"
#include "bootstrap.h"
#include "cpu.h"

EFI_SYSTEM_TABLE *system_table;
EFI_HANDLE image_handle;

struct bootstrap_info bootstrap_info;

static EFI_STATUS uefi_print(CHAR16 *str) {
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out;
	out = system_table->ConOut;
	return out->OutputString(out, str);
}

static EFI_STATUS uefi_log(CHAR16 *str) {
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *err;
	err = system_table->StdErr;
	return err->OutputString(err, str);
}

static void uefi_hang(void) {
	for (;;);
}

static const CHAR16 *const efi_error_strings[] = {
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
	L"The media has cuefi_hanged since last access",
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

static void check_error_and_panic(EFI_STATUS code,  CHAR16 *msg) {
	const CHAR16 *errstr;

	if (!(code & EFI_ERROR_BIT_MASK))
		return;
	code ^= EFI_ERROR_BIT_MASK;
	if (code > 35)
		errstr = L"";
	else
		errstr = efi_error_strings[code];
	uefi_log(msg);
	uefi_log(L": ");
	uefi_log((CHAR16 *)errstr);
	uefi_log(L"\r\n");
	uefi_print(msg);
	uefi_print(L": ");
	uefi_print((CHAR16 *)errstr);
	uefi_log(L"\r\n");
	uefi_hang();
}

struct memory_map {
	UINTN npages_allocated;
	UINTN map_size;
	UINTN map_key;
	UINTN descriptor_size;
	UINT32 descriptor_version;
	EFI_MEMORY_DESCRIPTOR map[1];
};

static struct memory_map *get_memory_map(void) {
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
	npages = (allocation_size + EFI_PAGESIZE - 1) / EFI_PAGESIZE;

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

static const char *memory_type_strings[] = {
	"Reserved",
	"Loader Code",
	"Loader Data",
	"Boot Services Code",
	"Boot Services Data",
	"Runtime Services Code",
	"Runtime Services Data",
	"Conventional",
	"Unusable",
	"ACPI Reclaimable",
	"ACPI NVS",
	"MMIO",
	"MMIO Port Space",
	"Pal Code",
	"Persistent"
};

static CHAR16 *pixel_format_strings[] = {
	L"RGBX 8 Bit/Color",
	L"BGRX 8 Bit/Color",
	L"Bit Mask",
	L"Blt Only"
};
static void set_video_mode(void) {
	EFI_STATUS e;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
	UINTN info_size;
	EFI_GUID gop_guid;
	UINT32 max_mode;
	size_t best_mode;
	uint64_t max_area, area;
	size_t i;

	gop_guid = (EFI_GUID) EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	e = system_table->BootServices->LocateProtocol(&gop_guid, NULL, (VOID **) &gop);
	check_error_and_panic(e, L"LocateProtocol(GOP)");

	max_mode = gop->Mode->MaxMode;

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
		uefi_print(L"No Suitable Mode Detected");
		uefi_log(L"No Suitable Mode Detected");
		uefi_hang();
	}

	e = gop->QueryMode(gop, best_mode, &info_size, &info);
	check_error_and_panic(e, L"gop->QueryMode(best_mode)");

	e = gop->SetMode(gop, best_mode);
	check_error_and_panic(e, L"gop->SetMode()");

	bootstrap_info.framebuffer.width = gop->Mode->Info->HorizontalResolution;
	bootstrap_info.framebuffer.height = gop->Mode->Info->VerticalResolution;
	bootstrap_info.framebuffer.pitch = gop->Mode->Info->PixelsPerScanLine;
	if (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
		bootstrap_info.framebuffer.format = RGBX;
	else
		bootstrap_info.framebuffer.format = BGRX;
	bootstrap_info.framebuffer.buffer = (uint32_t *) gop->Mode->FrameBufferBase;
}

static EFI_FILE_PROTOCOL *get_uefi_file(CHAR16 *path, UINT64 mode, UINT64 attributes) {
	EFI_FILE_PROTOCOL *ret;
	EFI_FILE_PROTOCOL *root;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs;
	EFI_LOADED_IMAGE_PROTOCOL *image;
	EFI_STATUS e;
	EFI_GUID sfs_guid;
	EFI_GUID image_guid;

	image_guid = (EFI_GUID) EFI_LOADED_IMAGE_PROTOCOL_GUID;
	e = system_table->BootServices->HandleProtocol(image_handle, &image_guid, (void **) &image);
	check_error_and_panic(e, L"BootServices->HandleProtocol(Loaded Image Protocol)");

	sfs_guid = (EFI_GUID) EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	e = system_table->BootServices->HandleProtocol(image->DeviceHandle, &sfs_guid, (void **) &sfs);
	check_error_and_panic(e, L"BootServices->HandleProtocol(Simple File System)");

	e = sfs->OpenVolume(sfs, &root);
	check_error_and_panic(e, L"get_uefi_file(): ESP->OpenVolume()");

	e = root->Open(root, &ret, path, mode, attributes);
	check_error_and_panic(e, L"get_uefi_file(): Open(path)");

	e = root->Close(root);
	check_error_and_panic(e, L"get_uefi_file(): root->Close()");

	return ret;
}

static int memcmp(const void *_a, const void *_b, size_t n) {
	const char *a, *b;
	a = _a;
	b = _b;
	for (; n; a++, b++, n--)
		if (*b - *a)
			return *b - *a;
	return 0;
}

static void *uefi_alloc(size_t nbytes) {
	EFI_STATUS e;
	EFI_PHYSICAL_ADDRESS ret;
	UINTN npages;

	npages = (nbytes + EFI_PAGESIZE - 1) / EFI_PAGESIZE;
	e = system_table->BootServices->AllocatePages(
		AllocateAnyPages, EfiLoaderData, npages, &ret);
	check_error_and_panic(e, L"uefi_alloc(): AllocatePages(): ");
	return (void *) ret;
	
}

static void load_console_font(void) {
	EFI_STATUS e;
	UINTN nread;
	uint8_t *bitmap;
	UINTN allocation_size;
	uint8_t magic_buffer[4];
	uint8_t psf1_magic[2] = { 0x36, 0x04 };
	uint8_t psf2_magic[4] = { 0x72, 0xb5, 0x4a, 0x86};
	EFI_FILE_PROTOCOL *file;

	file = get_uefi_file(L"\\boot\\consolefont.psf", EFI_FILE_MODE_READ, 0);
	uefi_print(L"Console Font File Opened Successfully\r\n");

	nread = 4;
	e = file->Read(file, &nread, &magic_buffer);
	check_error_and_panic(e, L"consolefont->Read(Magic)");
	if (memcmp(magic_buffer, psf1_magic, 2) == 0) {
		uefi_print(L"PSF Version 1\r\n");
		bootstrap_info.framebuffer.font.width = 8;
		bootstrap_info.framebuffer.font.height = magic_buffer[3];
		bootstrap_info.framebuffer.font.charsize = magic_buffer[3];
	} else {
		if (memcmp(magic_buffer, psf2_magic, 4) == 0)
			uefi_print(L"PSF Version 2 is currently unsupported");
		else
			uefi_print(L"Unknown PSF Version");
		uefi_hang();
	}

	allocation_size = bootstrap_info.framebuffer.font.charsize * 256;
	bitmap = uefi_alloc(allocation_size);

	nread = allocation_size;
	e = file->Read(file, &nread, bitmap);
	check_error_and_panic(e, L"load_console_font(): consolefont->Read(Bitmaps)");

	e = file->Close(file);
	check_error_and_panic(e, L"consolefont->Close()");

	if (nread != allocation_size) {
		uefi_print(L"Console font file too short");
		uefi_log(L"Console font file too short");
		uefi_hang();
	}
	bootstrap_info.framebuffer.font.bitmaps = bitmap;

	uefi_print(L"Bitmaps Loaded!\r\n");
}

enum phys_mem_type internal_memory_types[] = {
	PHYS_MEM_RESERVED,
	PHYS_MEM_USED,
	PHYS_MEM_USED,
	PHYS_MEM_BOOTSTRAP_USED,
	PHYS_MEM_BOOTSTRAP_USED,
	PHYS_MEM_RESERVED,
	PHYS_MEM_RESERVED,
	PHYS_MEM_FREE,
	PHYS_MEM_RESERVED,
	PHYS_MEM_ACPI_RECLAIMABLE,
	PHYS_MEM_RESERVED,
	PHYS_MEM_RESERVED,
	PHYS_MEM_RESERVED,
	PHYS_MEM_RESERVED,
	PHYS_MEM_NONVOLATILE
};

static void finalize_memory_map(struct memory_map *uefimap, uint64_t transition_pages) {
	size_t nentries;
	size_t allocsize, allocpages;
	size_t i;
	struct bootstrap_memory_map_entry *newmap;
	EFI_MEMORY_DESCRIPTOR *desc;

	nentries = uefimap->map_size / uefimap->descriptor_size + 1;
	allocsize = nentries * sizeof(struct bootstrap_memory_map_entry);
	allocpages = (allocsize + EFI_PAGESIZE - 1) / EFI_PAGESIZE;

	desc = &uefimap->map[0];
	newmap = NULL;
	for (i = 0; i < nentries; desc = (void *) ((uintptr_t) desc + uefimap->descriptor_size), i++) {
		if (internal_memory_types[desc->Type] == PHYS_MEM_FREE) {
			/* I'm too tired while writing this and do not want to deal with page 0 */
			if (desc->PhysicalStart == 0)
				continue;

			if (allocpages >= desc->NumberOfPages) {
				newmap = (void *) desc->PhysicalStart;
				desc->PhysicalStart += allocpages * EFI_PAGESIZE;
				desc->NumberOfPages -= allocpages;
				break;
			}
		}
	}
	if (!newmap) {
		printf("Unable to allocate space for memory map, hanging...");
		uefi_hang();
	}

	newmap[0].type = PHYS_MEM_USED;
	newmap[0].base = (uintptr_t) newmap;
	newmap[0].size = allocpages * EFI_PAGESIZE;

	desc = &uefimap->map[0];
	for (i = 0; i < nentries; desc = (void *) ((uintptr_t) desc + uefimap->descriptor_size), i++) {
		newmap[i + 1].type = internal_memory_types[desc->Type];
		newmap[i + 1].base = desc->PhysicalStart;
		newmap[i + 1].size = desc->NumberOfPages * EFI_PAGESIZE;
	}

	bootstrap_info.memory.map = newmap;
	bootstrap_info.memory.count = nentries + 1;
	bootstrap_info.memory.transition_pages = transition_pages;
}

uint64_t allocate_transition_pages(void) {
	EFI_STATUS e;
	uint64_t ret;
	
	e = system_table->BootServices->AllocatePages(
		AllocateAnyPages, EfiLoaderData, BOOTSTRAP_TRANSITION_PAGE_COUNT, &ret);
	check_error_and_panic(e, L"allocate_transition_pages(): BootServices->AllocatePages(): ");
	return ret;
}

uint64_t allocate_bootstrap_stack(void) {
	EFI_STATUS e;
	uint64_t ret;
	UINTN count;

	count = (BOOTSTRAP_STACK_SIZE + EFI_PAGESIZE - 1) / EFI_PAGESIZE;

	
	e = system_table->BootServices->AllocatePages(
		AllocateAnyPages, EfiLoaderData, count, &ret);
	check_error_and_panic(e, L"allocate_transition_pages(): BootServices->AllocatePages");
	return ret;
}

void load_init_executable(void) {
	EFI_STATUS e;
	EFI_FILE_PROTOCOL *file;
	/* this is used because EFI_FILE_INFO has an vla */
	union {
		EFI_FILE_INFO info;
		char size[sizeof(EFI_FILE_INFO) + 100];
	} info_union = { 0 };
	EFI_GUID info_guid = EFI_FILE_INFO_ID;
	UINTN info_size = sizeof(EFI_FILE_INFO) + 100;
	UINTN nbytes;
	void *buffer;

	file = get_uefi_file(L"\\boot\\init", EFI_FILE_MODE_READ, 0);
	uefi_print(L"Init executable file opened\r\n");

	e = file->GetInfo(file, &info_guid, &info_size, &info_union.info);
	check_error_and_panic(e, L"initfile->GetInfo()");

	buffer = uefi_alloc(info_union.info.FileSize);
	nbytes = info_union.info.FileSize;
	e = file->Read(file, &nbytes, buffer);
	check_error_and_panic(e, L"initfile->Read()");

	e = file->Close(file);
	check_error_and_panic(e, L"initfile->Close()");

	if (nbytes != info_union.info.FileSize) {
		uefi_print(L"load_init_executable(): could not load full file");
		uefi_log(L"load_init_executable(): could not load full file");
		uefi_hang();
	}

	bootstrap_info.init.size = info_union.info.FileSize;
	bootstrap_info.init.data = buffer;
}

void bootstrap_entry(EFI_HANDLE handle, EFI_SYSTEM_TABLE *st) {
	struct memory_map *memmap;
	uint64_t transition_pages;

	EFI_STATUS e;

	image_handle = handle;
	system_table = st;

	st->ConOut->ClearScreen(st->ConOut);
	uefi_print(L"Booted!\r\n");

	set_video_mode();
	uefi_print(L"Detecting Memory...\r\n");

	load_init_executable();

	transition_pages = allocate_transition_pages();
	bootstrap_info.memory.stack = allocate_bootstrap_stack();
	
	load_console_font();
	memmap = get_memory_map();
	e = system_table->BootServices->ExitBootServices(handle, memmap->map_key);
	check_error_and_panic(e, L"ExitBootServices()");

	finalize_memory_map(memmap, transition_pages);

	call_kentry(bootstrap_info.memory.stack);
	for (;;);
}
