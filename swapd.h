/*
 * $Id: swapd.h,v 1.5 1995/05/21 20:27:53 alfie Exp alfie $
 *
 */

# define VERSION	"1.4"

/* The directory in which the extra swap files will be created */
# ifndef TMPDIR
#  define TMPDIR	"/var/tmp"
# endif

/* The swap file name template to be created in the temporary directory */
# ifndef SUFFIX
#  define SUFFIX	"/swapd-XXXXXX"
# endif

/* The priority to raise program to (negative) */
# ifndef PRIORITY
#  define PRIORITY	-4
# endif

/* The interval (in seconds) between checks */
# ifndef INTERVAL
#  define INTERVAL	1
# endif

/* The maximum number of extra swap files that will be created */
# ifndef NUMCHUNKS
#  define NUMCHUNKS	8
# endif

/* The size of each extra swapfile */
# ifndef CHUNKSZ
#  define CHUNKSZ	4 * 1024 * 1024
# endif

/* If the free swap falls below this level, an extra swap file is added */
# ifndef LOWER
#  define LOWER		CHUNKSZ / 2
# endif

/* An extra swap file is only removed if it will leave at least much free */
# ifndef UPPER
#  define UPPER		CHUNKSZ
# endif
