#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define HAS_STAT defined(_POSIX_VERSION) 

#ifdef HAS_STAT
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct opts {
	const char *mbr_path;
	const char *out_path;
	uint64_t block_count;
	size_t block_size;
	const char *partition_file;
};

void usage_exit(const char *prog, int exitval) {
	printf("usage: %s options\n", prog);
	printf("options:\n");
	printf("\t-o out-file\n");
	printf("\t\toutput to disk out-file (required)\n");
	printf("\t-m mbr-file\n");
	printf("\t\tuse mbr-file as the master boot record (requried)\n");
	printf("\t-s lba-count\n");
	printf("\t\tcreate disk with lba-count logical blocks (required)\n");
	printf("\t-b block-size\n");
	printf("\t\tuse blocks of block-size bytes (requried)\n");
	printf("\t-p partition-file\n");
	printf("\t\tuse partition-file to describe the on disk partitions");
	printf(" (required)\n");
	printf("\t-h\n");
	printf("\t\tshow this help screen and exit\n");
	printf("\t-P\n");
	printf("\t\tprint a description of the partition file format\n");
	exit(exitval);
}
/* NOTE: this is misleading, it will wrap on the next space AFTER width columns */
void width_print(const char **strs, unsigned width) {
	unsigned col = 0;
	const char *str;
	char c;
	for (; *strs != NULL; strs++) {
		for (str = *strs; *str != '\0'; str++) {
			c = *str;
			if (c == '\n') {
				col = 0;
			} else if (col >= width && isspace(c)) {
				col = 0;
				c = '\n';
			} else {
				col++;
			}
			putchar(c);
		}
	}
}
const char *partition_usage_str[] = {
	"Partition File Format\n",
	"Partition descriptors are stored in a text file. ",
	"The file contains a series of records for each partition. ",
	"Whitespace is ignored preceding and following each line, and ",
	"lines beginning with a '#' character are comments and are ",
	"also ignored.\n",
	"An example is below:\n",
	"Partition Primary\n",
	"\tType 000000-0000-0000-0000-000000000000\n",
	"\tEnd +255\n",
	"Partition boot\n",
	"\tStart +50\n",
	"\tType esp\n",
	"\tContents boot.img\n\n",
	"This format sets up 2 partitions, named 'Primary' ",
	"and 'boot', the name of a partition can also be left empty. ",
	"The Start field indicates where the partition starts, it can either ",
	"be an absolute lba address, or relative by preceding it with a + or -. ",
	"Relative start addresses are from the end of the last partition, ",
	"if positive, or from the end of the disk, if negative. End ",
	"addresses can also be either absolute or relative, being measured ",
	"from the base of current partition, or the end of the disk if ",
	"positive or negative respectively. The type field can either contain ",
	"an explicit type guid, or a string type, currently very few string types ",
	"are supported. Finally, Contents dictates the content of the partition ",
	"if ignored, the partition is filled with zeros.\n",
	"The start field defaults to +0, the type field is required. ",
	"At least one of the End or Contents field must be filled, ",
	"If the End field is left blank, then the partition will be the smallest ",
	"it can be while still containing the entirety of the contents\n",
	NULL
};

void partition_usage_exit(void) {
	width_print(partition_usage_str, 80);
	exit(EXIT_SUCCESS);
}

struct opts parse_opts(const char *argv[]) {
	const char *prog = argv[0];
	struct opts opts = { 0 };
	const char *tmp;


	for (argv++; *argv != NULL; argv++) {
		if (strncmp(argv[0], "-o", 2) == 0) {
			if (argv[0][2] != '\0')
				opts.out_path = argv[0] + 2;
			else if (argv[1] != NULL)
				opts.out_path = *(++argv);
			else
				usage_exit(prog, EXIT_FAILURE);

		} else if (strncmp(argv[0], "-h", 2) == 0) {
			usage_exit(prog, EXIT_SUCCESS);

		} else if (strncmp(argv[0], "-m", 2) == 0) {
			if (argv[0][2] != '\0')
				opts.mbr_path = argv[0] + 2;
			else if (argv[1] != NULL)
				opts.mbr_path = *(++argv);
			else
				usage_exit(prog, EXIT_FAILURE);
		} else if (strncmp(argv[0], "-s", 2) == 0) {
			if (argv[0][2] != '\0')
				tmp = argv[0] + 2;
			else if (argv[1] != NULL)
				tmp = *(++argv);
			else
				usage_exit(prog, EXIT_FAILURE);
			opts.block_count = strtol(tmp, NULL, 0);
		} else if (strncmp(argv[0], "-b", 2) == 0) {
			if (argv[0][2] != '\0')
				tmp = argv[0] + 2;
			else if (argv[1] != NULL)
				tmp = *(++argv);
			else
				usage_exit(prog, EXIT_FAILURE);
			opts.block_size = strtol(tmp, NULL, 0);
		} else if (strncmp(argv[0], "-p", 2) == 0) {
			if (argv[0][2] != '\0')
				tmp = argv[0] + 2;
			else if (argv[1] != NULL)
				tmp = *(++argv);
			else
				usage_exit(prog, EXIT_FAILURE);
			opts.partition_file = tmp;
		} else if (strncmp(argv[0], "-P", 2) == 0) {
			partition_usage_exit();
		} else {
			printf("unknown option \"%s\"\n", argv[0]);
			usage_exit(prog, EXIT_FAILURE);
		}
	}

