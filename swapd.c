/*
 * $Id: swapd.c,v 1.2 1995/02/12 22:36:00 alfie Exp alfie $ 
 *
 * swapd - dynamically add and remove swap
 *
 *     Nick Holloway <Nick.Holloway@alfie.demon.co.uk>
 */ 

# include <stdio.h>
# include <syslog.h>
# include <unistd.h>
# include <stdlib.h>
# include <malloc.h>
# include <getopt.h>
# include <signal.h>
# include <errno.h>
# include <sys/stat.h>
# include <sys/file.h>
# include <sys/vfs.h>
# include <sys/resource.h>
# include <linux/mm.h>

# include "swapd.h"

int     priority  = PRIORITY;   /* -p */
int     chunksz   = CHUNKSZ;    /* -s */
int     numchunks = NUMCHUNKS;  /* -n */
int     lower     = LOWER;      /* -l */
int     upper     = UPPER;      /* -u */
int     interval  = INTERVAL;   /* -i */
char   *tmpdir    = TMPDIR;     /* -d */

/*
 * The intention was that `debug' would control the amount of logging that
 * occurred.  However, setlogmask is currently broke (fixed in future
 * release of libc).  So, if you want the extra logging, define DEBUG.
 */
int	debug	= 0;		/* -D */

char  **swapfile;		/* the names of swap files created */
int     chunks	= 0;		/* number of swap files currently alloc'ed */

int     addswap ( int );
int     delswap ( int );

/*
 * Check the parameters (either given or compiled in) for sense.
 */
void	ckconf ( char *argv0 )
{
    struct stat st;
    char *msg = NULL;

    if ( lstat ( tmpdir, &st ) < 0 || ! S_ISDIR ( st.st_mode ) )
	msg = "tmpdir is not a directory";
    else if ( chunksz < 2 * PAGE_SIZE )
	msg = "size too small for swapfile";
    else if ( chunksz > ( PAGE_SIZE - 10 ) * 8 * PAGE_SIZE )
	msg = "size too large for swapfile (strange, but true)";
    else if ( upper < lower )
	msg = "lower limit is smaller than upper limit";

    if ( msg ) {
	fprintf ( stderr, "%s: %s\n", argv0, msg );
	exit ( 1 );
    }
}

/*
 * Remove all swapfiles.
 */
void	cleanup ()
{
    while ( chunks ) {
        delswap ( --chunks );
    }
}

/*
 * On signals, tidy up, and vanish.
 */
void    sighandler ( int signal )
{
#ifdef DEBUG
    syslog ( LOG_INFO, "shutting down on signal %d...", signal );
#endif
    cleanup ();
#ifdef DEBUG
    syslog ( LOG_INFO, "...done" );
#endif
    exit ( 1 );
}

/*
 * Usage, with optional error message.
 */
void	usage ( char *argv0, char *str )
{
    if ( str && *str )
	(void) fprintf ( stderr, "%s: %s\n", argv0, str );
    (void) fprintf ( stderr, "usage: %s "
	    "[-p prio] [-d dir] [-i interval] [-n num] [-s size] [-l lower] [-u upper]\n",
	    argv0 );
    exit ( 1 );
}

/*
 * help message
 */
void	help ( char *argv0 )
{
    (void) fprintf ( stderr, "%s: dynamically maintain swap\n"
	    "\t-p\tpriority to run at\n"
	    "\t-d\tdirectory to create swap files in\n"
	    "\t-i\tinterval to check system\n"
	    "\t-s\tsize of each swap file\n"
	    "\t-l\tlower limit for spare VM (trigger to add swap)\n"
	    "\t-u\tupper limit for spare VM (trigger to remove swap)\n"
	    "\t-n\tmaximum number of swap files to create\n", argv0 );
}

/*
 * Work out what to multiply a value by based on the suffix.
 */
int suffix ( char **str )
{
    int mult = 1;
    while ( **str ) {
	switch ( *(*str)++ ) {
	case 'b': case 'B':
	    mult *= 512;
	    break;
	case 'k': case 'K':
	    mult *= 1024;
	    break;
	case 'm': case 'M':
	    mult *= 1024 * 1024;
	    break;
	default:
	    return 0;
	}
    }
    return mult;
}

/*
 * The main program.
 * Parse arguments, prepare for battle stations.
 * Loop repeatedly, deciding whether to add or remove swap based on swap
 * currently available, and the high and low water marks.
 */
