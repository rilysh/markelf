/* published in public domain. */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <err.h>
#include <getopt.h>
#include <errno.h>

/* Noreturn specifier. */
#if defined (__GNUC__) || defined (__clang__)	\
	|| defined (__TINYC__)
# define NORETURN    __attribute__((noreturn))
#else
# define NORETURN
#endif

/* ELF class architecture. */
#define ELF_CLASS_A32         (1)
#define ELF_CLASS_A64         (2)

/* Is the input value a digit? */
#define IS_DIGIT(x)      ((unsigned int)*arg - '0' < 10)

/* Array size. */
#define ARRAY_SIZE(x)    (sizeof(x) / sizeof(x[0]))

/* Options structure. */
struct opts {
	int opt;
	int bopt;
	int copt;
	int topt;
};

/* Key value storage structure. */
struct kvstorage {
	int key;
	const char *val;
};

/* Mark the ELF class (either as 32-bit or 64-bit). */
static int mark_elf_class(int fd, int is_32b)
{
	char bw[1];

	/* Look for the ELF class. */
	if (lseek(fd, 4, SEEK_SET) == (off_t)-1) {
	        warn("lseek()");
		return (-1);
	}

	/* A32 means 32-bit and A64 means 64-bit. */
        bw[0] = (is_32b <= 0) ? ELF_CLASS_A32 : ELF_CLASS_A64;

	/* Write the buffer to that position. */
        if (write(fd, bw, 1) < 0) {
		warn("write()");
		return (-1);
	}

	return (0);
}

/* Mark the OS ABI to a specific ABI. */
static int mark_elf_osabi(int fd, int vabi)
{
	char bw[1];

	/* ABI version cannot be larger than this. */
	if (vabi > 18) {
	        warnx("cannot set an unknown ABI version.");
		return (-1);
	}

	/* Seek for the offset 7. */
	if (lseek(fd, 7, SEEK_SET) < (off_t)0) {
		warn("lseek()");
		return (-1);
	}

	/* Write data to that offset. */
	bw[0] = (char)vabi;
	if (write(fd, bw, 1) < 0) {
		warn("write()");
		return (-1);
	}

	return (0);
}

/* Convert the string to an integer. */
static int ascii_to_int(const char *s)
{
	long ival;
	char *eptr;

	ival = strtol(s, &eptr, 10);

	if (errno != 0)
		err(EXIT_FAILURE, "strtol()");
	if (eptr == s)
		errx(EXIT_FAILURE,
		     "error: invalid ABI number.");

	return (int)ival;
}

/* Print the usage information. */
NORETURN static void print_usage(int status)
{
	FILE *out;

	out = (status == EXIT_SUCCESS) ? stdout : stderr;

	fputs("* markelf *\n"
	      "usage: [c, class], [t, type], [b, to32], [h, help]\n"
	      "types: sysv(0), hpux(1), netbsd(2), linux(3), hurd(4)\n"
	      "solaris(6), aix(7), irix(8), freebsd(9), tru64(10)\n"
	      "modesto(11), openbsd(12), openvms(13), nonstopkernel(14)\n"
	      "aros(15), fenix(16), cloudabi(17), openvos(18)\n",
	      out);
	exit(status);
}

int main(int argc, char **argv)
{
	struct opts opts;
	int fd, vabi, one_ok;
	char *arg;
	size_t i;
	const struct option loptions[] = {
		{ "class", no_argument,       NULL, 'c' },
		{ "type",  required_argument, NULL, 't' },
		{ "to64",  no_argument,       NULL, 'b' },
		{ "help",  no_argument,       NULL, 'h' },
		{  NULL,   0,                 NULL,  0, },
	};
	const struct kvstorage kv[] = {
		{ 0,  "sysv"     },  { 1,  "hpux"           },
		{ 2,  "netbsd"   },  { 3,  "linux"          },
		{ 4,  "hurd"     },  { 6,  "solaris"        },
		{ 7,  "aix"      },  { 8,  "irix"           },
		{ 9,  "freebsd"  },  { 10, "tru64"          },
		{ 11, "modesto"  },  { 12, "openbsd"        },
		{ 13, "openvms"  },  { 14, "nonstopkernel"  },
		{ 15, "aros",    },  { 16, "fenix"          },
		{ 17, "cloudabi" },  { 18, "openvos"        },
	};

	/* When we don't have enough arguments. */
	if (argc < 2)
		errx(EXIT_FAILURE, "error: no args are provided.");

	opts.bopt = opts.copt = opts.topt = one_ok = 0;
	while ((opts.opt = getopt_long(argc, argv, "ct:bh",
				       loptions, NULL)) != -1) {
		switch (opts.opt) {
		case 'c':
			/* Option: '-c' ('--class') */
			opts.copt = 1;
			break;

		case 't':
			/* Option: '-t' ('--type') */
			opts.topt = 1;
			arg = optarg;
			break;

		case 'b':
			/* Option: '-b' ('--to32') */
			opts.bopt = 1;
			break;

		case 'h':
			/* Option: '-h' ('--help') */
			print_usage(EXIT_SUCCESS);
			/* Unreachable. */
			break;

		default:
			/* Invalid option. */
			exit(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (opts.bopt && opts.copt == 0)
		errx(EXIT_FAILURE,
		     "error: option '-b' cannot be used "
		     "without the use of option '-c'.");

	/* Parity check. */
	if (argv[0] == NULL)
		errx(EXIT_FAILURE, "error: no file path was provided.");
	if (opts.copt == 0 && opts.topt == 0)
		errx(EXIT_FAILURE, "error: no option is provided.");

	fd = open(argv[0], O_RDWR);
	if (fd < 0)
		errx(EXIT_FAILURE,
		     "error: cannot open file '%s' "
		     "for reading and writing.", argv[0]);

	/* When we provide the '--class' option. */
	if (opts.copt) {
		mark_elf_class(fd, opts.bopt);
		if (opts.bopt)
			fprintf(stdout,
				"ok: marked '%s' as 64-bit binary.\n", argv[0]);
		else
			fprintf(stdout,
			      "ok: marked '%s' as 32-bit binary.\n", argv[0]);
	}

	/* When we provide the '--type' option. */
	if (opts.topt) {
		/* Check for the ABI version argument. */
		if (IS_DIGIT(*arg)) {
			mark_elf_osabi(fd, ascii_to_int(arg));
			fprintf(stdout, "ok: marking '%s' ABI to '%s'.\n",
				argv[0], arg);
		} else {
			/* Iterate over the structure to match
			   our query (if exists). */
			for (i = 0; i < ARRAY_SIZE(kv); i++) {
				if (strcasecmp(kv[i].val, arg) == 0) {
					vabi = kv[i].key;
					one_ok = 1;
					break;
				}
			}

			/* Test to know if there's at least one matching we
			   found that's similar to our predefined keys. */
			if (one_ok) {
				mark_elf_osabi(fd, vabi);
				fprintf(stdout, "ok: marking '%s' ABI to '%s'.\n",
					argv[0], arg);
			} else {
				close(fd);
				errx(EXIT_FAILURE,
				     "error: invalid ABI name.");
			}
		}
	}

	/* Close the file descriptor. */
	close(fd);
}
