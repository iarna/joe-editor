/*
 *	Various utilities
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *		(C) 2001 Marek 'Marx' Grac
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if 0
int joe_ispunct(int wide,struct charmap *map,int c)
{
	if (joe_isspace(c))
		return 0;

	if (c=='_')
		return 1;

	if (isalnum_(wide,map,c))
		return 0;

	return joe_isprint(wide,map,c);
}
#endif

int escape(int utf8,unsigned char **a, int *b)
{
	int c;
	unsigned char *s = *a;
	int l = *b;

	if (*s == '\\' && l >= 2) {
		++s; --l;
		switch (*s) {
		case 'n':
			c = 10;
			++s; --l;
			break;
		case 't':
			c = 9;
			++s; --l;
			break;
		case 'a':
			c = 7;
			++s; --l;
			break;
		case 'b':
			c = 8;
			++s; --l;
			break;
		case 'f':
			c = 12;
			++s; --l;
			break;
		case 'e':
			c = 27;
			++s; --l;
			break;
		case 'r':
			c = 13;
			++s; --l;
			break;
		case '8':
			c = 8;
			++s; --l;
			break;
		case '9':
			c = 9;
			++s; --l;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			c = *s - '0';
			++s; --l;
			if (l > 0 && *s >= '0' && *s <= '7') {
				c = c * 8 + s[1] - '0';
				++s; --l;
			}
			if (l > 0 && *s >= '0' && *s <= '7') {
				c = c * 8 + s[1] - '0';
				++s; --l;
			}
			break;
		case 'x':
		case 'X':
			c = 0;
			++s; --l;
			if (l > 0 && *s >= '0' && *s <= '9') {
				c = c * 16 + *s - '0';
				++s; --l;
			} else if (l > 0 && *s >= 'A' && *s <= 'F') {
				c = c * 16 + *s - 'A' + 10;
				++s; --l;
			} else if (l > 0 && *s >= 'a' && *s <= 'f') {
				c = c * 16 + *s - 'a' + 10;
				++s; --l;
			}

			if (l > 0 && *s >= '0' && *s <= '9') {
				c = c * 16 + *s - '0';
				++s; --l;
			} else if (l > 0 && *s >= 'A' && *s <= 'F') {
				c = c * 16 + *s - 'A' + 10;
				++s; --l;
			} else if (l > 0 && *s >= 'a' && *s <= 'f') {
				c = c * 16 + *s - 'a' + 10;
				++s; --l;
			}
			break;
		default:
			if (utf8)
				c = utf8_decode_fwrd(&s, &l);
			else {
				c = *s++;
				--l;
			}
			break;
		}
	} else if (utf8) {
		c = utf8_decode_fwrd(&s,&l);
	} else {
		c = *s++;
		--l;
	}
	*a = s;
	*b = l;
	return c;
}

/*
 * return minimum/maximum of two numbers
 */
unsigned int uns_min(unsigned int a, unsigned int b)
{
	return a < b ? a : b;
}

signed int int_min(signed int a, signed int b)
{
	return a < b ? a : b;
}

signed long int long_max(signed long int a, signed long int b)
{
	return a > b ? a : b;
}

signed long int long_min(signed long int a, signed long int b)
{
	return a < b ? a : b;
}

#if 0
/* 
 * Characters which are considered as word characters 
 * 	_ is considered as word character because is often used 
 *	in the names of C/C++ functions
 */
int isalnum_(int wide,struct charmap *map,int c)
{
	/* Fast... */
	if (c>='0' && c<='9' ||
	    c>='a' && c<='z' ||
	    c>='A' && c<='Z' ||
	    c=='_')
	  return 1;
	else if(c<128)
	  return 0;

	/* Slow... */
	if (wide)
		return joe_iswalpha(c);
	else
		return joe_iswalpha(to_uni(map,c));
}

