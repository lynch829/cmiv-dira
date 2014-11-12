
#include <math.h>
#include <string.h>

#ifdef __APPLE__
  #include <OpenCL/opencl.h>
#else
  #include <CL/cl.h>
#endif

#include "mex.h"

const char *KernelSource = "\n" \
"        __kernel void computePolyProj(__global int *ePtr, const double ue, __global double *nPtr,\n" \
"                              __global double *pPtr, __global double *muPtr,\n" \
"                              __global double *apPtr, const int e_Size,\n" \
"                                const int p_Size, const int mu_Size,\n" \
"                                const int total_size)\n" \
"{\n" \
"\n" \
"    #define energies 140\n" \
"\n" \
"    int t_id = get_global_id(0);\n" \
" \n" \
"    int k;\n" \
"    int l;\n" \
"\n" \
"    __private int energyLevels[energies];\n" \
"\n" \
"    for(k=1;k<energies;++k)\n" \
"        energyLevels[k] = ePtr[k];\n" \
"  \n" \
"    double temporarySum = 0;\n" \
"    double result = 0;\n" \
"\n" \
"    for(k = 1; k < e_Size-1; ++k)\n" \
"    {\n" \
" 	  temporarySum = 0;\n" \
"               \n" \
" 	  for(l = 0; l < p_Size; ++l)\n" \
"      {\n" \
" 	    temporarySum += (-*(muPtr + l*mu_Size + energyLevels[k] - 1))*\n" \
"                         100*(*(pPtr + t_id + l*total_size));\n" \
"      }\n" \
" 	  result += (energyLevels[k] * (*(nPtr + k))) *\n" \
"              	( energyLevels[k+1] - energyLevels[k-1]) *\n" \
" 		         exp(temporarySum);\n" \
"    }\n" \
"    *(apPtr + t_id) = -log(result/2/ue);\n" \
"}\n" \
"\n";

static char rcs_id[] = "$Revision: 1.10 $";

/* Input Arguments */
#define ELOW      (prhs[0])
#define EHIGH     (prhs[1])
#define UELOW     (prhs[2])
#define UEHIGH    (prhs[3])
#define NLOW      (prhs[4])
#define NHIGH     (prhs[5])
#define P         (prhs[6])
#define MULOW     (prhs[7])
#define MUHIGH    (prhs[8])

/* Output Arguments */
#define	APLOW     (plhs[0])
#define	APHIGH    (plhs[1])

/**
 * Need 9 input arguments: ELow, EHigh, uELow, ueHigh, NLow, NHigh, p, muLow, muHigh
 * Produces 2 output: ApLow, ApHigh
 */
