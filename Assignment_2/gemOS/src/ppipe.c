#include<ppipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>

// Per process information for the ppipe.
struct ppipe_info_per_process {

    // TODO:: Add members as per your need...
    int readOff;
    int currPID;
    int canRead;
    int canWrite;
    int hasLeftGlobalReadOff; // keeps a check when a process's read
    // off has moved on from the global read;

};

// Global information for the ppipe.
struct ppipe_info_global {

    char *ppipe_buff;       // Persistent pipe buffer: DO NOT MODIFY THIS.

    // TODO:: Add members as per your need...
    int globalWriteOff;
    int globalStartReadOff;

    int dataPresent;

    int processReadCount;
    int processWriteCount;
    int processCount;
};

// Persistent pipe structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct ppipe_info {

    struct ppipe_info_per_process ppipe_per_proc [MAX_PPIPE_PROC];
    struct ppipe_info_global ppipe_global;

};


// Function to allocate space for the ppipe and initialize its members.
struct ppipe_info* alloc_ppipe_info() {

    // Allocate space for ppipe structure and ppipe buffer.
    struct ppipe_info *ppipe = (struct ppipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign ppipe buffer.
    ppipe->ppipe_global.ppipe_buff = buffer;

    /**
     *  TODO:: Initializing pipe fields
     *
     *  Initialize per process fields for this ppipe.
     *  Initialize global fields for this ppipe.
     *
     */ 

    // global fields
    ppipe->ppipe_global.globalWriteOff = 0;
    ppipe->ppipe_global.globalStartReadOff = 0;
    ppipe->ppipe_global.dataPresent = 0;
    ppipe->ppipe_global.processReadCount = 0;
    ppipe->ppipe_global.processWriteCount = 0;
    ppipe->ppipe_global.processCount = 0;

    // per process fields
    for(int i = 0;i<MAX_PPIPE_PROC;i++){
        ppipe->ppipe_per_proc[i].readOff = 0;
        ppipe->ppipe_per_proc[i].currPID = -1;
        ppipe->ppipe_per_proc[i].canRead = 0;
        ppipe->ppipe_per_proc[i].canWrite = 0;
        ppipe->ppipe_per_proc[i].hasLeftGlobalReadOff = 0;
    }
    // Return the ppipe.
    return ppipe;

}

// Function to free ppipe buffer and ppipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_ppipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->ppipe->ppipe_global.ppipe_buff);
    os_page_free(OS_DS_REG, filep->ppipe);

} 
int findPProcessIndex(struct file* filep){
    struct exec_context *current = get_current_ctx();
    int index = -1;
    for(int i = 0;i<MAX_PPIPE_PROC;i++){
        if(current->pid == filep->ppipe->ppipe_per_proc[i].currPID){
            index = i;
            break;
        }
    }
    return index;
}

// Fork handler for ppipe.
int do_ppipe_fork (struct exec_context *child, struct file *filep) {
    
    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the ppipe.
     *  This handler will be called twice since ppipe has 2 file objects.
     *  Also consider the limit on no of processes a ppipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */

    int ret_value = -EOTHERS;
    if(filep->ppipe->ppipe_global.processCount == MAX_PPIPE_SIZE){
        return ret_value;
    }
    ret_value = 0;
    
    int attached = 0;
    for(int i = 0;i<MAX_PPIPE_SIZE;i++){
        if(filep->ppipe->ppipe_per_proc[i].currPID == child->pid){
            attached = 1;
            break;
        }
    }
    int indexParent = findPProcessIndex(filep);
    if(attached == 0){
        // have to set the parameters of the pipe for this process
        int index;
        for(int i = 0;i<MAX_PPIPE_SIZE;i++){
            if(filep->ppipe->ppipe_per_proc[i].currPID == -1){
                index = i;
                break;
            }
        }
        printk("Index provided to the new process %d\n",index);
        // per process variables
        filep->ppipe->ppipe_per_proc[index].currPID = child->pid;
        filep->ppipe->ppipe_per_proc[index].canRead = 1;
        filep->ppipe->ppipe_per_proc[index].canWrite = 1;
        // have to set it's read offset to the parent read offset
        filep->ppipe->ppipe_per_proc[index].readOff = filep->ppipe->ppipe_per_proc[indexParent].readOff;
        filep->ppipe->ppipe_per_proc[index].hasLeftGlobalReadOff = filep->ppipe->ppipe_per_proc[indexParent].hasLeftGlobalReadOff;

        // global variables
        filep->ppipe->ppipe_global.processCount++;
        filep->ppipe->ppipe_global.processReadCount++;
        filep->ppipe->ppipe_global.processWriteCount++;
    }
    //updating the ref of the filep count
    filep->ref_count++;

    // Return successfully.
    return 0;

}