int isalpha_(int wide,struct charmap *map,int c)
{
	/* Fast... */
	if (c>='a' && c<='z' ||
	    c>='A' && c<='Z' ||
	    c=='_')
	  return 1;
	else if(c<128)
	  return 0;

	/* Slow... */
	if (wide)
		return joe_iswalpha(c);
	else
		return joe_iswalpha(to_uni(map,c));
}
#endif

/* Versions of 'read' and 'write' which automatically retry when interrupted */
ssize_t joe_read(int fd, void *buf, size_t size)
{
	ssize_t rt;

	do {
		rt = read(fd, buf, size);
	} while (rt < 0 && errno == EINTR);
	return rt;
}

ssize_t joe_write(int fd, void *buf, size_t size)
{
	ssize_t rt;

	do {
		rt = write(fd, buf, size);
	} while (rt < 0 && errno == EINTR);
	return rt;
}

int joe_ioctl(int fd, int req, void *ptr)
{
#ifdef JOEWIN
	return -1;
#else
	int rt;
	do {
		rt = ioctl(fd, req, ptr);
	} while (rt == -1 && errno == EINTR);
	return rt;
#endif
}

/* Heap checking versions of malloc() */

/* #define HEAP_CHECK 1 */

#ifdef HEAP_CHECK

struct mcheck {
	struct mcheck *next;
	int state;		/* 0=malloced, 1=free */
	int size;		/* size in bytes */
	int magic;
};

struct mcheck *first;
struct mcheck *last;

void check_malloc()
{
	struct mcheck *m;
	int y = 0;
	for(m=first;m;m=m->next) {
		unsigned char *ptr = (unsigned char *)m+sizeof(struct mcheck);
		int x;
		if(m->magic!=0x55AA55AA) {
			printf("corrupt heap: head %x\n",ptr);
			*(int *)0=0;
		}
		for(x=0;x!=16384;++x)
			if(ptr[x+m->size]!=0xAA) {
				printf("Corrupt heap: tail %x\n",ptr);
				*(int *)0=0;
			}
		if(m->state)
			for(x=0;x!=m->size;++x)
				if(ptr[x]!=0x41) {
					printf("Corrupt heap: modified free block %x\n",ptr);
					*(int *)0=0;
				}
		++y;
	}
}

void *joe_malloc(size_t size)
{
	struct mcheck *m = (struct mcheck *)malloc(size+sizeof(struct mcheck)+16384);
	unsigned char *ptr = (unsigned char *)m+sizeof(struct mcheck);
	int x;
	m->state = 0;
	m->size = size;
	m->magic=0x55AA55AA;
	m->next = 0;

	if(!size) {
		printf("0 passed to malloc\n");
		*(int *)0=0;
	}
	for(x=0;x!=16384;++x)
		ptr[x+size]=0xAA;
	if(first) {
		last->next = m;
		last = m;
	} else {
		first = last = m;
	}
	check_malloc();
	/* printf("malloc size=%d addr=%x\n",size,ptr); fflush(stdout); */
	return ptr;
}

void *joe_calloc(size_t nmemb, size_t size)
{
	size_t sz = nmemb*size;
	int x;
	unsigned char *ptr;
	ptr = joe_malloc(sz);
	for(x=0;x!=sz;++x)
		ptr[x] = 0;
	return ptr;
}

void *joe_realloc(void *ptr, size_t size)
{
	struct mcheck *n;
	unsigned char *np;
	struct mcheck *m = (struct mcheck *)((char *)ptr-sizeof(struct mcheck));

	if(!size) {
		printf("0 passed to realloc\n");
		*(int *)0=0;
	}

	np = joe_malloc(size);

	n = (struct mcheck *)(np-sizeof(struct mcheck));

	if(m->size>size)
		memcpy(np,ptr,size);
	else
		memcpy(np,ptr,m->size);

	joe_free(ptr);

	return np;
}

