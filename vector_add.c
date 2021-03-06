#define PROGRAM_FILE "vector_add.cl"
#define DATA_SIZE 1024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef MAC
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

/* Find a GPU or CPU associated with the first available platform */
cl_device_id create_device() {

   cl_platform_id platform;
   cl_device_id dev;
   int err;

   /* Identify a platform */
   err = clGetPlatformIDs(1, &platform, NULL);
   if(err < 0) {
      printf("Couldn't identify a platform: %d\n", err);
      exit(1);
   } 

   /* Access a device */
   err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &dev, NULL);
   if(err == CL_DEVICE_NOT_FOUND) {
      err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &dev, NULL);
   }
   if(err < 0) {
      perror("Couldn't access any devices");
      exit(1);   
   }

   return dev;
}

cl_context create_context(cl_device_id dev) {

	cl_context ctx;
	int err;
	ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err);
    if(err < 0){
        printf("couldn't create a context: %d\n", err);
        exit(1);
    };
	
	return ctx;
	
}

cl_kernel create_kernel(cl_program prog){
	cl_kernel kn;
	int err;
    kn = clCreateKernel(prog, "vector_add", &err);
    if(err < 0){
        printf("Couldn't create a kernel: %d\n", err);
        exit(1);
    };
	return kn;
}

cl_command_queue create_queue(cl_device_id dev, cl_context ctx){
	cl_command_queue queue;
	int err;
	queue = clCreateCommandQueue(ctx, dev, 0, &err);
    if(err < 0) {
        printf("Couldn't create a command queue: %d\n", err);
        exit(1);
    };
	return queue;
}

/* Create program from a file and compile it */
cl_program build_program(cl_context ctx, cl_device_id dev, const char* filename) {

   cl_program program;
   FILE *program_handle;
   char *program_buffer, *program_log;
   size_t program_size, log_size;
   int err;

   /* Read program file and place content into buffer */
   program_handle = fopen(filename, "r");
   if(program_handle == NULL) {
      printf("Couldn't find the program file\n");
      exit(1);
   }
   fseek(program_handle, 0, SEEK_END);
   program_size = ftell(program_handle);
   rewind(program_handle);
   program_buffer = (char*)malloc(program_size + 1);
   program_buffer[program_size] = '\0';
   fread(program_buffer, sizeof(char), program_size, program_handle);
   fclose(program_handle);

   /* Create program from file */
   program = clCreateProgramWithSource(ctx, 1, 
      (const char**)&program_buffer, NULL, &err);
   if(err < 0) {
      printf("Couldn't create the program: %d\n", err);
      exit(1);
   }
   free(program_buffer);

   /* Build program */
   err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
   if(err < 0) {

      /* Find size of log and print to std output */
      clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, 
            0, NULL, &log_size);
      program_log = (char*) malloc(log_size + 1);
      program_log[log_size] = '\0';
      clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, 
            log_size + 1, program_log, NULL);
      printf("%s\n", program_log);
      free(program_log);
      exit(1);
   }
   return program;
}
int main() {

   /*****************************************/
   /* OpenCL specific variables */
   /*****************************************/  
   cl_device_id dev;
   cl_context ctx;
   cl_command_queue queue;
   cl_program program;
   cl_kernel kernel;
   cl_mem d_A;
   cl_mem d_B;
   cl_mem d_C;   
   cl_int i, err;
   // cl_int data_size = DATA_SIZE;
  
   /*****************************************/
   /* Initialize Host memory */
   /*****************************************/    
   float *h_A = (float *)malloc(DATA_SIZE * sizeof(float));
   float *h_B = (float *)malloc(DATA_SIZE * sizeof(float));
   float *h_C = (float *)malloc(DATA_SIZE * sizeof(float));
   float *check = (float *)malloc(DATA_SIZE * sizeof(float));
   srand(time(NULL));
   
    /* Initializing array h_A and h_B */
   for (i=0; i < DATA_SIZE; i++){
       h_A[i] = (float) rand()/RAND_MAX;
       h_B[i] = (float) rand()/RAND_MAX;
       h_C[i] = 0.0f;
       check[i] = h_A[i] + h_B[i];
   }
    
   /*****************************************/
   /* Initialize OpenCL */
   /*****************************************/   
   /* Create a device and context */
    dev = create_device();
    ctx = create_context(dev);
    program = build_program(ctx, dev, PROGRAM_FILE);
    kernel = create_kernel(program);
    queue = create_queue(dev, ctx);
    program = build_program(ctx, dev, PROGRAM_FILE);
	
   /* Set up device memory buffer */  
	d_A = clCreateBuffer(ctx, CL_MEM_READ_ONLY | 
		CL_MEM_COPY_HOST_PTR, sizeof(cl_float) * DATA_SIZE, h_A, &err);
	d_B = clCreateBuffer(ctx, CL_MEM_READ_ONLY | 
		CL_MEM_COPY_HOST_PTR, sizeof(cl_float) * DATA_SIZE, h_B, &err);
	d_C = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, 
		sizeof(cl_float) * DATA_SIZE, NULL, &err);	
    if(err < 0) {
        printf("Couldn't create a buffer: %d\n", err);
        exit(1);   
    };
   
   /* Create kernel argument */
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_A);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_B);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_C);
    if(err < 0) {
        printf("Couldn't set a kernel argument: %d\n", err);
        exit(1);   
    };

   /* Enqueue kernel */
	size_t work_size = DATA_SIZE;
	err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &work_size, 0, 0, 0, 0);
    if(err < 0) {
        printf("Couldn't enqueue the kernel: %d\n", err);
        exit(1);
    }
    /* Read memory buffer from device */
    err = clEnqueueReadBuffer(queue, d_C, CL_TRUE, 0,
            DATA_SIZE * sizeof(float), h_C, 0, NULL, NULL);
    if(err < 0) {
        printf("Couldn't read buffer: %d\n", err);
        exit(1);
    }

   /*****************************************/   
   /* Check the result with CPU */
   /*****************************************/   
   err =1;
   for(i=0; i<DATA_SIZE; i++){
       if(h_C[i] != check[i]){
           err=0;
       }
   }
   if(err == 1){
       printf("Check pass\n");
   }
   else {
       printf("Check fail\n ");
   }

   /* Deallocate resources */
   clReleaseMemObject(d_A);
   clReleaseMemObject(d_B);
   clReleaseMemObject(d_C);
   clReleaseKernel(kernel);
   clReleaseCommandQueue(queue);
   clReleaseProgram(program);
   clReleaseContext(ctx);
   return 0;
}