void 
mexFunction(int nlhs, mxArray  *plhs[], int nrhs, const mxArray  *prhs[])
{
  /* Input pointers */
  int *eLowPtr;     /* Energy levels */
  int *eHighPtr;     /* Energy levels */
  double ueLow;	
  double ueHigh;
  double *nLowPtr;		
  double *nHighPtr;
  double *pPtr;		
  double *muLowPtr;
  double *muHighPtr;
  double *apLowPtr;
  double *apHighPtr;
  
  /* temp pointers for copying values */
  double *from_doublePtr;   
  double *to_doublePtr;   
  int *from_intPtr;   
  int *to_intPtr;  
  const int *dimPtr;   
  
  /*Size variables */
  int columns;      /* columns */
  int rows;         /* rows */
  int total_size;   /* Total image size, columns * rows */
  int eLow_Size;       /* Number of energy levels */
  int eHigh_Size;       /* Number of energy levels */
  int p_Size;       /* number of layers */
  int muLow_Size;
  int muHigh_Size;
  
  int k;            /* Loop counter */
  
  int error;
  cl_platform_id platform;
  unsigned int no_platforms;
  cl_device_id device;			 /* compute device id */
  unsigned int no_devices;
  size_t no_workgroups;
  cl_context context;				 /* compute context */
  cl_command_queue commandqueue;		  /* compute command queue */
  
  static cl_program program_computePolyProj;
  static cl_kernel kernel_computePolyProj;
  
  cl_mem E_input; 
  cl_mem UE_input;
  cl_mem N_input;
  cl_mem P_input;
  cl_mem MU_input;
  cl_mem AP_output;
  
  size_t localworksize;
  size_t globalworksize;
  
  cl_event event;
  
  /* Check validity of arguments */
  if (nrhs != 9)
  {
    mexErrMsgTxt("Incorrect number of INPUT arguments.");
  }
  
  if (nlhs != 2)
  {
    mexErrMsgTxt("Incorrect number of OUTPUT arguments.");
  }
  
  if (mxIsSparse(ELOW) || mxIsSparse(UELOW) || mxIsSparse(NLOW) || mxIsSparse(P) || mxIsSparse(MULOW))
  {
    mexErrMsgTxt("Sparse inputs not supported.");
  }
  
  if (mxIsSparse(EHIGH) || mxIsSparse(UEHIGH) || mxIsSparse(NHIGH) || mxIsSparse(MUHIGH))
  {
    mexErrMsgTxt("Sparse inputs not supported.");
  }
  
  if (!mxIsDouble(ELOW) || !mxIsDouble(UELOW) || !mxIsDouble(NLOW) || !mxIsDouble(P) || !mxIsDouble(MULOW))
  {
    mexErrMsgTxt("Input must be double.");
  }
  
  if (!mxIsDouble(EHIGH) || !mxIsDouble(UEHIGH) || !mxIsDouble(NHIGH) || !mxIsDouble(MUHIGH))
  {
    mexErrMsgTxt("Input must be double.");
  }
  
  /**
   * Matrix allocation
   */
  
  /* Get the size of E matrix and allocate memory */
  columns = mxGetN(ELOW);
  rows = mxGetM(ELOW);
  total_size = rows * columns; 
  eLowPtr = (int *) mxCalloc(total_size, sizeof(int));
  from_doublePtr = mxGetPr(ELOW);
  to_intPtr = eLowPtr;
  for (k = 0; k < total_size; k++)
    *(to_intPtr++) = (int)*(from_doublePtr++);
  
  eLow_Size = rows;
  
  columns = mxGetN(EHIGH);
  rows = mxGetM(EHIGH);
  total_size = rows * columns; 
  eHighPtr = (int *) mxCalloc(total_size, sizeof(int));
  from_doublePtr = mxGetPr(EHIGH);
  to_intPtr = eHighPtr;
  for (k = 0; k < total_size; k++)
    *(to_intPtr++) = (int)*(from_doublePtr++);
  
  eHigh_Size = rows;
  
  /* Copy value of uE */
  from_doublePtr = mxGetPr(UELOW);
  ueLow = *from_doublePtr;
  
  from_doublePtr = mxGetPr(UEHIGH);
  ueHigh = *from_doublePtr;
  
  /* Get the size of N matrix and allocate memory */
  columns = mxGetN(NLOW);
  rows = mxGetM(NLOW);
  total_size = rows * columns; 
  nLowPtr = (double *) mxCalloc(total_size, sizeof(double));
  from_doublePtr = mxGetPr(NLOW);
  to_doublePtr = nLowPtr;
  for (k = 0; k < total_size; k++)
    *(to_doublePtr++) = *(from_doublePtr++);
  
  columns = mxGetN(NHIGH);
  rows = mxGetM(NHIGH);
  total_size = rows * columns; 
  nHighPtr = (double *) mxCalloc(total_size, sizeof(double));
  from_doublePtr = mxGetPr(NHIGH);
  to_doublePtr = nHighPtr;
  for (k = 0; k < total_size; k++)
    *(to_doublePtr++) = *(from_doublePtr++);
  
  /* Get the size of MU matrix and allocate memory */
  columns = mxGetN(MULOW);
  rows = mxGetM(MULOW);
  total_size = rows * columns; 
  muLowPtr = (double *) mxCalloc(total_size, sizeof(double));
  from_doublePtr = mxGetPr(MULOW);
  to_doublePtr = muLowPtr;
  for (k = 0; k < total_size; k++)
    *(to_doublePtr++) = *(from_doublePtr++);
  
  muLow_Size = rows;
  
  columns = mxGetN(MUHIGH);
  rows = mxGetM(MUHIGH);
  total_size = rows * columns; 
  muHighPtr = (double *) mxCalloc(total_size, sizeof(double));
  from_doublePtr = mxGetPr(MUHIGH);
  to_doublePtr = muHighPtr;
  for (k = 0; k < total_size; k++)
    *(to_doublePtr++) = *(from_doublePtr++);
  
  muHigh_Size = rows;
  
  /* P */
  columns = mxGetN(P);
  rows = mxGetM(P);
  total_size = rows * columns; 
  pPtr = (double *) mxCalloc(total_size, sizeof(double));
  from_doublePtr = mxGetPr(P);
  to_doublePtr = pPtr;
  for (k = 0; k < total_size; k++)
    *(to_doublePtr++) = *(from_doublePtr++);
  
  /* Get the z-dimension for matrix P */
  dimPtr = mxGetDimensions(P);
  p_Size = *(dimPtr + 2);
  
  total_size = rows*columns/p_Size;
  
  /* Allocate a 2D matrix for the output AP */
  /* Columns is 720x5 = 3600, need only 720 as column value, so divide by p_Size */
  APLOW = mxCreateDoubleMatrix(rows, columns/p_Size, mxREAL);
  apLowPtr = mxGetPr(APLOW);
  
  APHIGH = mxCreateDoubleMatrix(rows, columns/p_Size, mxREAL);
  apHighPtr = mxGetPr(APHIGH);
  
  error = clGetPlatformIDs(1, &platform, &no_platforms);
    
//   printf("Platforms: %i \n", no_platforms);

  /* Where to run */
  error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, &no_devices);
     
