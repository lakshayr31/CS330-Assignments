#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>

// Per process info for the pipe.
struct pipe_info_per_process {

    // TODO:: Add members as per your need...
    // per process fields
    int currPID;
    int canRead;
    int canWrite;

};

// Global information for the pipe.
struct pipe_info_global {

    char *pipe_buff;    // Pipe buffer: DO NOT MODIFY THIS.

    // TODO:: Add members as per your need...
    // global fields 
    int readOff;
    int writeOff;

    // struct file *readEnd;
    // struct file *writeEnd;

    int dataPresent;

    int processReadCount;
    int processWriteCount;
    int processCount;

};

// Pipe information structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct pipe_info {

    struct pipe_info_per_process pipe_per_proc [MAX_PIPE_PROC];
    struct pipe_info_global pipe_global;

};


// Function to allocate space for the pipe and initialize its members.
struct pipe_info* alloc_pipe_info () {
	
    // Allocate space for pipe structure and pipe buffer.
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign pipe buffer.
    pipe->pipe_global.pipe_buff = buffer;

    /**
     *  TODO:: Initializing pipe fields
     *  
     *  Initialize per process fields for this pipe.
     *  Initialize global fields for this pipe.
     *
     */

    // global fields 
    pipe->pipe_global.readOff = 0;
    pipe->pipe_global.writeOff = 0;
    pipe->pipe_global.processCount = 0;
    pipe->pipe_global.processReadCount = 0;
    pipe->pipe_global.processWriteCount = 0;
    pipe->pipe_global.dataPresent = 0;
    // per process fields
    for(int i = 0;i<MAX_PIPE_PROC;i++){
        pipe->pipe_per_proc[i].currPID = -1;
        pipe->pipe_per_proc[i].canRead = 0;
        pipe->pipe_per_proc[i].canWrite = 0;
    }

    // Return the pipe.
    return pipe;

}

// Function to free pipe buffer and pipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_pipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->pipe->pipe_global.pipe_buff);
    os_page_free(OS_DS_REG, filep->pipe);

}
int findProcessIndex(struct file *filep){
    struct exec_context *current = get_current_ctx();
    int index = -1;
    for(int i = 0;i<MAX_PIPE_PROC;i++){
        if(current->pid == filep->pipe->pipe_per_proc[i].currPID){
            index = i;
            break;
        }
    }
    return index;
}

// Fork handler for the pipe.
int do_pipe_fork (struct exec_context *child, struct file *filep) {
    printk("Fork Happening \n");
    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the pipe.
     *  This handler will be called twice since pipe has 2 file objects.
     *  Also consider the limit on no of processes a pipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */
    int ret_value = -EOTHERS;
    if(filep->pipe->pipe_global.processCount == MAX_PIPE_PROC){
        // show error that the limit on the processes for this pipe has been reached
        return ret_value;
    }

    ret_value = 0;
    //updating the process parameter
    //checking if this process if already attached to the pipe
    int attached = 0;
    for(int i = 0;i<MAX_PIPE_PROC;i++){
        if(filep->pipe->pipe_per_proc[i].currPID == child->pid){
            attached = 1;
            break;
        }
    }
    if(attached == 0){
        int index;
        for(int i = 0;i<MAX_PIPE_SIZE;i++){
            if(filep->pipe->pipe_per_proc[i].currPID == -1){
                index = i;
                break;
            }
        }
        printk("Index provided to the new process %d\n",index);
        filep->pipe->pipe_per_proc[index].currPID = child->pid;
        // we are opening the read and write end of the pipe with respect to this process
        filep->pipe->pipe_per_proc[index].canRead = 1;
        filep->pipe->pipe_per_proc[index].canWrite = 1;
        // since we allotted this process the pipe
        // updating the global parameter
        filep->pipe->pipe_global.processCount++;
        filep->pipe->pipe_global.processReadCount++;
        filep->pipe->pipe_global.processWriteCount++;
    }
    
    //updating the ref of the pipe read and write end
    filep->ref_count++;

    // Return successfully.
    return ret_value;

}