// Function to close the ppipe ends and free the ppipe when necessary.
long ppipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the ppipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the ppipe.
     *  Use free_pipe() function to free ppipe buffer and ppipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *                                                                          
     */

    int ret_value = -EOTHERS;
    int index = findPProcessIndex(filep);
    if(index < 0){
        return ret_value;
    }  

    //close the end of the pipe
    if(filep->mode == O_READ){
        filep->ppipe->ppipe_per_proc[index].canRead = 0;
        filep->ppipe->ppipe_global.processReadCount--;
    }
    if(filep->mode == O_WRITE){
        filep->ppipe->ppipe_per_proc[index].canWrite = 0;
        filep->ppipe->ppipe_global.processWriteCount--;
    }
    if(filep->ppipe->ppipe_per_proc[index].canWrite == 0 && filep->ppipe->ppipe_per_proc[index].canRead == 0){
        filep->ppipe->ppipe_global.processCount--;
        filep->ppipe->ppipe_per_proc[index].currPID = -1;
    }
    if(filep->ppipe->ppipe_global.processCount == 0){
        printk("Pipe Free\n");
        free_ppipe(filep);
    }

    // Close the file.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Function to perform flush operation on ppipe.
int do_flush_ppipe (struct file *filep) {
    printk("Entered Flush\n");
    /**
     *  TODO:: Implementation of Flush system call
     *
     *  Reclaim the region of the persistent pipe which has been read by 
     *      all the processes.
     *  Return no of reclaimed bytes.
     *  In case of any error return -EOTHERS.
     *
     */

    int reclaimed_bytes = 0;

    // find the minimum read off out off all the processes can access the pipe and that have read side open
    int minReadOff = MAX_PPIPE_SIZE+1;

    int countOfRead = 0;
    //keeps check if any processes are present at the global read;
    int presentAtGlobalReadOff = 0;
    for(int i = 0;i<MAX_PPIPE_PROC;i++){
        if(filep->ppipe->ppipe_per_proc[i].currPID != -1){
            if(filep->ppipe->ppipe_per_proc[i].canRead == 1){
                countOfRead++;
                if(minReadOff > filep->ppipe->ppipe_per_proc[i].readOff){
                    minReadOff = filep->ppipe->ppipe_per_proc[i].readOff;
                }
                if(filep->ppipe->ppipe_per_proc[i].hasLeftGlobalReadOff == 0){
                    presentAtGlobalReadOff = 1;
                }
            }
        }
    }
    if(countOfRead == 0 ){
        return 0;
    }
    printk("Are any present at global read off : %d\n",presentAtGlobalReadOff);
    int globalReadOff = filep->ppipe->ppipe_global.globalStartReadOff;
    printk("Global Read Off %d \n",globalReadOff);
    printk("minReadOff %d \n",minReadOff);
    if(minReadOff == globalReadOff){
        // add the relevant cases // would not be solved be checking of data present
        if(filep->ppipe->ppipe_global.globalWriteOff == globalReadOff && filep->ppipe->ppipe_global.dataPresent == MAX_PPIPE_SIZE){
            if(presentAtGlobalReadOff == 0){
                // this means no process has read at the globalReadOff for reading and all have passed it
                reclaimed_bytes = MAX_PPIPE_SIZE;
            }
        }
    }
    else if( minReadOff < globalReadOff ){
        reclaimed_bytes = MAX_PPIPE_SIZE - globalReadOff + minReadOff;
    }
    else {
        reclaimed_bytes = minReadOff - globalReadOff;
    }
    //setting the new global read pointer
    filep->ppipe->ppipe_global.globalStartReadOff = minReadOff;
    filep->ppipe->ppipe_global.dataPresent -= reclaimed_bytes;
    // set the files to which global readOff has caught up to
    if(reclaimed_bytes > 0){
        for(int i = 0;i<MAX_PPIPE_PROC;i++){
            if(filep->ppipe->ppipe_per_proc[i].readOff == minReadOff){
                filep->ppipe->ppipe_per_proc[i].hasLeftGlobalReadOff = 0;
            } 
        }
    }
    // Return reclaimed bytes.
    return reclaimed_bytes;

}