	if (!opts.mbr_path || !opts.out_path || !opts.block_count || !opts.block_size || !opts.partition_file)
		usage_exit(prog, EXIT_FAILURE);
	return opts;
}

int write_exact_file(FILE *dst, const char *buf, size_t n) {
	size_t total = 0;
	size_t nwrite;

	while (total < n) {
		nwrite = fwrite(buf + total, 1, n - total, dst);
		if (nwrite == 0)
			return errno ? -errno : INT_MIN;
		total += nwrite;
	}
	return 0;
}

int read_exact_file(FILE *src, char *buf, size_t n) {
	size_t total = 0;
	size_t nread;

	while (total < n) {
		nread = fread(buf + total, 1, n - total, src);
		if (nread == 0)
			return MIN(-errno, -1);
		total += nread;
	}
	return 0;
}

int exact_copy_file(FILE *dst, FILE *src, const size_t n) {
	char *buf;
	size_t total = 0;
	int ret;
	size_t sz = MIN(n, BUFSIZ);

	buf = malloc(sz);
	if (!buf)
		return -errno;

	for (total = 0; total + sz < n; total += sz) {
		ret = read_exact_file(src, buf, sz);
		if (ret)
			goto cleanup;
		ret = write_exact_file(dst, buf, sz);
		if (ret)
			goto cleanup;
	}

	/* this implies sz > n - total */
	if (total < n) {
		ret = read_exact_file(src, buf, n - total);
		if (ret)
			goto cleanup;
		ret = write_exact_file(dst, buf, n - total);
	}

cleanup:
	free(buf);
	return ret;
}

int write_mbr(FILE *outf, const char *mbrpath, size_t nblocks, size_t blocksize) {
	FILE *mbrf;
	int e, saved;
	char *buffer;
	uint16_t maxhead;
	uint8_t maxsector;
	uint8_t maxcylinder;

	buffer = calloc(1, blocksize);
	if (!buffer)
		return -errno;

	mbrf = fopen(mbrpath, "rb");
	if (!mbrf) {
		free(buffer);
		perror(mbrpath);
		return -errno;
	}
	e = read_exact_file(mbrf, buffer, 512);
	saved = errno;	
	fclose(mbrf);
	if (e) {
		free(buffer);
		return saved;
	}
	/* TODO: this is gross */

	/* alter */
	memset(buffer + 440, 0, 6);
	memset(buffer + 462, 0, 48);
	buffer[510] = 0x55;
	buffer[511] = (char) 0xaa;

	if (nblocks > 0xffffff) {
		maxcylinder = 0xff;
		maxhead = 0x3ff;
		maxsector = 0x3f;
	} else {
		maxcylinder = nblocks >> 16;
		maxhead = (nblocks >> 6) & 0x3ff;
		maxsector = nblocks & 0x3f;
	}

	/* flags */
	buffer[446] = 0;
	/* starting chs */
	buffer[447] = 0;
	buffer[448] = 2;
	buffer[449] = 0;
	/* type */
	buffer[450] = (char) 0xee;
	/* ending chs */
	buffer[451] = maxcylinder;
	buffer[452] = maxsector | ((maxhead >> 2) & 0xc0);
	buffer[453] = maxhead;
	/* starting lba */
	buffer[454] = 1;
	buffer[455] = 0;
	buffer[456] = 0;
	buffer[457] = 0;
	/* ending lba */
	nblocks = MIN(nblocks - 1, 0xffffffff);
	buffer[458] = nblocks;
	buffer[459] = nblocks >> 8;
	buffer[460] = nblocks >> 16;
	buffer[461] = nblocks >> 24;

	e = write_exact_file(outf, buffer, blocksize);
	free(buffer);
	return e;
}