int     main ( int argc, char *argv[] )
{
    long    swap, getswap();
    struct  sigaction act;
    int     i;

    while ( ( i = getopt ( argc, argv, "vhd:i:l:n:s:u:D" ) ) != -1 ) {
	switch ( i ) {
	case '?':
	    usage ( argv[0], "unknown flag, use -h for help" );
	    break;
	case 'v':
	    fprintf ( stderr, "swapd version %s\n", VERSION );
	    exit ( 0 );
	case 'h':
	    help ( argv[0] ); 
	    exit ( 0 );
	case 'p':
	    priority = strtol ( optarg, &optarg, 10 );
	    if ( priority < PRIO_MIN || priority > PRIO_MAX || *optarg != '\0' )
	        usage ( argv[0], "bad value for priority" );
	    break;
	case 'd':
	    tmpdir = optarg;
	    break;
	case 'i':
	    interval = strtol ( optarg, &optarg, 10 );
	    if ( interval < 0 || *optarg != '\0' )
	        usage ( argv[0], "bad value for interval" );
	    break;
	case 'l':
	    lower = strtol ( optarg, &optarg, 10 );
	    lower *= suffix ( &optarg );
	    if ( lower <= 0 || *optarg != '\0' )
		usage ( argv[0], "bad value for lower limit" );
	    break;
	case 'n':
	    numchunks = strtol ( optarg, &optarg, 10 );
	    numchunks *= suffix ( &optarg );
	    if ( numchunks <= 0 || *optarg != '\0' )
		usage ( argv[0], "bad value for maximum number of swapfiles" );
	    break;
	case 's':
	    chunksz = strtol ( optarg, &optarg, 10 );
	    chunksz *= suffix ( &optarg );
	    if ( chunksz <= 0 || *optarg != '\0' )
		usage ( argv[0], "bad value for size of swapfile" );
	    break;
	case 'u':
	    upper = strtol ( optarg, &optarg, 10 );
	    upper *= suffix ( &optarg );
	    if ( upper <= 0 || *optarg != '\0' )
		usage ( argv[0], "bad value for upper limit" );
	    break;
#ifdef DEBUG
	case 'D':
	    debug = 1;
	    break;
#endif
	default:
	    usage ( argv[0], "internal getopt foulup" );
	    break;
	}
    }

    if ( argc - optind != 0 ) {
	usage ( argv[0], "" );
    }

    ckconf ( argv[0] );

    if ( geteuid () ) {
        (void) fprintf ( stderr, "%s: must be run as root\n", argv[0] );
        exit ( 1 );
    }

    /* don't want swap files world readable */
    (void) umask ( 0077 );

    /* want to run at higher priority so get a chance to add swap space */
    if ( setpriority ( PRIO_PROCESS, getpid (), priority ) < 0 ) {
	(void) fprintf ( stderr, "%s: setpriority: %s\n", argv[0],
		strerror ( errno ) );
	exit ( 1 );
    }

    if ( ! debug ) {
	switch ( fork() ) {
	case -1:		/* error */
	    (void) fprintf ( stderr, "%s: fork failed\n", argv[0] );
	    exit ( 1 );
	case 0:			/* child */
	    for ( i = getdtablesize()-1; i >= 0; i-- )
	        (void) close ( i );
	    (void) setsid ();
	    break;
	default:		/* parent */
	    exit ( 0 );
	}
    }

    openlog ( "swapd", LOG_DAEMON | LOG_CONS | (debug ? LOG_PERROR : 0), 0 );
    setlogmask ( debug ? LOG_UPTO ( LOG_DEBUG ) : LOG_UPTO ( LOG_NOTICE ) );

    /* allocate memory needed at start, rather than leaving to later */
    swapfile = malloc ( sizeof(char*) * numchunks );
    if ( swapfile == NULL ) {
        syslog ( LOG_ERR, "malloc failed... bye" );
        exit ( 1 );
    }
    for ( i = 0; i < numchunks; i++ ) {
        swapfile[i] = malloc ( strlen ( tmpdir ) + sizeof ( SUFFIX ) );
	if ( swapfile[i] == NULL ) {
	    syslog ( LOG_ERR, "malloc failed... bye" );
	    exit ( 1 );
	}
    }

    act.sa_handler = sighandler;
    sigfillset ( &act.sa_mask );
    act.sa_flags = 0;
    (void) sigaction ( SIGHUP, &act, (struct sigaction*)NULL );
    (void) sigaction ( SIGINT, &act, (struct sigaction*)NULL );
    (void) sigaction ( SIGQUIT, &act, (struct sigaction*)NULL );
    (void) sigaction ( SIGTERM, &act, (struct sigaction*)NULL );

    for ( ; ; ) {
	swap = getswap();
#ifdef DEBUG
	syslog ( LOG_DEBUG, "%dk available swap", swap / 1024 );
#endif
	if ( swap < lower && chunks < numchunks ) {
	    if ( addswap ( chunks ) )
		chunks++;
	}
	if ( swap > chunksz + upper && chunks > 0 ) {
	    delswap ( --chunks );
	}
	sleep ( interval );
    }
}

/*
 * Return the current amount of swap available.
 * This is the sum of free and shared.  It is got by parsing /proc/meminfo
 *             total:   used:    free:   shared:  buffers:
 *     Mem:   7417856  5074944  2342912   880640  3563520
 *     Swap:  8441856        0  8441856
 */