// Function to close the pipe ends and free the pipe when necessary.
long pipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the pipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the pipe.
     *  Use free_pipe() function to free pipe buffer and pipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *
     */

    int ret_value = -EOTHERS;
    struct exec_context *current = get_current_ctx();
    int index = findProcessIndex(filep);
    
    if(index < 0){
        return -EOTHERS;
    }
    //close the end of the pipe from this process
    if(filep->mode == O_READ){
        filep->pipe->pipe_per_proc[index].canRead = 0;
        filep->pipe->pipe_global.processReadCount--;
    }
    else if(filep->mode == O_WRITE){
        filep->pipe->pipe_per_proc[index].canWrite = 0;
        filep->pipe->pipe_global.processWriteCount--; 
    }
    if(filep->pipe->pipe_per_proc[index].canWrite == 0 && filep->pipe->pipe_per_proc[index].canRead == 0){
        filep->pipe->pipe_global.processCount--;
        filep->pipe->pipe_per_proc[index].currPID = -1;
    }
    if(filep->pipe->pipe_global.processCount == 0){
        printk("Pipe Free \n");
        free_pipe(filep);
    }
    // Close the file and return.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    printk("Reading or Writing Close Count : %d\n",filep->ref_count);
    printk("Process Count Pipe : %d \n",filep->pipe->pipe_global.processCount);
    // And return.
    return ret_value;

}

// Check whether passed buffer is valid memory location for read or write.
int is_valid_mem_range (unsigned long buff, u32 count, int access_bit) {

    /**
     *  TODO:: Implementation for buffer memory range checking
     *
     *  Check whether passed memory range is suitable for read or write.
     *  If access_bit == 1, then it is asking to check read permission.
     *  If access_bit == 2, then it is asking to check write permission.
     *  If range is valid then return 1.
     *  Incase range is not valid or have some permission issue return -EBADMEM.
     *
     */

    int ret_value = -EBADMEM;
    
    // checking if the address provided is a valid memory location
    //checking with mm segment
    struct exec_context *current = get_current_ctx();
    printk("Entered is_valid_mem_change\n");
    // printk("Buff: %x \n", buff);
    for(int i = 0; i< MAX_MM_SEGS;i++){
        
        // printk("Segment No %x \n",i);
        // printk("Start: %x End: %x NextFree: %x \n", current->mms[i].start, current->mms[i].end, current->mms[i].next_free);

        if(buff >= current->mms[i].start && buff <= current->mms[i].end){
            if(buff + count < current->mms[i].next_free){
            // if(buff + count <= current->mms[i].end){    
                // printk("Entered \n");
                if(access_bit == 1){
                    if((access_bit & current->mms[i].access_flags) == 1){
                        ret_value = 1;
                        break;
                    }
                } 
                else if(access_bit == 2){
                    // printk("Access Bit 2 \n");
                    // printk("Value of And : %d \n", (access_bit & current->mms[i].access_flags) );
                    if((access_bit & current->mms[i].access_flags) == 0x2){
                        // printk("Return Value = Given 1\n");
                        ret_value = 1;
                        break;
                    }
                }
            }
        }
        if(i == 3 && ret_value < 0){
            //checking in stack 
            if((buff >= STACK_START - MAX_STACK_SIZE) && (buff + count <= STACK_START)){
                // printk("Buffer is present in stack\n");
                if(access_bit == 1){
                    if((access_bit & current->mms[i].access_flags) == 1){
                        ret_value = 1;
                        break;
                    }
                } 
                else if(access_bit == 2){
                    // printk("Access Bit 2 \n");
                    // printk("Value of And : %d \n", (access_bit & current->mms[i].access_flags) );
                    if((access_bit & current->mms[i].access_flags) == 0x2){
                        // printk("Return Value = Given 1\n");
                        ret_value = 1;
                        break;
                    }
                }
            }
        }
    }
    //checking with vm area
    if(ret_value < 0){
        if(current->vm_area == NULL){
            // printk("vm_area is NULL\n");
        }
        // printk("Start: %x End: %x NextFree: %x \n",current->vm_area->vm_start, current->vm_area->vm_end, current->vm_area->vm_next);
        if(buff >= current->vm_area->vm_start && buff <= current->vm_area->vm_end){
            if(access_bit == 1){
                if((access_bit & current->vm_area->access_flags) == 1){
                    ret_value = 1;
                }
            } 
            else if(access_bit == 2){
                if((access_bit & current->vm_area->access_flags) == 2){
                    ret_value = 1;
                }
            }
        }
    }
    printk("The Return Value is : %d\n",ret_value);
    // Return the finding.
    return ret_value;

}