void joe_free(void *ptr)
{
	struct mcheck *m = (struct mcheck *)((char *)ptr-sizeof(struct mcheck));
	int x;
	if (m->magic!=0x55AA55AA) {
		printf("Free non-malloc block %x\n",ptr);
		*(int *)0=0x0;
	}
	if (m->state) {
		printf("Double-free %x\n",ptr);
		*(int *)0=0x0;
	}
	for(x=0;x!=m->size;++x)
		((unsigned char *)ptr)[x]=0x41;
	m->state = 1;
	check_malloc();
	/* printf("free=%x\n",ptr); */
}

#else

void *joe_malloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		ttsig(-1);
	return p;
}

void *joe_calloc(size_t nmemb,size_t size)
{
	void *p = calloc(nmemb, size);
	if (!p)
		ttsig(-1);
	return p;
}

void *joe_realloc(void *ptr,size_t size)
{
	void *p = realloc(ptr, size);
	if (!p)
		ttsig(-1);
	return p;
}

void joe_free(void *ptr)
{
	free(ptr);
}

#endif

size_t zlen(unsigned char *s)
{
	return strlen((char *)s);
}

int zcmp(unsigned char *a, unsigned char *b)
{
	return strcmp((char *)a, (char *)b);
}

int zncmp(unsigned char *a, unsigned char *b, size_t len)
{
	return strncmp((char *)a, (char *)b, len);
}

unsigned char *zdup(unsigned char *bf)
{
	int size = zlen(bf);
	unsigned char *p = (unsigned char *)joe_malloc(size+1);
	memcpy(p,bf,size+1);
	return p;
}

#if 0
unsigned char *zcpy(unsigned char *a, unsigned char *b)
{
	strcpy((char *)a,(char *)b);
	return a;
}
#endif

unsigned char *zstr(unsigned char *a, unsigned char *b)
{
	return (unsigned char *)strstr((char *)a,(char *)b);
}

unsigned char *zncpy(unsigned char *a, unsigned char *b, size_t len)
{
	strncpy((char *)a,(char *)b,len);
	return a;
}

unsigned char *zlcpy(unsigned char *a, size_t len, unsigned char *b)
{
	unsigned char *org = a;
	if (!len) {
		fprintf(stderr, "zlcpy called with len == 0\n");
		exit(1);
	}
	--len;
	while (len && *b) {
		*a++ = *b++;
		--len;
	}
	*a = 0;
	return org;
}

unsigned char *mcpy(unsigned char *a, unsigned char *b, size_t len)
{
	if (len)
		memcpy(a, b, len);
	return a;
}

#if 0
unsigned char *zcat(unsigned char *a, unsigned char *b)
{
	strcat((char *)a,(char *)b);
	return a;
}
#endif

unsigned char *zlcat(unsigned char *a, size_t siz, unsigned char *b)
{
	unsigned char *org = a;
	while (*a && siz) {
		++a;
		--siz;
	}
	zlcpy(a, siz, b);
	return org;
}

unsigned char *zchr(unsigned char *s, int c)
{
	return (unsigned char *)strchr((char *)s,c);
}

unsigned char *zrchr(unsigned char *s, int c)
{
	return (unsigned char *)strrchr((char *)s,c);
}

int filecmp(unsigned char* s1, unsigned char* s2)
{
#ifndef JOEWIN
	return zcmp(s1, s2);
#else
	if (!s1 || !s2)
	{
		if (!s1 && !s2)
		{
			return 0;
		}

		return s1 ? 1 : -1;
	}

	for (;; s1++, s2++)
	{
		unsigned char c1 = *s1, c2 = *s2;
		
		if (!c1 || !c2)
		{
			if (!c1 && !c2)
			{
				return 0;
			}

			return c1 ? 1 : -1;
		}

		if (c1 == c2)
		{
			/* Pass */
		}
		else if ((c1 == '\\' || c1 == '/') && (c2 == '\\' || c2 == '/'))
		{
			/* Pass */
		}
		else if (tolower(c1) == tolower(c2))
		{
			/* Pass */
		}
		else if (c1 < c2)
		{
			return -1;
		}
		else
		{
			return 1;
		}
	}
#endif
}