// Read handler for the ppipe.
int ppipe_read (struct file *filep, char *buff, u32 count) {
    
    /**
     *  TODO:: Implementation of PPipe Read
     *
     *  Read the data from ppipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the ppipe then just read
     *      that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If read end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */
    //validate file objects access right
    if(filep->mode != O_READ){
        return -EACCES;
        // return error
    }

    int index = findPProcessIndex(filep);
    if(index < 0){
        return -EOTHERS;
    }
    // can we read from the pipe from the current process?
    if(filep->ppipe->ppipe_per_proc[index].canRead == 0){
        return -EINVAL;
    }
    // read the data from the pipe for this particular process
    int readOff = filep->ppipe->ppipe_per_proc[index].readOff;
    int writeOff = filep->ppipe->ppipe_global.globalWriteOff;
    int globalReadOff = filep->ppipe->ppipe_global.globalStartReadOff;

    u32 dataRead = 0;

    printk("Reading : Process Read Off : %d\n",readOff);
    printk("Reading : Global Process Start Off : %d\n",globalReadOff);
    if(readOff < writeOff){
        while(readOff < writeOff && dataRead < count){
            buff[dataRead]= filep->ppipe->ppipe_global.ppipe_buff[readOff];

            dataRead++;
            readOff++;
        }
    }
    else{
        if(readOff == writeOff){
            if(filep->ppipe->ppipe_global.dataPresent == 0 || readOff != globalReadOff){
                // cannot read 
                printk("Cannot read more.\n");
            }
            else if(readOff == globalReadOff){
                if(filep->ppipe->ppipe_per_proc[index].hasLeftGlobalReadOff == 1){
                    printk("Cannot read more\n");
                }
                else{
                    while(readOff < MAX_PPIPE_SIZE && dataRead < count){
                        buff[dataRead] = filep->ppipe->ppipe_global.ppipe_buff[readOff];

                        dataRead++;
                        readOff++;
                    }
                    if(count-dataRead > 0){
                        readOff = 0;
                        while(readOff < writeOff && dataRead < count){
                            buff[dataRead] = filep->ppipe->ppipe_global.ppipe_buff[dataRead];
                            
                            dataRead++;
                            readOff++;
                        }
                    }                
                }
            }
            
        }
        else{
            while(readOff < MAX_PPIPE_SIZE && dataRead < count){
                buff[dataRead] = filep->ppipe->ppipe_global.ppipe_buff[readOff];

                dataRead++;
                readOff++;
            }
            if(count-dataRead > 0){
                readOff = 0;
                while(readOff < writeOff && dataRead < count){
                    buff[dataRead] = filep->ppipe->ppipe_global.ppipe_buff[dataRead];
                    
                    dataRead++;
                    readOff++;
                }
            }
        }
    }
    if(readOff == MAX_PPIPE_SIZE){
        readOff = 0;
    }
    // set the new process wise read offset 
    filep->ppipe->ppipe_per_proc[index].readOff = readOff;
    int bytes_read = 0;

    bytes_read = dataRead;
    if(bytes_read > 0){
        filep->ppipe->ppipe_per_proc[index].hasLeftGlobalReadOff = 1;
    }
    // Return no of bytes read.
    return bytes_read;
}

