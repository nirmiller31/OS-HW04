
#include <iostream>
#include <unistd.h>
#include <cstring>

#define ZERO_SIZE_MALLOC_REQ  0
#define MAX_SIZE_MALLOC_REQ   100000000
#define FAIL_SBRK_MALLOC_REQ  -1



//================ Struct related start ================
struct MallocMetadata{
     size_t size;
     bool is_free;
     MallocMetadata* next;
     MallocMetadata* prev;
};

MallocMetadata* heap_list = nullptr;

size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _num_meta_data_bytes();
size_t _size_meta_data();
//================= Struct related end =================

//================= List related start =================
void insert_block_to_heap_list(MallocMetadata* new_block){
     if(!heap_list){               // Case we are only starting
          heap_list = new_block;
          new_block->next = nullptr;
          new_block->prev = nullptr;
          return;                  // Initiated and done
     }
     if(new_block < heap_list){    // Case new_block has smaller address than heap_list
          new_block->next = heap_list;
          new_block->prev = nullptr;
          heap_list->prev = new_block;
          heap_list = new_block;
          return;
     }
     MallocMetadata* current_list = heap_list;
     while(current_list->next && (current_list->next < new_block)){
          current_list = current_list->next;      // Find the last block
     }
     new_block->next = current_list->next;        // Add the new block in the currect place
     new_block->prev = current_list;

     if(current_list->next){                      
          (current_list->next)->prev = new_block;
     }
     current_list->next = new_block;
     return;
}
//================== List related end ==================

//=========== malloc_2 implemintations start ===========
void* smalloc(size_t size){

     if(size == ZERO_SIZE_MALLOC_REQ){            // Bullet a. (size is 0), TODO what if size<0?
          return NULL;
     }
     if(size > MAX_SIZE_MALLOC_REQ){              // Bullet b. (size is bigger than 10^8)
          return NULL;
     }

     MallocMetadata* current_list       = heap_list;
     while(current_list){
          if(current_list->is_free && current_list->size >= size){         // If found big enough slot for our block
               current_list->is_free    = false;
               return (void*)(current_list + 1);                           // 1 for the Metadata
          }
          current_list = current_list->next;
     }

     void* new_block_start = sbrk(_size_meta_data() + size);               // If here, we didnt find place in list, alloc new
     if(new_block_start == (void*)FAIL_SBRK_MALLOC_REQ){
          return NULL;                            // Bullet c. (size is bigger than 10^8)
     }

     MallocMetadata* new_block_Metadata = (MallocMetadata*)new_block_start;
     new_block_Metadata -> size         = size;
     new_block_Metadata ->is_free       = false;
     new_block_Metadata -> next         = nullptr;     // Will be handeled in list manager
     new_block_Metadata -> prev         = nullptr;

     insert_block_to_heap_list(new_block_Metadata);

     return (void*)(new_block_Metadata + 1); 
     
}

void* scalloc(size_t num, size_t size){

     if(num == ZERO_SIZE_MALLOC_REQ || size == ZERO_SIZE_MALLOC_REQ){           // Bullet a. (size is 0), TODO what if size<0?
          return NULL;
     }
     size_t total_size = num*size;
     if((total_size) > MAX_SIZE_MALLOC_REQ){                                    // Bullet b. (size is bigger than 10^8)
          return NULL;
     }

     void* new_block = smalloc(total_size);
     if(!new_block){
          return NULL;
     }

     std::memset(new_block, 0, total_size);
     return new_block;
}

void sfree(void* p){

     if(p == NULL){
          return;                  // Second bullet
     }
     MallocMetadata* block_Metadata = (((MallocMetadata*)p) - 1);
     if(block_Metadata -> is_free) {
          return;
     }
     block_Metadata -> is_free = true;
     return;
}

void* srealloc(void* oldp, size_t size){

     if(size == ZERO_SIZE_MALLOC_REQ){            // Bullet a. (size is 0), TODO what if size<0?
          return NULL;
     }
     if(size > MAX_SIZE_MALLOC_REQ){              // Bullet b. (size is bigger than 10^8)
          return NULL;
     }
     if(oldp == NULL){
          return smalloc(size);                   // Bullet Succes.b. smalloc equivalant
     }
     MallocMetadata* block_Metadata = (((MallocMetadata*)oldp) - 1);
     if(block_Metadata->size >= size){
          return oldp;                            // First bullet
     }

     void* new_block = smalloc(size);             // If here we need a new memory allocation
     if(!new_block){
          return NULL;
     }

     std::memmove(new_block, oldp, block_Metadata->size);
     sfree(oldp);

     return new_block;

}
//============ malloc_2 implemintations end ============


size_t _num_free_blocks(){
     size_t free_blocks_count      = 0;
     MallocMetadata* current_p     = heap_list;
     while(current_p){
          if(current_p -> is_free){
               free_blocks_count++;
          }
          current_p = current_p->next;
     }
     return free_blocks_count;
}
size_t _num_free_bytes(){
     size_t free_bytes_count       = 0;
     MallocMetadata* current_p     = heap_list;
     while(current_p){
          if(current_p -> is_free){
               free_bytes_count += current_p->size;
          }
          current_p = current_p->next;
     }
     return free_bytes_count;
}
size_t _num_allocated_blocks(){
     size_t alloced_blocks_count   = 0;
     MallocMetadata* current_p     = heap_list;
     while(current_p){
          alloced_blocks_count++;
          current_p = current_p->next;
     }
     return alloced_blocks_count;
}
size_t _num_allocated_bytes(){
     size_t alloced_bytes_count    = 0;
     MallocMetadata* current_p     = heap_list;
     while(current_p){
          alloced_bytes_count += current_p->size;
          current_p = current_p->next;
     }
     return alloced_bytes_count;
}
size_t _num_meta_data_bytes(){
     return _num_allocated_blocks() * _size_meta_data();
}
size_t _size_meta_data(){
     return sizeof(MallocMetadata);
}
