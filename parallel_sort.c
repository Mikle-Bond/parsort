/* main.c
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef int t_t;
void heapize(t_t *arr, size_t size);
void select_big(t_t *f, t_t *s, t_t *t, size_t sz);
void select_small(t_t *f, t_t *s, t_t *t, size_t sz);

#if 0 // here is stuff I thought can be helpful

#define max2(x, y) (x ^ ((x ^ y) & -(x < y)))
#define min2(x, y) (y ^ ((x ^ y) & -(x < y)))

inline static my_log2(int x)
{
	// this works for x < 2^24 and uses a hack with floatpoint math
	typedef union { float f; unsigned u; } t; 
	
	// 'm' stands for 'magic constant' and represents 2^23 in float
	static t m; m.u = 0x4B000000;
	register t a;
	
	// we put x to mantissa of magic constant, and than substract normalized 1.0
	// that makes the float number to shift mantissa left, till it finds new 1
 	a.u = x | m.u;
	a.f -= m.f;
	
	// after that exponen becomes 23 - (position of MSB counting from left) + (float bias)
	// where bias is 0x7F = 128
	return (a.u >> 23) - 0x7f;
} 

#endif // 0

// debug output
#define out(r, ...) do{fprintf(stderr, "%d: ", r); fprintf(stderr, ##__VA_ARGS__); fputs("\n", stderr);}while(0)
#define oint(val) fprintf(stderr, "%s = %d ", #val, val)

// MPI file routine
#define reed(where, sz) MPI_File_read_shared(ifh, where, sz, MPI_INT, MPI_STATUS_IGNORE)
#define wrrt(from, sz) MPI_File_write_shared(ofh, from,  sz, MPI_INT, MPI_STATUS_IGNORE)

// MPI send/recieve wrappers
#define tag(from, to) 0x0FFFFFFF & (((from) << 12) | ((to) & 0xffff)) 
#define snd(what, sz, to) MPI_Send(what, sz, MPI_INT, to, tag(rank, to), MPI_COMM_WORLD)
#define rcv(what, sz, to) MPI_Recv(what, sz, MPI_INT, to, tag(to, rank), MPI_COMM_WORLD, MPI_STATUS_IGNORE)

#define fail(...) do{out(__VA_ARGS__); MPI_Abort(MPI_COMM_WORLD, -1); MPI_Finalize(); }while(0)

#if 0
#define onl puts("")
#define snd(what, sz, to) warnx("%d: snd (%d->%d), t=%8i", rank, rank, to, tag(rank, to)); dsnd(what, sz, to)
#define rcv(what, sz, to) warnx("%d: rcv (%d->%d), t=%8i", rank, to, rank, tag(to, rank)); drcv(what, sz, to)
#endif

// debug array dumping
void dump_arr(t_t *arr, size_t sz) {
	size_t i; 
	for (i = 0; i < sz; i += 1) {
		printf("%d ", arr[i]);
	}
	printf("\n");
	fflush(stdout);
}

// check array to be sorted
int chk_sorted(t_t *arr, size_t sz) {
	size_t i;
	for (i = 0; i < sz - 1; i += 1)
		if (arr[i] > arr[i+1])
			return 0;
	return 1;
}

int main(int argc, char *argv[])
{
	// initialize MPI
	MPI_Init(&argc, &argv);
	unsigned rank;
	unsigned np;
	MPI_Comm_rank(MPI_COMM_WORLD, (int*)&rank);
	MPI_Comm_size(MPI_COMM_WORLD, (int*)&np);
//	warnx("Initialized: %d/%d", rank, np);
	
	// get array
	t_t *frst, *scnd, *thrd;
	size_t size = 0;

	if (argc < 3) {
		fail(rank, "No name to read from, aborting...");
	}

	MPI_File ifh, ofh;
	MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_RDONLY, MPI_INFO_NULL, &ifh);
	MPI_File_open(MPI_COMM_WORLD, argv[2], MPI_MODE_CREATE|MPI_MODE_WRONLY, MPI_INFO_NULL, &ofh);

	if (rank == 0) {
		// first process reads size form file
		reed(&size, 1);
	} else {
		// ... and the others will get it one-by-one
		rcv(&size, 1, rank - 1);
	}

	size_t part_sz = size / np + !!(size % np); // this might be bigger then needed
	size_t real_sz = size / np + !!(rank < (size % np)); // used only after sorting
	
	frst = (t_t *)malloc(sizeof(*frst) * part_sz);
	scnd = (t_t *)malloc(sizeof(*scnd) * part_sz);
	thrd = (t_t *)malloc(sizeof(*thrd) * part_sz);

	if (!(frst && scnd && thrd)) {
		fail(rank, "Allocation failure");
	}
	
//	warnx("%d: allocation successful, part_sz = %d", rank, part_sz);
	if (part_sz > real_sz)
		frst[real_sz] = INT_MAX;

	reed(frst, real_sz);

	//out(rank, "scanf:"), dump_arr(frst, part_sz);
	
	if (rank != np - 1) {
		snd(&size, 1, rank + 1);
	}
MPI_Barrier(MPI_COMM_WORLD);
	
	double timer = MPI_Wtime();

	// sort part
	heapize(frst, part_sz);
//	warnx("%d: sort done, checking", rank);
//	if (chk_sorted(frst, part_sz))
//		out(rank, "sort successful");
//	else
//		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);

	unsigned mask = 0x1;
	unsigned to;
	t_t *t = NULL;
	#define swp(a, b) t = a, a = b, b = t
	while(mask < np) {
		// first iteration, one half treated as reversed
		unsigned mext = (mask | (mask-1)); // mask extended with 1 til right corner
		to = (rank & ~mext) | (mext & ~rank); // rank with reversed tail
		if (to < np) {
		//	oint(mask); oint(rank), oint(to); out(rank, " rev comp");
			if (to > rank) {
				snd(frst, part_sz, to);
				rcv(scnd, part_sz, to);
				select_small(frst, scnd, thrd, part_sz);
			} else {
				rcv(scnd, part_sz, to);
				snd(frst, part_sz, to);
				select_big(frst, scnd, thrd, part_sz);
			}

			swp(thrd, frst);
		}
	//	if (chk_sorted(frst, part_sz))
	//		out(rank, "sort successful after (%d, %d)", rank, to);

//	MPI_Barrier(MPI_COMM_WORLD);
		unsigned m = mask;
		while (m >>= 1) {
			to = rank ^ m;
			if (to >= np)
				continue;

		//	oint(mask); oint(m); oint(rank), oint(to); out(rank, "norm comp");
			if (to > rank) {
				snd(frst, part_sz, to);
				rcv(scnd, part_sz, to);
				select_small(frst, scnd, thrd, part_sz);
			} else {
				rcv(scnd, part_sz, to);
				snd(frst, part_sz, to);
				select_big(frst, scnd, thrd, part_sz);
			}
			swp(thrd, frst);
	//		if (chk_sorted(frst, part_sz))
	//			out(rank, "sort successful after (%d, %d)", rank, to);

//		MPI_Barrier(MPI_COMM_WORLD);
		}
		mask <<= 1;
	}
	#undef swp

MPI_Barrier(MPI_COMM_WORLD);
	timer = MPI_Wtime() - timer;

	out(rank, "bitonic sort completed");
	if (!rank) 
		out(rank, "timelaps: %lg", timer);

	// check
	if (chk_sorted(frst, part_sz))
		out(rank, "Inner sort passed");
	else
		out(rank, "FAILURE: disorder inside part");
	
	// recalc amount of real elements for output 
	if (rank < size / part_sz) {
		real_sz = part_sz;
	} else if (rank == size / part_sz) {
		real_sz = size % part_sz;
	} else {
		real_sz = 0;
	}


	// Get last element of previous part
	t_t i = 0;
	if (rank == 0)
		i = INT_MIN;
	else
		rcv(&i, 1, rank - 1);

	if (i > frst[0]) {
		out(rank, "FAILURE: disorder between parts: %d on previous, %d here", i, frst[0]);
	} else {
		out(rank, "Inbetween sort passed");
	}

	// dump_arr(frst, part_sz);
	if (real_sz)
		wrrt(frst, real_sz);
	
	if (rank != np - 1)
		snd(frst + part_sz - 1, 1, rank + 1);

	
	free(frst);
	free(scnd);
	free(thrd);

	MPI_File_close(&ifh);
	MPI_File_close(&ofh);

	MPI_Finalize();
	return 0;
}

void select_small(t_t *f, t_t *s, t_t *t, size_t sz) 
{
	size_t i;

	for (i = 0; i < sz; i += 1) {
		if (*f < *s) {
			*t = *f;
			++f;
		} else {
			*t = *s;
			++s;
		}
		++t;
	}
}

void select_big(t_t *f, t_t *s, t_t *t, size_t sz) 
{
	size_t i;
	f += sz - 1;
	s += sz - 1;
	t += sz - 1;

	for (i = 0; i < sz; i += 1) {
		if (*f > *s) {
			*t = *f;
			--f;
		} else {
			*t = *s;
			--s;
		}
		--t;
	}
}

// selects max in tree-like structure
inline int maxarr3(t_t *arr, int p)
{
	register int l = 2 * p;
	register int r = l + 1;
	l = (arr[l] > arr[r] ? l : r);
	return (arr[l] > arr[p] ? l : p); 
}

inline void swap(t_t *arr, int i, int j)
{
	t_t t = arr[i]; arr[i] = arr[j]; arr[j] = t;
}

// Pulls element t down into the tree, till it fits.
// Note, that arr is normalized to have elemet's order 
// to start from 1.
void pull_down(t_t *arr, size_t size, size_t t)
{
	size_t p;
	while (t * 2 < size) {
		// here the left child has a right brother
		
		p = maxarr3(arr, t);
		if (p == t)
			break;
//		arr[t] = arr[t]; 
		swap(arr, p, t);
		t = p;
	}
	if (2 * t == size) {
		// here the left child is the last element
		// e.g. there is no right child
		
		if (arr[t] < arr[size])
			swap(arr, t, size);
	}
}

void heapize(t_t *arr, size_t size)
{
	arr -= 1;
	// to make our elements count from 1
	// note that arr[0] is segfalt, but arr[size] makes sence
	
	// find the deepest parent
	size_t p = size >> 1;
	
	if ( (size & 0x1u) == 0) {
		// this is the parent with only left child
		// process it separately
		if (arr[p] < arr[size]) 
			swap(arr, p, size);
		p -= 1;
	}
	
	while (p) {
		pull_down(arr, size, p);
		p -= 1;
	} 
	
	while (size > 1) {
		// TODO: Here the optimization is possible, where we use 
		//       additional variable on stack instead of memory access
		//       See commented parts of code below
	//	t_t x = arr[1];
	//	arr[1] = arr[size]; 
		swap(arr, 1, size);
		size -= 1;
		pull_down(arr, size, 1);
	}
}


