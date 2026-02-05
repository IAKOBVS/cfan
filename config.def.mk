# NVML (uncomment to disable)
LIB_CUDA = /opt/cuda/lib64
LDFLAGS_CUDA = -L$(LIB_CUDA) -lnvidia-ml
REQ_CUDA = gpu-nvidia.h