int fullfilecmp(unsigned char *f1, unsigned char *f2)
{
#ifndef JOEWIN
	return zcmp(f1, f2);
#else
	wchar_t wf1[MAX_PATH + 1], wf2[MAX_PATH + 1];

	if (utf8towcs(wf1, f1, MAX_PATH) || utf8towcs(wf2, f2, MAX_PATH))
	{
		/* Can't convert! */
		assert(FALSE);
		return filecmp(f1, f2);
	}

	if (fixpath(wf1, MAX_PATH) || fixpath(wf2, MAX_PATH))
	{
		return filecmp(f1, f2);
	}

	/* Is this really the way to compare filenames in every locale? */
	return wcsicmp(wf1, wf2);
#endif
}

#ifndef SIG_ERR
#define SIG_ERR ((sighandler_t) -1)
#endif

/* wrapper to hide signal interface differrencies */
int joe_set_signal(int signum, sighandler_t handler)
{
	int retval;
#ifdef HAVE_SIGACTION
	struct sigaction sact;

	mset(&sact, 0, sizeof(sact));
	sact.sa_handler = handler;
#ifdef SA_INTERRUPT
	sact.sa_flags = SA_INTERRUPT;
#endif
	retval = sigaction(signum, &sact, NULL);
#elif defined(HAVE_SIGVEC)
	struct sigvec svec;

	mset(&svec, 0, sizeof(svec));
	svec.sv_handler = handler;
#ifdef HAVE_SV_INTERRUPT
	svec.sv_flags = SV_INTERRUPT;
#endif
	retval = sigvec(signum, &svec, NULL);
#else
	retval = (signal(signum, handler) != SIG_ERR) ? 0 : -1;
#ifdef HAVE_SIGINTERRUPT
	siginterrupt(signum, 1);
#endif
#endif
	return(retval);
}

/* Helpful little parsing utilities */

/* Skip whitespace and return first non-whitespace character */

int parse_ws(unsigned char **pp,int cmt)
{
	unsigned char *p = *pp;
	while (*p==' ' || *p=='\t')
		++p;
	if (*p=='\r' || *p=='\n' || *p==cmt)
		*p = 0;
	*pp = p;
	return *p;
}

/* Parse an identifier into a buffer.  Identifier is truncated to a maximum of len-1 chars. */

int parse_ident(unsigned char **pp, unsigned char **buf)
{
	unsigned char *p = *pp;
	unsigned char *bf = *buf;
	bf = vstrunc(bf, 0);
	if (joe_isalpha_(locale_map,*p)) {
		while(joe_isalnum_(locale_map,*p)) {
			bf = vsadd(bf, *p++);
		}
		while(joe_isalnum_(locale_map,*p))
			++p;
		*pp = p;
		*buf = bf;
		return 0;
	} else {
		*buf = bf;
		return -1;
	}
}

/* Parse to next whitespace */

