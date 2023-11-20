# Usage
==============================
    Before you run this script, make sure you done:
    * Set device path in nvme_operation_handler()
    * Set break down size in nvme_operation_handler() (default is 128KB)
    ## Run by simulation:
        1. "cd /path/to/nvme_transfer"
        2. "make"
        3. "sudo ./nvme_api"
    ## Run with deepspeed
        1. Make sure your deepspeed can run by sudo
        2. Copy all .cpp and .h except main.cpp in nvme_transfer folder to /path/to/deepspeed/ops/csrc/aio/py_lib/
        3. Modify function in /path/to/deepspeed/ops/csrc/aio/py_lib/deepspeed_py_aio_handle.cpp by modify_function.cpp code
        4. Modify function AsyncIOBuilder->sources in /path/to/deepspeed/op_builder/async_io.py, add .cpp file name of step 2.
        4. Run deepspeed by sudo
# define
=============================
    ## In layer_manager.cpp
        #define debugCompare 0 --> For debug compare fail use
        #define currentIOMethod --> Set current IO method (ioctl/libaio...)
        #define libAIOasync 0 --> For libAIO async use
        #define libAIOPhysical 0 --> Switch libAIO open file or device
    ## In 
        #define asyncFeat 0 --> For IOCTL async use
        #define chunkSize 0xFF8 --> Set chunk size per IOCTL command
        #define posixMemalignFeat (!asyncFeat && 1) --> Add posix_memalign feature
            ### If asyncFeat == 0 and posixMemalignFeat == 0 --> ASYNC type, NOT use posix_memalign --> Invaild argument may happened when chunkSize > 0x4F0
            ### If asyncFeat == 0 and posixMemalignFeat == 1 --> SYNC type, USE posix_memalign --> Works fine with chunkSize <= 0xFF8
            ### If asyncFeat == 1 and posixMemalignFeat == 0 --> ASYNC type, USE posix_memalign --> NOT WORKING, NEED FIX!
            ### If asyncFeat == 1 and posixMemalignFeat == 1 --> NOT HAVE THIS SETTING IN CURRENT CODE!