long getswap ()
{
    static  int     fd	= -1;		/* fd to read meminfo from */
    char    buffer [ 512 ];		/* enough to slurp meminfo in */
    char   *cp;
    long    freemem, cache, freeswap;
    int     n;

    if ( fd < 0 ) {
        fd = open ( "/proc/meminfo", O_RDONLY );
        if ( fd < 0 ) {
	    syslog ( LOG_ERR, "can't open \"/proc/meminfo\": %m" );
            exit ( 1 );
	}
    }

    if ( lseek ( fd, 0, SEEK_SET ) == EOF ) {
        syslog ( LOG_ERR, "lseek failed on \"/proc/meminfo\": %m" );
        exit ( 1 );
    }
    if ( ( n = read ( fd, buffer, sizeof(buffer)-1 ) ) < 0 ) {
	syslog ( LOG_ERR, "read failed on \"/proc/meminfo\": %m" );
	exit ( 1 );
    }
    buffer[n] = '\0';				/* null terminate */
    cp = buffer;
    while ( *cp != '\n' )		/* skip to memory usage line */
	cp++;
    while ( *cp != ' ' )		/* skip past 'Mem:' */
	cp++;
    if ( 2 != sscanf ( cp, "%*d %*d %ld %*d %ld", &freemem, &cache ) ) {
	syslog ( LOG_ERR, "sscanf failed on memory info" );
	exit ( 1 );
    }
    while ( *cp != '\n' )		/* skip to swap usage line */
	cp++;
    while ( *cp != ' ' )		/* skip past 'Swap:' */
	cp++;
    if ( 1 != sscanf ( cp, "%*d %*d %ld", &freeswap ) ) {
	syslog ( LOG_ERR, "sscanf failed on swap info" );
	exit ( 1 );
    }

    return ( freemem + cache + freeswap );
}

/*
 * Add swapfile number i.
 */
int addswap ( int i )
{
    static  char    page [ PAGE_SIZE ];	/* one page of memory used to make swap file */

    struct  statfs  fsstat;
    int     fd;
    int     pages   = chunksz / sizeof(page);
    int     clean   = 0;

    if ( statfs ( tmpdir, &fsstat ) < 0 ) {
        syslog ( LOG_ERR, "statfs failed on \"%s\": %m", tmpdir );
        cleanup ();
        exit ( 1 );
    }
    if ( fsstat.f_bsize * fsstat.f_bavail < chunksz ) {
        syslog ( LOG_NOTICE, "no space for swapfile on \"%s\"", tmpdir );
	return 0;
    }

    strcpy ( swapfile[i], tmpdir );
    strcat ( swapfile[i], SUFFIX );
    if ( ( fd = mkstemp ( swapfile[i] ) ) < 0 ) {
	syslog ( LOG_ERR, "mkstemp failed: %m" );
	cleanup ();
	exit ( 1 );
    }

    /* add a swap signature */
    memcpy ( page + sizeof(page) - 10, "SWAP-SPACE", 10 );

    /* mark all pages as being valid */
    memset ( page, 0xFF, ( pages + 7 ) / 8 );
    page [ 0 ] &= 0xFE;
    if ( pages % 8 )
	page [ ( pages + 7 ) / 8 - 1 ] &= ( 0xFF >> ( 8 - ( pages % 8 ) ) );

    /* stick the pages for swapping onto disk */
    while ( pages-- > 0 ) {
        if ( write ( fd, page, sizeof(page) ) != sizeof(page) ) {
	    syslog ( LOG_NOTICE, "write failed on \"%s\": %m", swapfile[i] );
	    (void) close ( fd );
	    (void) unlink ( swapfile[i] );
	    return 0;
	}
	if ( ! clean++ )
	    memset ( page, 0, sizeof(page) );
    }
    if ( fsync ( fd ) < 0 ) {
	syslog ( LOG_ERR, "fsync failed on \"%s\": %m", swapfile[i] );
	close ( fd );
	(void) unlink ( swapfile[i] );
	return 0;
    }
    if ( close ( fd ) < 0 ) {
	syslog ( LOG_ERR, "fsync failed on \"%s\": %m", swapfile[i] );
	(void) unlink ( swapfile[i] );
	return 0;
    }

#ifdef DEBUG
    syslog ( LOG_INFO, "adding \"%s\" as swap", swapfile[i] );
#endif
    if ( swapon ( swapfile[i] ) < 0 ) {
	syslog ( LOG_ERR, "swapon failed on \"%s\": %m", swapfile[i] );
	(void) unlink ( swapfile[i] );
	return 0;
    }

    return 1;
}

/*
 * Remove swapfile number i.
 */
int delswap ( int i )
{
    if ( swapoff ( swapfile[i] ) < 0 ) {
	syslog ( LOG_ERR, "swapoff failed on \"%s\": %m", swapfile[i] );
	return 0;
    }
    if ( unlink ( swapfile[i] ) < 0 ) {
        syslog ( LOG_ERR, "unlink of \"%s\" failed: %m", swapfile[i] );
    }

#ifdef DEBUG
    syslog ( LOG_INFO, "removed \"%s\" as swap", swapfile[i] );
#endif
    return 1;
}
