#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#	include <OpenCL/opencl.h>
#else
#	include <CL/cl.h>
#endif

#define DEFAULT_INTENSITY (24u)
#define DEFAULT_LOCAL_WORK_SIZE (256)

#ifndef MIN
#	define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifdef EBUG
#	define STR(a) #a
#	define XSTR(a) STR(a)
#	define WRITE(a, b) write(a, b, sizeof(b) - 1)
#	define ASSERT(a)                                                                            \
		do {                                                                                    \
			if(__builtin_expect(!(a), 0)) {                                                     \
				WRITE(STDERR_FILENO, "(" __FILE__ ":" XSTR(__LINE__) "): !(\"" STR(a) "\")\n"); \
				exit(EXIT_FAILURE);                                                             \
			}                                                                                   \
		} while(0)
#else
#	define ASSERT(a) ((void)(a))
#endif

static inline __attribute__((always_inline)) uint32_t
rotl32(const uint32_t a, const unsigned b) {
	return (a << b) | (a >> (32u - b));
}

static inline void
get_device(cl_device_id *device) {
	cl_uint n_platforms = 0, i, pid_idx = 0, n_devices = 0, did_idx = 0;
	cl_platform_id *pid;
	cl_device_id *did;
	size_t vendor_name_sz = 0, device_name_sz = 0;
	char *vendor_name, *device_name;
	
	ASSERT(clGetPlatformIDs(0, NULL, &n_platforms) == CL_SUCCESS);
	ASSERT(n_platforms);
	ASSERT((pid = malloc(sizeof(*pid) * n_platforms)));
	ASSERT(clGetPlatformIDs(n_platforms, pid, NULL) == CL_SUCCESS);
	
	for(i = 0; i < n_platforms; ++i) {
		ASSERT(clGetPlatformInfo(pid[i], CL_PLATFORM_VENDOR, 0, NULL, &vendor_name_sz) == CL_SUCCESS);
		ASSERT((vendor_name = malloc(vendor_name_sz)));
		ASSERT(clGetPlatformInfo(pid[i], CL_PLATFORM_VENDOR, vendor_name_sz, vendor_name, NULL) == CL_SUCCESS);
		printf("Index: %d Vendor: %s\n", i, vendor_name);
		free(vendor_name);
	}
	
	printf("Select platform: ");
	ASSERT(scanf("%u", &pid_idx) == 1);
	ASSERT(pid_idx < n_platforms);
	ASSERT(clGetDeviceIDs(pid[pid_idx], CL_DEVICE_TYPE_GPU, 0, NULL, &n_devices) == CL_SUCCESS);
	ASSERT(n_devices);
	ASSERT((did = malloc(sizeof(*did) * n_devices)));
	ASSERT(clGetDeviceIDs(pid[pid_idx], CL_DEVICE_TYPE_GPU, n_devices, did, NULL) == CL_SUCCESS);
	free(pid);
	
	for(i = 0; i < n_devices; ++i) {
		ASSERT(clGetDeviceInfo(did[i], CL_DEVICE_NAME, 0, NULL, &device_name_sz) == CL_SUCCESS);
		ASSERT((device_name = malloc(device_name_sz)));
		ASSERT(clGetDeviceInfo(did[i], CL_DEVICE_NAME, device_name_sz, device_name, NULL) == CL_SUCCESS);
		printf("Index: %d Device Name: %s\n", i, device_name);
		free(device_name);
	}
	
	printf("Select device: ");
	ASSERT(scanf("%u", &did_idx) == 1);
	ASSERT(did_idx < n_devices);
	*device = did[did_idx];
	free(did);
}

static inline void
build_program(cl_context ctx, const char *file_name, cl_program *program) {
	int fd;
	size_t len;
	void *m;
	const char *src;
	
	fd = open(file_name, O_RDONLY);
	len = (size_t)lseek(fd, 0, SEEK_END);
	m = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	ASSERT(m != MAP_FAILED);
	src = (const char *)m;
	ASSERT((*program = clCreateProgramWithSource(ctx, 1, &src, NULL, NULL)));
	munmap(m, len);
	ASSERT(clBuildProgram(*program, 0, NULL, NULL, NULL, NULL) == CL_SUCCESS);
}

static inline void
random_bytes(void *buf, const size_t len) {
	int rand_fd = open("/dev/urandom", O_RDONLY);
	ASSERT(read(rand_fd, buf, len) != -1);
	close(rand_fd);
}