#define PARTITION_NAME_LENGTH 36
struct partition_record {
	char typeguid[16];
	char partitionguid[16];
	uint64_t starting_lba;
	uint64_t ending_lba;
	uint64_t attributes;
	uint16_t name[PARTITION_NAME_LENGTH];
	const char *path;
};

struct partition_position {
	enum {
		POSITION_UNSET,
		POSITION_RELATIVE,
		POSITION_ABSOLUTE
	} type;
	int64_t value;
};

struct partition_descriptor {
	char *type;
	struct partition_position start;
	struct partition_position end;
	char *name;
	const char *content_path; 
	struct partition_descriptor *next;
};

struct gpt_record {
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	char diskguid[16];
	uint32_t npartitions;
	uint32_t partition_map_crc;
	struct partition_record *partitions;

	char *partition_map;
	uint64_t block_count;
	uint64_t block_size;
};

#define PARTITION_FILE_MAX_LINE_LENGTH 512


int begins_with(const char *string, const char *pattern) {
	return !strncmp(string, pattern, strlen(pattern));
}


const char *skip_whitespace(const char *s) {
	while (isspace(*s))
		s++;
	return s;
}

int parse_partition_declaration(struct partition_descriptor **descriptor, const char *line) {	
	char *name;
	struct partition_descriptor *tmp;

	if (!begins_with(line, "Partition"))
		return 0;
	line += strlen("Partition");
	if (*line != '\0' && !isspace(*line))
		return 0;

	tmp = calloc(1, sizeof(struct partition_descriptor));
	if (!tmp)
		return -errno;
	tmp->next = *descriptor;
	*descriptor = tmp;

	line = skip_whitespace(line);
	name = malloc(strlen(line) + 1);
	if (!name)
		return -errno;
	strcpy(name, line);
	(*descriptor)->name = name;
	return 1;
}

int parse_partition_position(struct partition_position *pos, const char *line) {
	char *endp;
	uint64_t val;
	val = strtol(line, &endp, 0);
	if (*endp != '\0') {
		printf("invalid partition position: \"%s\"\n", line);
		return INT_MIN;
	}
	if (*line == '+' || *line == '-')
		pos->type = POSITION_RELATIVE;
	else
		pos->type = POSITION_ABSOLUTE;
	pos->value = val;
	return 1;
} 

int parse_partition_comment(struct partition_descriptor **d, const char *line) {
	(void) d;
	return *line == '#' || *line == '\0';
}

int parse_partition_start(struct partition_descriptor **descriptor, const char *line) {
	if (!begins_with(line, "Start "))
		return 0;
	line += strlen("Start ");
	line = skip_whitespace(line);
	return parse_partition_position(&(*descriptor)->start, line);
}

int parse_partition_end(struct partition_descriptor **descriptor, const char *line) {
	if (!begins_with(line, "End "))
		return 0;
	line += strlen("End ");
	line = skip_whitespace(line);
	return parse_partition_position(&(*descriptor)->end, line);
}

int parse_partition_contents(struct partition_descriptor **descriptor, const char *line) {
	char *tmp;
	if (!begins_with(line, "Contents "))
		return 0;
	line += strlen("Contents ");
	line = skip_whitespace(line);

	tmp = malloc(strlen(line) + 1);
	if (!tmp)
		return -errno;
	strcpy(tmp, line);
	(*descriptor)->content_path = tmp;
	return 1;
}

int parse_partition_type(struct partition_descriptor **descriptor, const char *line) {
	char *tmp;
	if (!begins_with(line, "Type "))
		return 0;
	line += strlen("Type ");
	line = skip_whitespace(line);
	tmp = malloc(strlen(line) + 1);
	if (!tmp)
		return -errno;
	strcpy(tmp, line);
	(*descriptor)->type = tmp;
	return 1;
}

/* return 1 on success, 0 if not matching, -errno on failure */
int (*partition_parse_funcs[])(struct partition_descriptor **, const char *) = {
	parse_partition_declaration,
	parse_partition_type,
	parse_partition_start,
	parse_partition_end,
	parse_partition_contents,
	parse_partition_comment,
	NULL
};

struct partition_descriptor *reverse_partition_descriptors(struct partition_descriptor *head) {
	struct partition_descriptor *a, *b, *c;
	a = head;
	b = c = NULL;
	while (a) {
		c = b;
		b = a;
		a = a->next;
		b->next = c;
	}
	return b;
}

