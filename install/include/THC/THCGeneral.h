#ifndef THC_GENERAL_INC
#define THC_GENERAL_INC

#include "THGeneral.h"
#include "THAllocator.h"
#undef log1p

#include "cuda.h"
#include "cuda_runtime.h"
#include "cublas_v2.h"

/* #undef USE_MAGMA */

#ifdef __cplusplus
# define THC_EXTERNC extern "C"
#else
# define THC_EXTERNC extern
#endif

#ifdef _WIN32
# ifdef THC_EXPORTS
#  define THC_API THC_EXTERNC __declspec(dllexport)
# else
#  define THC_API THC_EXTERNC __declspec(dllimport)
# endif
#else
# define THC_API THC_EXTERNC
#endif

#ifndef THAssert
#define THAssert(exp)                                                   \
  do {                                                                  \
    if (!(exp)) {                                                       \
      _THError(__FILE__, __LINE__, "assert(%s) failed", #exp);          \
    }                                                                   \
  } while(0)
#endif

struct THCRNGState;  /* Random number generator state. */

typedef struct _THCCudaResourcesPerDevice {
  cudaStream_t* streams;
  cublasHandle_t* blasHandles;
  /* Size of scratch space per each stream on this device available */
  size_t scratchSpacePerStream;
  /* Device-resident scratch space per stream, used for global memory
     reduction kernels. */
  void** devScratchSpacePerStream;
} THCCudaResourcesPerDevice;


/* Global state to be held in the cutorch table. */
typedef struct THCState
{
  struct THCRNGState* rngState;
  struct cudaDeviceProp* deviceProperties;
  /* Convenience reference to the current stream/handle in use */
  cudaStream_t currentStream;
  cublasHandle_t currentBlasHandle;
  /* Set of all allocated resources. resourcePerDevice[dev]->streams[0] is NULL,
     which specifies the per-device default stream. blasHandles do not have a
     default and must be explicitly initialized. We always initialize 1
     blasHandle but we can use more.
  */
  THCCudaResourcesPerDevice* resourcesPerDevice;
  /* Captured number of devices upon startup; convenience for bounds checking */
  int numDevices;
  /* Number of Torch defined resources available, indices 1 ... numStreams */
  int numUserStreams;
  int numUserBlasHandles;
  /* Index of the current selected per-device resource. Actual CUDA resource
     changes based on the current device, since resources are per-device */
  int currentPerDeviceStream;
  int currentPerDeviceBlasHandle;
  /* Allocator using cudaMallocHost. */
  THAllocator* cudaHostAllocator;

  /* Table of enabled peer-to-peer access between directed pairs of GPUs.
     If i accessing allocs on j is enabled, p2pAccess[i][j] is 1; 0 otherwise. */
  int** p2pAccessEnabled;

  /* Is direct cross-kernel p2p access allowed? Normally, only cross-GPU
     copies are allowed via p2p if p2p access is enabled at all for
     the pair of GPUs in question, but if this flag is true, then
     all cross-GPU access checks are disabled, allowing kernels to
     directly access memory on another GPUs.
     Note that p2p access must exist and be enabled for the pair of
     GPUs in question. */
  int p2pKernelAccessEnabled;

  void (*cutorchGCFunction)(void *data);
  void *cutorchGCData;
  long heapSoftmax;
  long heapDelta;
} THCState;

THC_API void THCudaInit(THCState* state);
THC_API void THCudaShutdown(THCState* state);
THC_API void THCudaEnablePeerToPeerAccess(THCState* state);

/* If device `dev` can access allocations on device `devToAccess`, this will return */
/* 1; otherwise, 0. */
THC_API int THCState_getPeerToPeerAccess(THCState* state, int dev, int devToAccess);
/* Enables or disables allowed p2p access using cutorch copy. If we are */
/* attempting to enable access, throws an error if CUDA cannot enable p2p */
/* access. */
THC_API void THCState_setPeerToPeerAccess(THCState* state, int dev, int devToAccess,
                                          int enable);

/* By default, direct in-kernel access to memory on remote GPUs is
   disabled. When set, this allows direct in-kernel access to remote
   GPUs where GPU/GPU p2p access is enabled and allowed. */
THC_API int THCState_getKernelPeerToPeerAccessEnabled(THCState* state);
THC_API void THCState_setKernelPeerToPeerAccessEnabled(THCState* state, int val);

THC_API struct cudaDeviceProp* THCState_getCurrentDeviceProperties(THCState* state);

THC_API void THCMagma_init(THCState *state);

/* State manipulators and accessors */
THC_API int THCState_getNumDevices(THCState* state);
THC_API void THCState_reserveStreams(THCState* state, int numStreams, int nonBlocking);
THC_API int THCState_getNumStreams(THCState* state);

THC_API cudaStream_t THCState_getDeviceStream(THCState *state, int device, int stream);
THC_API cudaStream_t THCState_getCurrentStream(THCState *state);
THC_API int THCState_getCurrentStreamIndex(THCState *state);
THC_API void THCState_setStream(THCState *state, int device, int stream);
THC_API void THCState_setStreamForCurrentDevice(THCState *state, int stream);

THC_API void THCState_reserveBlasHandles(THCState* state, int numHandles);
THC_API int THCState_getNumBlasHandles(THCState* state);

THC_API cublasHandle_t THCState_getDeviceBlasHandle(THCState *state, int device, int handle);
THC_API cublasHandle_t THCState_getCurrentBlasHandle(THCState *state);
THC_API int THCState_getCurrentBlasHandleIndex(THCState *state);
THC_API void THCState_setBlasHandle(THCState *state, int device, int handle);
THC_API void THCState_setBlasHandleForCurrentDevice(THCState *state, int handle);

/* For the current device and stream, returns the allocated scratch space */
THC_API void* THCState_getCurrentDeviceScratchSpace(THCState* state);
THC_API void* THCState_getDeviceScratchSpace(THCState* state, int device, int stream);
THC_API size_t THCState_getCurrentDeviceScratchSpaceSize(THCState* state);
THC_API size_t THCState_getDeviceScratchSpaceSize(THCState* state, int device);

#define THCudaCheck(err)  __THCudaCheck(err, __FILE__, __LINE__)
#define THCublasCheck(err)  __THCublasCheck(err,  __FILE__, __LINE__)

THC_API void __THCudaCheck(cudaError_t err, const char *file, const int line);
THC_API void __THCublasCheck(cublasStatus_t status, const char *file, const int line);

THC_API cudaError_t THCudaMalloc(THCState *state, void **ptr, size_t size);
THC_API cudaError_t THCudaFree(THCState *state, void *ptr);
THC_API void THCSetGCHandler(THCState *state,
                             void (*torchGCHandlerFunction)(void *data),
                             void *data );
THC_API void THCHeapUpdate(THCState *state, long size);

#endif