int parse_tows(unsigned char **pp, unsigned char **bf)
{
	unsigned char *buf = *bf;
	unsigned char *p = *pp;
	buf = vstrunc(buf, 0);
	while (*p && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r' && *p!='#')
		buf = vsadd(buf, *p++);

	*pp = p;
	*bf = buf;
	return 0;
}

/* Parse over a specific keyword */

int parse_kw(unsigned char **pp, unsigned char *kw)
{
	unsigned char *p = *pp;
	while(*kw && *kw==*p)
		++kw, ++p;
	if(!*kw && !joe_isalnum_(locale_map,*p)) {
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse a field (same as parse_kw, but string must be terminated with whitespace) */

int parse_field(unsigned char **pp, unsigned char *kw)
{
	unsigned char *p = *pp;
	while(*kw && *kw==*p)
		++kw, ++p;
	if(!*kw && (!*p || *p==' ' || *p=='\t' || *p=='#' || *p=='\n' || *p=='\r')) {
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse a specific character */

int parse_char(unsigned char **pp, unsigned char c)
{
	unsigned char *p = *pp;
	if (*p == c) {
		*pp = p+1;
		return 0;
	} else
		return -1;
}

/* Parse an integer.  Returns 0 for success. */

int parse_int(unsigned char **pp, int *buf)
{
	unsigned char *p = *pp;
	if ((*p>='0' && *p<='9') || *p=='-') {
		*buf = atoi((char *)p);
		if(*p=='-')
			++p;
		while(*p>='0' && *p<='9')
			++p;
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse a long */

int parse_long(unsigned char **pp, long *buf)
{
	unsigned char *p = *pp;
	if ((*p>='0' && *p<='9') || *p=='-') {
		*buf = atol((char *)p);
		if(*p=='-')
			++p;
		while(*p>='0' && *p<='9')
			++p;
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse a string of the form "xxxxx" into a fixed-length buffer.  The
 * address of the buffer is 'buf'.  The length of this buffer is 'len'.  A
 * terminating NUL is added to the parsed string.  If the string is larger
 * than the buffer, the string is truncated.
 *
 * C string escape sequences are handled.
 *
 * 'p' holds an address of the input string pointer.  The pointer
 * is updated to point right after the parsed string if the function
 * succeeds.
 *
 * Returns the length of the string (not including the added NUL), or
 * -1 if there is no string or if the input ended before the terminating ".
 */

int parse_string(unsigned char **pp, unsigned char **dst)
{
	unsigned char *start = vstrunc(*dst, 0);
	unsigned char *p= *pp;
	if(*p=='\"') {
		++p;
		while(*p && *p!='\"') {
			int x = 50;
			int c = escape(0, &p, &x);
			start = vsadd(start, c);
		}
		if(*p == '\"') {
			*pp = p + 1;
			*dst = start;
			return vslen(start);
		}
	}
	*dst = 0;
	return -1;
}

/* Emit a string with escape sequences */

#ifdef junk

/* Used originally for printing macros */

void emit_string(FILE *f,unsigned char *s,int len)
{
	unsigned char buf[8];
	unsigned char *p, *q;
	fputc('\"',f);
	while(len) {
		p = unescape(buf,*s++);
		for(q=buf;q!=p;++q)
			fputc(*q,f);
		--len;
	}
	fputc('\"',f);
}
#endif

/* Emit a string */

void emit_string(FILE *f,unsigned char *s,int len)
{
	fputc('"',f);
	while(len) {
		if (*s=='"' || *s=='\\')
			fputc('\\',f), fputc(*s,f);
		else if(*s=='\n')
			fputc('\\',f), fputc('n',f);
		else if(*s=='\r')
			fputc('\\',f), fputc('r',f);
		else if(*s==0)
			fputc('\\',f), fputc('0',f), fputc('0',f), fputc('0',f);
		else
			fputc(*s,f);
		++s;
		--len;
	}
	fputc('"',f);
}

/* Parse a character range: a-z */

int parse_range(unsigned char **pp, int *first, int *second)
{
	unsigned char *p= *pp;
	int a, b;
	if(!*p)
		return -1;
	if(*p=='\\' && p[1]) {
		++p;
		if(*p=='n')
			a = '\n';
		else if(*p=='t')
  			a = '\t';
		else
			a = *p;
		++p;
	} else
		a = *p++;
	if(*p=='-' && p[1]) {
		++p;
		if(*p=='\\' && p[1]) {
			++p;
			if(*p=='n')
				b = '\n';
			else if(*p=='t')
				b = '\t';
			else
				b = *p;
			++p;
		} else
			b = *p++;
	} else
		b = a;
	*first = a;
	*second = b;
	*pp = p;
	return 0;
}