int read_partition_descriptors(struct partition_descriptor **descs, FILE *partf) {
	struct partition_descriptor *descriptor = NULL;
	struct partition_descriptor *tmpdesc;
	char buf[PARTITION_FILE_MAX_LINE_LENGTH];
	char *trimmed, *tmp;
	size_t i;
	int e;

	while (fgets(buf, PARTITION_FILE_MAX_LINE_LENGTH, partf)) {
		trimmed = (char *) skip_whitespace(buf);
		tmp = strchr(trimmed, '\0');
		tmp--;
		while (isspace(*tmp))
			*tmp-- = '\0';
		for (i = 0; partition_parse_funcs[i] != NULL; i++) {
			e = partition_parse_funcs[i](&descriptor, trimmed);
			if (e == 1)
				break;
			if (e < 0)
				goto error;
		}
		if (partition_parse_funcs[i] == NULL) {
			printf("error on partition descriptor file: \"%s\"\n", buf);
			e = INT_MIN;
			goto error;
		}
	}
	if (ferror(partf)) {
		e = -errno;
		goto error;
	}
	*descs = reverse_partition_descriptors(descriptor);	
	return 0;

error:
	while (descriptor) {
		tmpdesc = descriptor->next;
		free(descriptor);
		descriptor = tmpdesc;
	}
	return e;
}

int finalize_partition_start(size_t partition_idx, 
		struct gpt_record *gpt, struct partition_descriptor *desc) {
	struct partition_record *record;
	struct partition_record *lastrecord;
	size_t i;
	uint64_t base;

	record = &gpt->partitions[partition_idx];
	lastrecord = &gpt->partitions[partition_idx - 1];
	if (desc->start.type == POSITION_UNSET) {
		desc->start.type = POSITION_RELATIVE;
		desc->start.value = 0;
	}

	if (desc->start.type == POSITION_RELATIVE) {
		if (desc->start.value < 0)
			base = gpt->last_usable_lba;
		else if (partition_idx == 0)
			base = gpt->first_usable_lba;
		else
			base = lastrecord->ending_lba + 1;
		record->starting_lba = base + desc->start.value;
	} else {
		record->starting_lba = desc->start.value;
	}

	if (record->starting_lba < gpt->first_usable_lba) {
		printf("error: partition %zu (\"%s\"): ", partition_idx, desc->name);
		printf("starting lba (%" PRId64 ") lower than first usable lba (%" PRId64 ")\n",
			record->starting_lba, gpt->first_usable_lba);
		return INT_MIN;
	} else if (record->starting_lba > gpt->last_usable_lba) {
		printf("error: partition %zu (\"%s\"): ", partition_idx, desc->name);
		printf("starting lba (%" PRId64 ") greater than last usable lba (%" PRId64 ")\n",
			record->starting_lba, gpt->last_usable_lba);
		return INT_MIN;
	} else {
		for (i = 0; i < partition_idx; i++) {
			lastrecord = &gpt->partitions[i];
			if (record->starting_lba < lastrecord->starting_lba)
				continue;
			if (record->starting_lba > lastrecord->ending_lba)
				continue;
			printf("error: partition %zu overlaps with", partition_idx);
			printf(" partition %zu\n", i);
			return INT_MIN;
		}
	}
	return 0;
}

int64_t getfilesize(const char *path) {
#ifdef HAS_STAT
	int e;
	struct stat sb;
	e = stat(path, &sb);
	if (e)
		return -errno;
	return sb.st_size;
#else /* HAS_STAT */
	FILE *fp;
	int e, saved;
	fp = fopen(path, "rb");
	if (!fp)
		return -errno;
	e = fseek(fp, 0, SEEK_END);
	if (!e)
		e = ftell(fp)
	saved = errno;
	fclose(fp);
	if (e < 0)
		return -saved;
	else
		return e;
#endif /* HAS_STAT */	
}