// Function to read given no of bytes from the pipe.
int pipe_read (struct file *filep, char *buff, u32 count) {
    printk("Read Called \n");

    /**
     *  TODO:: Implementation of Pipe Read
     *
     *  Read the data from pipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the pipe then just read
     *       that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If read end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */

    //validate file object's access right
    if(filep->mode != O_READ){
        return -EACCES;
        // return error
    }
    int index = findProcessIndex(filep);
    // can we read from the pipe from the current process?
    if(filep->pipe->pipe_per_proc[index].canRead == 0){
        return -EINVAL;
    }

    // validate the buffer access 
    if(is_valid_mem_range((unsigned long)buff, count, 2) != 1){
        printk("Reading Failed\n");
        return -EBADMEM;
        // handle error when memory not accessible;
    }
    printk("Reading Successfully \n");
    // read the data from the pipe 
    int readOff = filep->pipe->pipe_global.readOff;
    int writeOff = filep->pipe->pipe_global.writeOff; 
    u32 dataRead = 0; 

    // if(count > filep->pipe->pipe_global.dataPresent){
    //     count = filep->pipe->pipe_global.dataPresent;
    // }

    if(readOff < writeOff){
        while(readOff < writeOff && dataRead < count){
            buff[dataRead] = filep->pipe->pipe_global.pipe_buff[readOff];

            dataRead++;
            readOff++;
        }
    }
    else{
        if(readOff == writeOff && filep->pipe->pipe_global.dataPresent == 0){
            printk("Cannot read more, because nothing left to read");
        }
        else {
                while(readOff < MAX_PIPE_SIZE && dataRead < count){
                buff[dataRead] = filep->pipe->pipe_global.pipe_buff[readOff];

                dataRead++;
                readOff++;
            }
            if(count-dataRead > 0){
                readOff = 0;
                while(readOff < writeOff && dataRead < count){
                    buff[dataRead] = filep->pipe->pipe_global.pipe_buff[readOff];

                    dataRead++;
                    readOff++;
                }
            }
        }
    }
    if(readOff == MAX_PIPE_SIZE){
        readOff = 0;
    }
    // --set the files new read offset
    filep->pipe->pipe_global.readOff = readOff;
    filep->pipe->pipe_global.dataPresent -= dataRead;

    printk("New Read Off : %d \n", readOff);
    int bytes_read = 0;
    bytes_read = dataRead;
    // Return no of bytes read.
    return bytes_read;

}

