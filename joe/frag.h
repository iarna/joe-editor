/* Byte code fragment construction functions */

typedef struct frag Frag;

struct frag {
	unsigned char *start;
	int len;
	int size;
	int align;
};

/* Initialize a fragment: 'alignment' sets the naturual alignment of
 * the fragment.  After any emit or fetch function is called, the
 * fragment is filled to the next multiple of 'alignment'.
 */

void iz_frag(Frag *, int alignmnet);

/* Generate byte offset you need to add to p so that
 * it is an exact multiple of size (which is a power of 2).
 */

#define align_o(p, size) (((size) - 1) & -(ptrdiff_t)(p))

/* Align frag to next multiple of n */

void align_frag(Frag *f, int n);

/* Append data to a fragment: return byte offset to data.
 * These do two alignments: one before and one after the emit.  Before,
 * it fills the fragment until its size is a multiple of the size of
 * the emitted data.  After the emit, it fills the fragment until its
 * size is a multiple of the natural alignment specified in iz_frag.
 */
 

int emitb_noalign(Frag *f, char c);
int emitb(Frag *f, char c);
int emith(Frag *f, short n);
int emiti(Frag *f, int n);
int emitd(Frag *f, double d);
int emitp(Frag *f, void *p);
int emits(Frag *f, unsigned char *s, int len);

int emit_branch(Frag *f, int target);
void fixup_branch(Frag *f, int pos);
void frag_link(Frag *f, int chain);

/* Access data in a fragment */

#define fragc(f, ofst) (*((f)->start + (ofst)))
#define fragh(f, ofst) (*(short *)((f)->start + (ofst)))
#define fragi(f, ofst) (*(int *)((f)->start + (ofst)))
#define fragd(f, ofst) (*(double *)((f)->start + (ofst)))
#define fragp(f, ofst) (*(void **)((f)->start + (ofst)))

/* Fetch an datum from a fragement and advance the "PC" */

int fetchi(Frag *f, int *pc);
short fetchh(Frag *f, int *pc);
void *fetchp(Frag *f, int *pc);
void fin_code(Frag *f);