int finalize_partition_end(size_t partition_idx, 
		struct gpt_record *gpt, struct partition_descriptor *desc) {
	struct partition_record *record;
	struct partition_record *lastrecord;
	uint64_t base;
	int64_t length;
	size_t i;
	record = &gpt->partitions[partition_idx];
	lastrecord = &gpt->partitions[partition_idx - 1];
	if (desc->end.type == POSITION_UNSET) {
		if (desc->content_path == NULL) {
			printf("error: partition %zu (\"%s\") ", partition_idx, desc->name);
			printf("lacks both an ending block and file content\n");
			return INT_MIN;
		}
		length = getfilesize(desc->content_path);
		if (length < 0) {
			printf("error: partition %zu (\"%s\") ", partition_idx, desc->name);
			printf("could not get length of file %s: ", desc->content_path);
			printf("%s", strerror(-length));
			return INT_MIN;
		}
		length += gpt->block_size - 1;
		length /= gpt->block_size;

		desc->end.type = POSITION_RELATIVE;
		desc->end.value = length;
	}

	if (desc->end.type == POSITION_RELATIVE) {
		if (desc->end.value < 0)
			base = gpt->last_usable_lba;
		else
			base = record->starting_lba;
		record->ending_lba = base + desc->end.value - 1;
	} else {
		record->ending_lba = desc->end.value;
	}

	if (record->ending_lba < record->starting_lba) {
		printf("error: partition %zu (\"%s\"): ", partition_idx, desc->name);
		printf("starting lba (%" PRId64 ") is greater than ending lba(%" PRId64 ")\n",
			record->starting_lba, record->ending_lba);
		return INT_MIN;
	} else if (record->ending_lba > gpt->last_usable_lba) {
		printf("error: partition %zu (\"%s\"): ", partition_idx, desc->name);
		printf("ending lba (%" PRId64 ") is greater than last usable lba (%" PRId64 ")\n",
			record->ending_lba, gpt->last_usable_lba);
		return INT_MIN;
	} else {
		for (i = 0; i < partition_idx; i++) {
			lastrecord = &gpt->partitions[i];
			if (lastrecord->starting_lba < record->starting_lba)
				continue;
			if (lastrecord->starting_lba > record->ending_lba)
				continue;
			printf("error: partition %zu ", partition_idx);
			printf("overlaps with partition %zu\n", i);
			return INT_MIN;
		}
	}
	return 0;
}

const struct {
	char *name;
	unsigned char guid[16];
} type_guids[] = {
	{ "zero", { 0 }},
	{ "esp", { 0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11, 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b}},
	{ NULL, { 0 }}
};

int validate_guid(const char *str) {
	size_t i = 0;
	for (; i < 8; i++)
		if (!isdigit(str[i]))
			return 1;
	if (str[i++] != '-')
		return 1;

	for (; i < 13; i++)
		if (!isdigit(str[i]))
			return 1;
	if (str[i++] != '-')
		return 1;

	for (; i < 18; i++)
		if (!isdigit(str[i]))
			return 1;
	if (str[i++] != '-')
		return 1;

	for (; i < 23; i++)
		if (!isdigit(str[i]))
			return 1;
	if (str[i++] != '-')
		return 1;
	for (; i < 36; i++)
		if (!isdigit(str[i]))
			return 1;
	return str[i] != '\0';
}

int convert_guid(char *dst, const char *src) {
	uint32_t g1;
	uint16_t g2, g3, g4;
	uint64_t g5;
	int n;

	n = sscanf(src, "%" SCNx32 "-%" SCNx16 "-%" SCNx16 "-%" SCNx16 "-%" SCNx64, 
		&g1, &g2, &g3, &g4, &g5);
	if (n != 5)
		return INT_MIN;

	dst[0] = g1;
	dst[1] = g1 >> 8;
	dst[2] = g1 >> 16;
	dst[3] = g1 >> 24;

	dst[4] = g2;
	dst[5] = g2 >> 8;

	dst[6] = g3;
	dst[7] = g3 >> 8;

	dst[8] = g4 >> 8;
	dst[9] = g4;

	dst[10] = g5 >> 40;
	dst[11] = g5 >> 32;
	dst[12] = g5 >> 24;
	dst[13] = g5 >> 16;
	dst[14] = g5 >> 8;
	dst[15] = g5;
	return 0;
}

int finalize_partition_type(struct partition_record *rec, struct partition_descriptor *desc) {
	size_t i;
	if (desc->type == NULL) {
		printf("error: partition \"%s\" missing type\n", desc->name);
		return INT_MIN;
	}

	for (i = 0; type_guids[i].name != NULL; i++) {
		if (strcmp(desc->type, type_guids[i].name) == 0) {
			memcpy(&rec->typeguid, type_guids[i].guid, 16);
			return 0;
		}
	}
	if (validate_guid(desc->type) || convert_guid(rec->typeguid, desc->type)) {
		printf("error: partition \"%s\" ", desc->name);
		printf("unknown type \"%s\"\n", desc->type);
		return INT_MIN;
	}

	return 0;
}