int
main(int argc, const char **argv)
{
	struct timespec start, end;
	size_t max_group_size, local_work_size, global_work_size = 1u << DEFAULT_INTENSITY;
	cl_device_id device;
	cl_context ctx;
	cl_command_queue queue;
	cl_mem nonce_obj, n_obj;
	cl_program program;
	cl_kernel kernel;
	cl_uint nonce = 0, w0 = 0, w1 = 0, w16, w19, w22, w24, w28, w30, n[5];
	
	switch(argc) {
		case 2:
			random_bytes(&w0, sizeof(w0));
			printf("Random w0: 0x%08x\n", w0);
			break;
		case 3:
			ASSERT(sscanf(argv[2], "%x", &w0) == 1);
			break;
		default:
			printf("Usage: %s SHA-1 w0\n", argv[0]);
			return 0;
	}
		
	ASSERT(sscanf(argv[1], "%08x%08x%08x%08x%08x", &(n[0]), &(n[1]), &(n[2]), &(n[3]), &(n[4])) == 5);
	n[0] -= 0x67452301u;
	n[1] -= 0xefcdab89u;
	n[2] -= 0x98badcfeu;
	n[3] -= 0x10325476u;
	n[4] -= 0xc3d2e1f0u;
	
	get_device(&device);
	ASSERT(clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(max_group_size), &max_group_size, NULL) == CL_SUCCESS);
	local_work_size = MIN(DEFAULT_LOCAL_WORK_SIZE, max_group_size);
	
	ASSERT((ctx = clCreateContext(NULL, 1, &device, NULL, NULL, NULL)));
	build_program(ctx, "brutenonce.cl", &program);
	ASSERT((nonce_obj = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, sizeof(nonce), &nonce, NULL)));
	ASSERT((n_obj = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(n), &n, NULL)));
	
#ifndef CL_VERSION_2_0
	ASSERT((queue = clCreateCommandQueue(ctx, device, 0, NULL)));
#else
	ASSERT((queue = clCreateCommandQueueWithProperties(ctx, device, 0, NULL)));
#endif
	
	ASSERT(clReleaseContext(ctx) == CL_SUCCESS);
	
	ASSERT((kernel = clCreateKernel(program, "brutenonce", NULL)));
	ASSERT(clReleaseProgram(program) == CL_SUCCESS);
	ASSERT(clSetKernelArg(kernel, 0, sizeof(nonce_obj), &nonce_obj) == CL_SUCCESS);
	ASSERT(clSetKernelArg(kernel, 1, sizeof(n_obj), &n_obj) == CL_SUCCESS);

	do {
		ASSERT(clSetKernelArg(kernel, 2, sizeof(w0), &w0) == CL_SUCCESS);
		w16 = rotl32(w0 ^ 0x80000000u, 1u);
		ASSERT(clSetKernelArg(kernel, 3, sizeof(w16), &w16) == CL_SUCCESS);
		w19 = rotl32(w16, 1u);
		ASSERT(clSetKernelArg(kernel, 4, sizeof(w19), &w19) == CL_SUCCESS);
		w22 = rotl32(w19, 1u);
		ASSERT(clSetKernelArg(kernel, 5, sizeof(w22), &w22) == CL_SUCCESS);
		w24 = w19 ^ 0x204u;
		ASSERT(clSetKernelArg(kernel, 6, sizeof(w24), &w24) == CL_SUCCESS);
		w28 = rotl32(w22, 2u);
		ASSERT(clSetKernelArg(kernel, 7, sizeof(w28), &w28) == CL_SUCCESS);
		w30 = rotl32(0x408u ^ w22 ^ w16, 1u);
		ASSERT(clSetKernelArg(kernel, 8, sizeof(w30), &w30) == CL_SUCCESS);
		ASSERT(clock_gettime(CLOCK_MONOTONIC, &start) != -1);
		do {
			ASSERT(clSetKernelArg(kernel, 9, sizeof(w1), &w1) == CL_SUCCESS);
			ASSERT(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL) == CL_SUCCESS);
			ASSERT(clEnqueueReadBuffer(queue, nonce_obj, CL_TRUE, 0, sizeof(nonce), &nonce, 0, NULL, NULL) == CL_SUCCESS);
			if(nonce) {
				printf("Nonce: 0x%08x%08x\n", __builtin_bswap32(nonce), __builtin_bswap32(w0));
				goto cleanup;
			}
		} while(w1 += global_work_size);
		ASSERT(clock_gettime(CLOCK_MONOTONIC, &end) != -1);
		printf("w0: 0x%08x, Hashrate: %.3f MH/s\n", w0, (~w1) / ((double)(end.tv_sec - start.tv_sec) * 1e6));
	} while(++w0);
	
cleanup:
	ASSERT(clReleaseMemObject(n_obj) == CL_SUCCESS);
	ASSERT(clReleaseMemObject(nonce_obj) == CL_SUCCESS);
	ASSERT(clReleaseKernel(kernel) == CL_SUCCESS);
	ASSERT(clReleaseCommandQueue(queue) == CL_SUCCESS);
}
