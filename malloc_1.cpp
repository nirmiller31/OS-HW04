
#include <iostream>
#include <unistd.h>

#define ZERO_SIZE_MALLOC_REQ  0
#define MAX_SIZE_MALLOC_REQ   100000000
#define FAIL_SBRK_MALLOC_REQ  -1

void* smalloc(size_t size){

     if(size == ZERO_SIZE_MALLOC_REQ){            // Bullet a. (size is 0), TODO what if size<0?
          return NULL;
     }
     if(size > MAX_SIZE_MALLOC_REQ){              // Bullet b. (size is bigger than 10^8)
          return NULL;
     }
     void* new_block_start = sbrk(size);          // Increase (try to) PROGRAM BREAK by size
     if(new_block_start == (void*)FAIL_SBRK_MALLOC_REQ){
          return NULL;                            // Bullet c. (size is bigger than 10^8)
     }
     return new_block_start;                      // Return success

}