int generate_guid(char *dst) {
	/* TODO: better rng (ifdef urandom) */
	size_t i;
	for (i = 0; i < 16; i++)
		dst[i] = rand();

	dst[6] = 0x40 | (dst[6] & 0xf);
	dst[8] = 0x80 | (dst[8] & 0x3f);
	return 0;
}

int finalize_partition_record(size_t partition_idx, 
		struct gpt_record *gpt, struct partition_descriptor *desc) {
	size_t i;
	int e;
	struct partition_record *record;
	record = &gpt->partitions[partition_idx];

	if (strlen(desc->name) > PARTITION_NAME_LENGTH) {
		printf("partition name \"%s\" too long (max %d)\n",
			desc->name, PARTITION_NAME_LENGTH);
		return INT_MIN;
	}

	/* Has to be done manually because record is 16 bit characters */
	/* but descriptor has 8 bit characters */
	for (i = 0; i < PARTITION_NAME_LENGTH; i++)
		record->name[i] = ' ';
	for (i = 0; desc->name[i] != '\0'; i++)
		record->name[i] = desc->name[i];

	e = finalize_partition_start(partition_idx, gpt, desc);
	if (e)
		return e;
	e = finalize_partition_end(partition_idx, gpt, desc);
	if (e)
		return e;

	e = finalize_partition_type(record, desc);
	if (e)
		return e;
	e = generate_guid(record->partitionguid);
	if (e)
		return e;
	record->attributes = 0;
	record->path = desc->content_path;
	desc->content_path = NULL;

	return 0;
}

#define PARTITION_ENTRY_SIZE 128

int make_partition_records(struct gpt_record *gpt, struct partition_descriptor *descs) {
	struct partition_descriptor *tmpdesc;
	size_t partition_map_lba_sz, i;
	struct partition_record *records = NULL;
	int e;

	gpt->npartitions = 0;
	for (tmpdesc = descs; tmpdesc != NULL; tmpdesc = tmpdesc->next)
		gpt->npartitions++;

	partition_map_lba_sz = gpt->npartitions * PARTITION_ENTRY_SIZE;
	partition_map_lba_sz += gpt->block_size - 1;
	partition_map_lba_sz /= gpt->block_size;

	gpt->first_usable_lba = 2 + partition_map_lba_sz;
	gpt->last_usable_lba = gpt->block_count - gpt->first_usable_lba;

	records = calloc(gpt->npartitions, sizeof(struct partition_record));
	if (!records)
		return -errno;
	gpt->partitions = records;

	for (tmpdesc = descs, i = 0; tmpdesc != NULL; tmpdesc = tmpdesc->next, i++) {
		e = finalize_partition_record(i, gpt, tmpdesc);
		if (e)
			goto error;
	}

	return 0;
error:
	for (i = 0; i < gpt->npartitions; i++)
		free((char *) records[i].path);
	free(records);
	gpt->partitions = NULL; /* avoid double free */
	return e;
}

int read_partitions(struct gpt_record *rec, const char *partfile) {
	FILE *partf;
	struct partition_descriptor *descriptors, *tmpdesc;
	int e;

	partf = fopen(partfile, "r");
	if (!partf)
		return -errno;
	e = read_partition_descriptors(&descriptors, partf);
	fclose(partf);
	if (e)
		return e; 

	e = make_partition_records(rec, descriptors);
	while (descriptors) {
		tmpdesc = descriptors->next;
		free(descriptors);
		descriptors = tmpdesc;
	}
	return e;
}

#define CRC32POLY 0x04C11DB7U

uint32_t crc32(const char *src_, size_t n) {
	const uint8_t *src = (const uint8_t *) src_;
	uint32_t crc;
	uint32_t mask;
	uint32_t reflected;
	size_t i;
	uint8_t bit;


	crc = 0;
	for (i = 0; i < 32; i++) {
		crc <<= 1;
		crc |= src[i / 8] >> (i & 7) & 1;
	}

	crc = ~crc;
	
	for (; i < n * 8 + 32; i++) {
		mask = crc >> 31 ? CRC32POLY : 0;

		bit = i / 8 < n ? src[i / 8] : 0;
		bit = bit >> (i & 7);
		bit &= 1;


		crc <<= 1;
		crc |= bit;

		crc ^= mask;
	}

	reflected = 0;
	for (i = 0; i < 32; i++) {
		reflected |= ((crc >> i) & 1) << (31 - i);
	}

	return ~reflected;
}