// Write handler for ppipe.
int ppipe_write (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of PPipe Write
     *
     *  Write the data from the provided buffer to the ppipe buffer.
     *  If count is greater than available space in the ppipe then just write
     *      data that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If write end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */
    //validation file object's right
    if(filep->mode != O_WRITE){
        return -EACCES;
    }
    int index = findPProcessIndex(filep);
    //process cannot access the pipe
    if(index < 0){
        return -EOTHERS;
    }
    // is the write end open for this process
    if(filep->ppipe->ppipe_per_proc[index].canWrite == 0){
        return -EINVAL;
    }

    int readOff = filep->ppipe->ppipe_global.globalStartReadOff;
    int writeOff = filep->ppipe->ppipe_global.globalWriteOff;
    u32 dataWritten = 0;
    
    
    printk("Writing : Global Write Off : %d\n",writeOff);
    if(readOff <= writeOff){
        if(readOff == writeOff && filep->ppipe->ppipe_global.dataPresent == MAX_PPIPE_SIZE){
            // cannot write to the pipe
            printk("Pipe Full Cannot Write Please Flush\n");
        }
        else {
            while(writeOff < MAX_PPIPE_SIZE && dataWritten < count){
                filep->ppipe->ppipe_global.ppipe_buff[writeOff] = buff[dataWritten];

                dataWritten++;
                writeOff++;
            }
            if(count-dataWritten > 0){
                writeOff = 0;
                while(writeOff < readOff && dataWritten < count){
                    filep->ppipe->ppipe_global.ppipe_buff[writeOff] = buff[dataWritten];

                    dataWritten++;
                    writeOff++;
                }
            }
        }
    }
    else{
        while(writeOff < readOff && dataWritten < count){
            filep->ppipe->ppipe_global.ppipe_buff[writeOff] = buff[dataWritten];

            dataWritten++;
            writeOff++;
        }
    }
    if(writeOff == MAX_PPIPE_SIZE){
        writeOff = 0;
    }
    // set the new pipe write offset
    filep->ppipe->ppipe_global.globalWriteOff = writeOff;

    // add data to the amount of data present
    filep->ppipe->ppipe_global.dataPresent += dataWritten;

    int bytes_written = 0;
    bytes_written = dataWritten;

    // Return no of bytes written.
    return bytes_written;

}

// Function to create persistent pipe.
int create_persistent_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of PPipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function.
     *  Create ppipe_info object by invoking the alloc_ppipe_info() function and
     *      fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *      -ENOMEM: If memory is not enough.
     *      -EOTHERS: Some other errors.
     *
     */

    u8 readFD = -1;
    u8 writeFD = -1;
    u8 flag = 0;
    for(u8 i = 0; i < MAX_OPEN_FILES ; i++){
        if(!flag && current->files[i]==NULL){
            readFD = i;
            flag = 1; 
        }
        else if(flag && current->files[i]==NULL){
            writeFD = i;
            break;
        }
    }
    if( readFD == -1 || writeFD == -1){
        return -ENOMEM;
    }

    //create two file objects
    current->files[readFD] = alloc_file();
    struct file *readp = current->files[readFD];

    current->files[writeFD] = alloc_file();
    struct file *writep = current->files[writeFD];

    struct ppipe_info *ppipe = alloc_ppipe_info();

    //filling global objects
    ppipe->ppipe_global.processReadCount = 1;
    ppipe->ppipe_global.processWriteCount = 1;
    ppipe->ppipe_global.processCount = 1;
    ppipe->ppipe_global.dataPresent = 0;

    //filling per process fields
    ppipe->ppipe_per_proc[0].currPID = current->pid;
    ppipe->ppipe_per_proc[0].canRead = 1;
    ppipe->ppipe_per_proc[0].canWrite = 1;

    //filling file object fields for both read and write
    readp->type = PPIPE;
    readp->mode = O_READ;
    readp->ppipe = ppipe;
    readp->fops->read = ppipe_read;
    readp->fops->close = ppipe_close;

    writep->type = PPIPE;
    writep->mode = O_WRITE;
    writep->ppipe = ppipe;
    writep->fops->write = ppipe_write;
    writep->fops->close = ppipe_close;

    //fill fd params
    fd[0] = readFD;
    fd[1] = writeFD;

    // Simple return.
    return 0;

}