//   printf("Devices: %i \n", no_devices);
  
  error = clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &no_workgroups, NULL);
//   printf("maximum number of workgroups: %d\n", (int)no_workgroups);
    
  context = clCreateContext(0, 1, &device, NULL, NULL, &error);

  // create command queue
  commandqueue = clCreateCommandQueue(context, device, 0, &error);
  
  // create the program
  program_computePolyProj = clCreateProgramWithSource(context, 1, (const char **) & KernelSource, NULL, &error);

  // build the program
  error = clBuildProgram(program_computePolyProj, 0, NULL, NULL, NULL, NULL);
  if (error != CL_SUCCESS)
  {
    // write out the build log, then exit
    char cBuildLog[10240];
    clGetProgramBuildInfo(program_computePolyProj, device, CL_PROGRAM_BUILD_LOG,
                          sizeof(cBuildLog), cBuildLog, NULL );
    printf("\nBuild Log:\n%s\n\n", (char *)&cBuildLog);
  }
  
  kernel_computePolyProj = clCreateKernel(program_computePolyProj, "computePolyProj", &error);
  
  /* Compute ApLow */
  E_input  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(int) * eLow_Size, eLowPtr, NULL);
  N_input  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(double) * eLow_Size, nLowPtr, NULL);
  P_input  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(double) * total_size*p_Size, pPtr, NULL);
  MU_input = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(double) * muLow_Size*p_Size, muLowPtr, NULL);
  
  AP_output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(double) * total_size, NULL, NULL);
  
  error  = clSetKernelArg(kernel_computePolyProj, 0, sizeof(cl_mem), (void *) &E_input);
  error |= clSetKernelArg(kernel_computePolyProj, 1, sizeof(double), &ueLow);
  error |= clSetKernelArg(kernel_computePolyProj, 2, sizeof(cl_mem), (void *) &N_input);
  error |= clSetKernelArg(kernel_computePolyProj, 3, sizeof(cl_mem), (void *) &P_input);
  error |= clSetKernelArg(kernel_computePolyProj, 4, sizeof(cl_mem), (void *) &MU_input);
  error |= clSetKernelArg(kernel_computePolyProj, 5, sizeof(cl_mem), (void *) &AP_output);
  error |= clSetKernelArg(kernel_computePolyProj, 6, sizeof(int), &eLow_Size);
  error |= clSetKernelArg(kernel_computePolyProj, 7, sizeof(int), &p_Size);
  error |= clSetKernelArg(kernel_computePolyProj, 8, sizeof(int), &muLow_Size);
  error |= clSetKernelArg(kernel_computePolyProj, 9, sizeof(int), &total_size);
  
  localworksize = no_workgroups;
  globalworksize = total_size;

  /* Enqueue and run */
  error = clEnqueueNDRangeKernel(commandqueue, kernel_computePolyProj, 1, NULL, &globalworksize, &localworksize, 0, NULL, &event);
  clWaitForEvents(1, &event); // Synch
  
    /* Read result */
  error = clEnqueueReadBuffer(commandqueue, AP_output, CL_TRUE, 0, sizeof(double) * total_size, apLowPtr, 0, NULL, &event );
  clWaitForEvents(1, &event); // Synch
  
  
  /* Compute ApHigh */
  E_input  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(int) * eHigh_Size, eHighPtr, NULL);
  N_input  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(double) * eHigh_Size, nHighPtr, NULL);
  P_input  = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(double) * total_size*p_Size, pPtr, NULL);
  MU_input = clCreateBuffer(context,  CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(double) * muHigh_Size*p_Size, muHighPtr, NULL);
  
  AP_output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(double) * total_size, NULL, NULL);
  
  error  = clSetKernelArg(kernel_computePolyProj, 0, sizeof(cl_mem), (void *) &E_input);
  error |= clSetKernelArg(kernel_computePolyProj, 1, sizeof(double), &ueHigh);
  error |= clSetKernelArg(kernel_computePolyProj, 2, sizeof(cl_mem), (void *) &N_input);
  error |= clSetKernelArg(kernel_computePolyProj, 3, sizeof(cl_mem), (void *) &P_input);
  error |= clSetKernelArg(kernel_computePolyProj, 4, sizeof(cl_mem), (void *) &MU_input);
  error |= clSetKernelArg(kernel_computePolyProj, 5, sizeof(cl_mem), (void *) &AP_output);
  error |= clSetKernelArg(kernel_computePolyProj, 6, sizeof(int), &eHigh_Size);
  error |= clSetKernelArg(kernel_computePolyProj, 7, sizeof(int), &p_Size);
  error |= clSetKernelArg(kernel_computePolyProj, 8, sizeof(int), &muHigh_Size);
  error |= clSetKernelArg(kernel_computePolyProj, 9, sizeof(int), &total_size);

  /* Enqueue and run */
  error = clEnqueueNDRangeKernel(commandqueue, kernel_computePolyProj, 1, NULL, &globalworksize, &localworksize, 0, NULL, &event);
  clWaitForEvents(1, &event); // Synch
  
    /* Read result */
  error = clEnqueueReadBuffer(commandqueue, AP_output, CL_TRUE, 0, sizeof(double) * total_size, apHighPtr, 0, NULL, &event );
  clWaitForEvents(1, &event); // Synch
  
  clReleaseMemObject(E_input);
  clReleaseMemObject(N_input);
  clReleaseMemObject(P_input);
  clReleaseMemObject(MU_input);
  clReleaseMemObject(AP_output);
  clReleaseKernel(kernel_computePolyProj);
  clReleaseProgram(program_computePolyProj);
  clReleaseCommandQueue(commandqueue);
  clReleaseContext(context);
}