void little_endian_write(char *buf, uint64_t n, uint8_t size) {
	while (size--) {
		*buf++ = n;
		n >>= 8;
	}
}

int generate_partition_map(struct gpt_record *gpt) {
	size_t mapsz, i, j;
	char *curbuf;
	struct partition_record *rec;

	mapsz = gpt->npartitions * PARTITION_ENTRY_SIZE;
	gpt->partition_map = malloc(mapsz);
	if (!gpt->partition_map)
		return -errno;

	curbuf = gpt->partition_map;
	for (i = 0; i < gpt->npartitions; i++, curbuf += PARTITION_ENTRY_SIZE) {
		rec = &gpt->partitions[i];
		memcpy(curbuf, rec->typeguid, 16);
		memcpy(curbuf + 16, rec->partitionguid, 16);
		little_endian_write(curbuf + 32, rec->starting_lba, 8);
		little_endian_write(curbuf + 40, rec->ending_lba, 8);
		little_endian_write(curbuf + 48, rec->attributes, 8);
		for (j = 0; j < PARTITION_NAME_LENGTH; j++)
			little_endian_write(curbuf + 56 + j * 2, rec->name[j], 2);
	}

	return 0;
}

int create_gpt_record(struct gpt_record *gpt, const char *partfile) {
	int e;
	size_t mapsz;

	e = generate_guid(gpt->diskguid);
	if (e)
		return e;
	e = read_partitions(gpt, partfile);
	if (e)
		return e;
	e = generate_partition_map(gpt);
	if (e)
		return e;

	mapsz = gpt->npartitions * PARTITION_ENTRY_SIZE;
	gpt->partition_map_crc = crc32(gpt->partition_map, mapsz);
	return 0;
}

#define GPT_SIGNATURE_LOWER UINT32_C(0x20494645)
#define GPT_SIGNATURE_UPPER UINT32_C(0x54524150)
#define GPT_REVISION UINT32_C(0x00010000)
#define GPT_HEADER_SIZE 96
int write_gpt(FILE *outf, struct gpt_record *rec, int primary) {
	char *buf;
	uint32_t crc;
	int e;

	buf = calloc(1, rec->block_size);
	if (!buf)
		return -errno;

	little_endian_write(buf, GPT_SIGNATURE_LOWER, 4);
	little_endian_write(buf + 4, GPT_SIGNATURE_UPPER, 4);
	little_endian_write(buf + 8, GPT_REVISION, 4);
	little_endian_write(buf + 12, GPT_HEADER_SIZE, 4);
	if (primary) { /* mylba and alternatelba */
		little_endian_write(buf + 24, 1, 8); 
		little_endian_write(buf + 32, rec->block_count - 1, 8);
	} else {
		little_endian_write(buf + 24, rec->block_count - 1, 8);
		little_endian_write(buf + 32, 1, 8);
	}

	little_endian_write(buf + 40, rec->first_usable_lba, 8);
	little_endian_write(buf + 48, rec->last_usable_lba, 8);
	memcpy(buf + 56, rec->diskguid, 16);
	if (primary) /* partition map lba */
		little_endian_write(buf + 72, 2, 8);
	else
		little_endian_write(buf + 72, rec->last_usable_lba + 1, 8);
	little_endian_write(buf + 80, rec->npartitions, 4);
	little_endian_write(buf + 84, PARTITION_ENTRY_SIZE, 4);
	little_endian_write(buf + 88, rec->partition_map_crc, 4);

	crc = crc32(buf, GPT_HEADER_SIZE);
	little_endian_write(buf + 16, crc, 4);

	e = write_exact_file(outf, buf, rec->block_size);
	free(buf);
	return e;
}

int write_partition_table(FILE *outf, struct gpt_record *gpt) {
	int e;
	size_t mapsz;
	size_t padsz;

	mapsz = gpt->npartitions * PARTITION_ENTRY_SIZE;
	e = write_exact_file(outf, gpt->partition_map, mapsz);
	if (e)
		return e;
	padsz = gpt->block_size - mapsz % gpt->block_size;
	for (; padsz > 0; padsz--) {
		e = putc(0, outf);
		if (e == EOF)
			return -errno;
	}
	return 0;
}

int partition_comparator(const void *a, const void *b) {
	const struct partition_record *pa, *pb;
	pa = a;
	pb = b;

	return pb->starting_lba - pa->starting_lba;
}

