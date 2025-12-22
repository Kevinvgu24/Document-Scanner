# Document-Scanner
A local program using external camera to scanning documents and image then save to PDF and JPEG file on your computer

## Requirements

### CPU Version
This program requires an external library:
- **OpenCV version 4.10.0** (CPU-only version)

### GPU Version
If you are using the GPU version, please install:
- **CUDA libraries** (version depends on your computer's GPU specifications)
- Pre-build the environments with **Visual Studio 2026** for the best performance and debugging

## Installation

### For CPU Version
```bash
pip install opencv-python==4.10.0.84
```

### For GPU Version
1. Install CUDA libraries appropriate for your GPU
2. Set up Visual Studio 2026 build environment
3. Build OpenCV with CUDA support following the official OpenCV documentation
