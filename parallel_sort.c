/* main.c
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


typedef int t_t;
void heapize(t_t *arr, size_t size);
void select_big(t_t *f, t_t *s, t_t *t, size_t sz);
void select_small(t_t *f, t_t *s, t_t *t, size_t sz);
t_t *mergewrap(t_t *arr, size_t size);
void mergesort(t_t *arr, t_t *wsp, size_t st, size_t en, char flag);

#ifndef MAX_HEAP_SORT
#define MAX_HEAP_SORT 1000
#endif // MAX_HEAP_SORT

// debug output
#define oint(val) fprintf(stderr, "%s = %d ", #val, val)
#define ollu(val) fprintf(stderr, "%s = %lu ", #val, val)
#define out(...) do{ fprintf(stderr, ##__VA_ARGS__); fputs("\n", stderr);}while(0)
#define fail(...) do{out(__VA_ARGS__); exit(EXIT_FAILURE); }while(0)

#define print(...) do{ printf(##__VA_ARGS__); fputs("\n", stdout);}while(0)

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
	// get array
	t_t *src, *dst;
	size_t phis_sz = 0;

	if (argc < 3) {
		fail("Not enough arguments provided, aborting...");
	}

	int ifh, ofh;
	ifh = open(argv[1], O_RDONLY, NULL);
	ofh = open(argv[2], O_WRONLY | O_CREAT, 0644);
	if (ifh < 0 || ofh < 0)
		fail("Cant open files");

	if (0 > read(ifh, &phis_sz, sizeof(int)))
		fail("Can't read from file");

	printf(	"OpenMP Merge sort\n"
		"=================\n"
		"\n"
		"Input file: \t%s\n"
		"Output file: \t%s\n"
		"Array size: \t%lu \n"
		"\n", 
		argv[1], argv[2], phis_sz);

	src = (t_t *)malloc(sizeof(*src) * phis_sz);
	dst = (t_t *)malloc(sizeof(*dst) * phis_sz);
	if (!src || !dst) 
		fail("Can't allocate memory");

	if (0 > read(ifh, src, sizeof(*src) * phis_sz))
		fail("Can't read array");
	
	double timer = omp_get_wtime();

#pragma omp parallel shared(src, dst, phis_sz)
#pragma omp single nowait
{
	mergesort(src, dst, 0, phis_sz, 0);
}

	timer = omp_get_wtime() - timer;

	out("Sort completed");
	out("Timelaps: %lg", timer);

	// check
	if (chk_sorted(dst, phis_sz))
		out("Sort check passed");
	else
		out("FAILURE: disorder detected");
	
	if (0 > write(ofh, dst, sizeof(int) * phis_sz))
		fail("Couldn't write to file");

	close(ifh);
	close(ofh);
	
	free(src);
	free(dst);

	return 0;
}

// selects max in tree-like structure
int maxarr3(t_t *arr, int p)
{
	register int l = 2 * p;
	register int r = l + 1;
	l = (arr[l] > arr[r] ? l : r);
	return (arr[l] > arr[p] ? l : p); 
}

void swap(t_t *arr, int i, int j)
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

void merge2(t_t *f, size_t s, size_t m, size_t e, t_t *t)
{
	size_t i = m;
	while (s < m && i < e)
		*t++ = (f[s] < f[i] ? f[s++] : f[i++]);
	while (s < m)
		*t++ = f[s++];
	while (i < e)
		*t++ = f[i++];
}

void mergesort(t_t *arr, t_t *wsp, size_t st, size_t en, char flag) 
{
	// out("sorting from %lu to %lu (flag = %d)", st, en, flag);
	if (en <= 1 + st) {
		return;
	} else if ((en - st) < MAX_HEAP_SORT && flag) {
//		#pragma omp task
		heapize(wsp + st, en - st);
	} else {
		size_t mid = (st + en) / 2;
		/* 
		 * Ok, this part is tricky.
		 *
		 * To use O(n) extra memory this function takes two arrays, instead of one.
		 * arr -- is array with input data, and wsp -- output array.
		 *
		 * But inside mergesort they will swap their roles with the depth of call.
		 * E.g. in the initial call (depth == 0) wsp will be merged from halfs, stored in arr;
		 * and halfs in arr (depth == 1) will be merged from quorters, stored in wsp; and so on.
		 *
		 * On the lowest layer, heapsort is used. The flag argument shows 
		 * where actual data is stored on the way down.
		 */
		#pragma omp task
		mergesort(wsp, arr, st, mid, !flag);
		#pragma omp task
		mergesort(wsp, arr, mid, en, !flag);
		#pragma omp taskwait
		merge2(arr, st, mid, en, wsp + st);
	}

	/*
	 * if (!chk_sorted(wsp + st, en - st)) {
	 *         out("------------------------\nsomething went wrong");
	 *         ollu(st), ollu(en);
	 *         dump_arr(wsp + st, en - st);
	 *         fail("stopping");
	 * } else {
	 *         out("midcheck passed %lu - %lu", st, en);
	 * }
	 */
}

t_t *mergewrap(t_t *arr, size_t size)
{
	t_t *wsp = (t_t *)malloc(size * sizeof(t_t));
	#pragma omp parallel
	#pragma omp single nowait
	mergesort(arr, wsp, 0, size, 0);
	return wsp;
}