int write_disk_body(FILE *outf, struct gpt_record *gpt) {
	struct partition_record *partitions = NULL;
	struct partition_record *curpartition;
	size_t allocsz, i, bytes, zerosz;
	int64_t filelen;
	FILE *fp;
	uint64_t current_block;
	char *zeroblock = NULL;
	int e = 0;

	zerosz = 8 * gpt->block_size;
	zeroblock = malloc(zerosz);
	if (!zeroblock) {
		e = -errno;
		goto cleanup;
	}

	allocsz = sizeof(struct partition_record) * gpt->npartitions;
	partitions = malloc(allocsz);
	if (!partitions) {
		e = -errno;
		goto cleanup;
	}
	memcpy(partitions, gpt->partitions, allocsz);
	qsort(partitions, gpt->npartitions, sizeof(struct partition_record), partition_comparator); 

	current_block = gpt->first_usable_lba;
	for (i = 0; i < gpt->npartitions; i++) {
		curpartition = &gpt->partitions[i];
		while (curpartition->starting_lba > current_block) {
			bytes = curpartition->starting_lba - current_block;
			bytes *= gpt->block_size;
			e = write_exact_file(outf, zeroblock, MIN(zerosz, bytes));
			if (e)
				goto cleanup;
			current_block += MIN(zerosz, bytes) / gpt->block_size;
		}

		if (curpartition->path == NULL)
			continue;

		filelen = getfilesize(curpartition->path);
		if (filelen < 0)
			goto cleanup;
		fp = fopen(curpartition->path, "rb");
		if (!fp)
			goto cleanup;
		bytes = curpartition->ending_lba - curpartition->starting_lba + 1;
		bytes *= gpt->block_size;
		if (bytes < (size_t) filelen) {
			e = exact_copy_file(outf, fp, bytes);
		} else {
			e = exact_copy_file(outf, fp, filelen);
			if (!e) {
				bytes -= filelen; 
				e = write_exact_file(outf, zeroblock, bytes);
			}
		}
		fclose(fp);
		if (e)
			goto cleanup;
		current_block = curpartition->ending_lba + 1;
	}

	while (current_block <= gpt->last_usable_lba) {
		bytes = 1 + gpt->last_usable_lba - current_block;
		bytes *= gpt->block_size;
		e = write_exact_file(outf, zeroblock, MIN(zerosz, bytes));
		if (e)
			goto cleanup;
		current_block += MIN(zerosz, bytes) / gpt->block_size;
	}

cleanup:
	free(zeroblock);
	free(partitions);
	return e;
}

int main(int argc, const char *argv[]) {
	struct opts opts;
	FILE *outfile;
	int s = 0;
	struct gpt_record gpt_rec = { 0 };

	(void) argc;
	opts = parse_opts(argv);

	gpt_rec.block_size = opts.block_size;
	gpt_rec.block_count = opts.block_count;
	outfile = fopen(opts.out_path, "wb");
	if (!outfile) {
		perror(opts.out_path);
		return EXIT_FAILURE;
	}

	s = write_mbr(outfile, opts.mbr_path, opts.block_count, opts.block_size);
	if (s) { 
		fprintf(stderr, "write_mbr(): %s\n", strerror(-s));
		goto cleanup;
	}

	s = create_gpt_record(&gpt_rec, opts.partition_file);
	if (s) {
		if (s != INT_MIN)
			fprintf(stderr, "create_gpt_record(): %s\n", strerror(-s));
		goto cleanup;
	} 

	s = write_gpt(outfile, &gpt_rec, 1);
	if (s) {
		fprintf(stderr, "write_primary_gpt(): %s\n", strerror(-s));
		goto cleanup;
	}

	s = write_partition_table(outfile, &gpt_rec);
	if (s) {
		fprintf(stderr, "write_partition_table(): %s\n", strerror(-s));
		goto cleanup;
	}

	s = write_disk_body(outfile, &gpt_rec);
	if (s) {
		fprintf(stderr, "write_disk_body(): %s\n", strerror(-s));
		goto cleanup;
	}

	s = write_partition_table(outfile, &gpt_rec);
	if (s) {
		fprintf(stderr, "write_partition_table(): %s\n", strerror(-s));
		goto cleanup;
	}

	s = write_gpt(outfile, &gpt_rec, 0);
	if (s)
		fprintf(stderr, "write_alternate_gpt(): %s\n", strerror(-s));
cleanup:
	if (gpt_rec.partitions)
		free(gpt_rec.partitions);
	if (gpt_rec.partition_map)
		free(gpt_rec.partition_map);
	fclose(outfile);
	return s ? EXIT_FAILURE : EXIT_SUCCESS;
}