// Function to write given no of bytes to the pipe.
int pipe_write (struct file *filep, char *buff, u32 count) {
    printk("Write Called \n");
    /**
     *  TODO:: Implementation of Pipe Write
     *
     *  Write the data from the provided buffer to the pipe buffer.
     *  If count is greater than available space in the pipe then just write data
     *       that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If write end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */
    //validate file object's access right
    if(filep->mode != O_WRITE){
        return -EACCES;
        // return error
    }
    struct exec_context *current = get_current_ctx();
    int index;
    for(int i = 0;i<MAX_PIPE_PROC;i++){
        if(current->pid == filep->pipe->pipe_per_proc[i].currPID){
            index = i;
            break;
        }
    }
    // can we write to the pipe from the current process?
    if(filep->pipe->pipe_per_proc[index].canWrite == 0){
        return -EINVAL;
    }

    // validate the buffer access 
    if(is_valid_mem_range((unsigned long)buff, count, 1) != 1){
        return -EBADMEM;
        // handle error when memory not accessible;
    }

    // write data to the pipe 
    int readOff = filep->pipe->pipe_global.readOff;
    int writeOff = filep->pipe->pipe_global.writeOff; 
    u32 dataWritten = 0; 

    // if(count > (MAX_PIPE_SIZE - filep->pipe->pipe_global.dataPresent)){
    //     count = MAX_PIPE_SIZE - filep->pipe->pipe_global.dataPresent;
    // }
    if(readOff <= writeOff){
        if(readOff == writeOff && filep->pipe->pipe_global.dataPresent == MAX_PIPE_SIZE){
            printk("The pupe is full cannot wirte more, please read first");
        }
        else {
            while(writeOff < MAX_PIPE_SIZE && dataWritten < count){
                filep->pipe->pipe_global.pipe_buff[writeOff] = buff[dataWritten];

                dataWritten++;
                writeOff++;
            }
            if(count-dataWritten > 0){
                writeOff = 0;
                while(writeOff < readOff && dataWritten < count){
                    filep->pipe->pipe_global.pipe_buff[writeOff] = buff[dataWritten];

                    dataWritten++;
                    writeOff++;
                }
            }
        }
    }
    else{
        while(writeOff < readOff && dataWritten < count ){
            filep->pipe->pipe_global.pipe_buff[writeOff] = buff[dataWritten] ;

            dataWritten++;
            writeOff++;
        }
    }
    if(writeOff == MAX_PIPE_SIZE){
        writeOff == 0;
    }
    // --set the files new write offset
    filep->pipe->pipe_global.writeOff = writeOff;
    filep->pipe->pipe_global.dataPresent += dataWritten;

    int bytes_written = 0;
    bytes_written = dataWritten;
    printk("Write: No. of bites writen %d and new writeOff %d \n ",dataWritten,filep->pipe->pipe_global.writeOff);
    // Return no of bytes written.
    return bytes_written;

}

// Function to create pipe.
int create_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of Pipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function. 
     *  Create pipe_info object by invoking the alloc_pipe_info() function and
     *       fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *       -ENOMEM: If memory is not enough.
     *       -EOTHERS: Some other errors.
     *
     */
    
    //find 2 free file descriptors 
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

    // create pipe_info object
    struct pipe_info *pipe = alloc_pipe_info();
    
    //fill the per process and global objects into the fields
    pipe->pipe_global.readOff = 0;
    pipe->pipe_global.writeOff = 0;
    pipe->pipe_global.processReadCount = 1;
    pipe->pipe_global.processWriteCount = 1;
    pipe->pipe_global.processCount = 1;
    pipe->pipe_global.dataPresent = 0;

    // per process?
    pipe->pipe_per_proc[0].currPID = current->pid;
    pipe->pipe_per_proc[0].canRead = 1;
    pipe->pipe_per_proc[0].canWrite = 1;


    //fill the file objects fields for both read and write
    readp->type = PIPE;
    readp->mode = O_READ;
    readp->pipe = pipe;
    readp->fops->read = pipe_read;
    readp->fops->close = pipe_close;

    writep->type = PIPE;
    writep->mode = O_WRITE;
    writep->pipe = pipe;
    writep->fops->write = pipe_write;
    writep->fops->close = pipe_close;

    //fill fd param
    fd[0] = readFD;
    fd[1] = writeFD;
    // char x[4] = "Hel";
    // printk(x);
    // printk("Read Side FOP = %x \n",readp);
    // printk("Write Side FOP = %x \n",writep);

    // printk("Read Side FOP = %d \n",readFD);
    // printk("Write Side FOP = %d \n",writeFD);
    // Simple return.
    return 0;

}
