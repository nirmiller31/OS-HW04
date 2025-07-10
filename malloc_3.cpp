
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

#define ZERO_SIZE_MALLOC_REQ  0
#define MAX_SIZE_MALLOC_REQ   100000000
#define FAIL_SBRK_MALLOC_REQ  -1

#define MAX_ORDER             10
#define TOTAL_BLOCKS          32
#define ZERO_ORDER_BLOCK_SIZE 128
#define INITIAL_BLOCK_SIZE    (1 << MAX_ORDER) * ZERO_ORDER_BLOCK_SIZE
#define INITIAL_HEAP_SIZE     INITIAL_BLOCK_SIZE * TOTAL_BLOCKS
#define SIZE_FOR_MMAP         (1 << MAX_ORDER) * ZERO_ORDER_BLOCK_SIZE

//================ Struct related start ================
struct MallocMetadata{
     size_t              size;
     bool                is_free;
     bool                is_mmap;
     int                 order;
     MallocMetadata*     next;
     MallocMetadata*     prev;
};

MallocMetadata* free_list[MAX_ORDER + 1] = {nullptr};       // Array of lists for every order
MallocMetadata* mmap_list                = nullptr;         // mmap_list as suggested

void* base_address = nullptr;

size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _num_meta_data_bytes();
size_t _size_meta_data();
//================= Struct related end =================

//================ Helper related start ================
void initial_allocator();

int get_order_from_size(size_t request_size);
void insert_to_free_list(MallocMetadata* block, int order);
void remove_from_free_list(MallocMetadata* block, int order);

void initial_allocator(){

     void* curr = sbrk(0);                                                      // Your alligment trick
     char* curr_ptr = (char*)curr;
     size_t align_size = INITIAL_HEAP_SIZE;
     size_t offset = (size_t)curr_ptr % align_size;
     size_t padding = (offset == 0) ? 0 : (align_size-offset);
     
     void* raw = sbrk(padding + INITIAL_HEAP_SIZE);
     if( raw == (void*)FAIL_SBRK_MALLOC_REQ ){
          perror("sbrk failed in the initial_allocator");                       // Some error handling
          exit(1);
     }

     base_address = (char*)raw + padding;                                       // Initial size for 32 free blocks, each is 10 order

     for(int i=0 ; i<TOTAL_BLOCKS ; ++i){
          MallocMetadata* block    = (MallocMetadata*)((char*)base_address + (i * INITIAL_BLOCK_SIZE));
          block -> size            = INITIAL_BLOCK_SIZE;
          block -> is_free         = true;
          block -> is_mmap         = false;
          block -> order           = MAX_ORDER;
          block -> next            = nullptr;
          block -> prev            = nullptr;
          insert_to_free_list(block, block -> order);
     }
}

int get_order_from_size(size_t request_size){
     size_t actual_requested_size = request_size + sizeof(MallocMetadata);      // The true size we need calculates the metadata
     size_t block_size = ZERO_ORDER_BLOCK_SIZE;                                 // Trying to fit the minimal size first
     int result_order = 0;
     while( (result_order <= MAX_ORDER) && (block_size < actual_requested_size) ){
          block_size <<=1;                                                      // **2
          result_order++;
     }
     return ( (result_order > MAX_ORDER) ? -1 : result_order );
}

void insert_to_free_list(MallocMetadata* block, int order){
     block -> next                 = nullptr;
     block -> prev                 = nullptr;
     
     if(!free_list[order]){                                                     // If first block in free_list
          free_list[order] = block;
          return;
     }

     MallocMetadata* current_list = free_list[order];
     if(block < current_list){                                                  // If block has smaller address than the smallest in the list
          block -> next            = current_list;
          current_list -> prev     = block;
          free_list[order]         = block;
          return;
     }

     while( (current_list -> next) && (current_list -> next < block) ){         // Here we want to find the ordered place
          current_list = current_list -> next;
     }
     block -> next = current_list -> next;
     block -> prev = current_list;
     if( current_list -> next ){                                                // If we are not at the end, updated the block's follower with the prev
          (current_list -> next) -> prev = block;
     }
     current_list -> next = block;                                              
}

void remove_from_free_list(MallocMetadata* block, int order){
     if( block -> prev ){                                                       // If block has previous, bind it to the follower
          (block -> prev) -> next       = block -> next;
     }
     else{                                                                      // Here block is the head
          free_list[order]              = block -> next;
     }
     if( block -> next ){                                                       // If block has follower, bind it to the previous
          (block -> next) -> prev       = block -> prev;
     }
     block -> next                      = nullptr;
     block -> prev                      = nullptr;
}
//================= Helper related end =================

//=========== malloc_3 implemintations start ===========
void* smalloc(size_t size){

     static bool list_initialized = false;

     if(size == ZERO_SIZE_MALLOC_REQ){            // Bullet a. (size is 0), TODO what if size<0?
          return NULL;
     }
     if(size > MAX_SIZE_MALLOC_REQ){              // Bullet b. (size is bigger than 10^8)
          return NULL;
     }
     if( !list_initialized ){
          initial_allocator();
          list_initialized = true;
     }

     if( size + sizeof(MallocMetadata) >= SIZE_FOR_MMAP ){  // Challenge 3 mmap() usage for >= 128kb
          void* addr = mmap(nullptr, size + sizeof(MallocMetadata), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
          if( addr == MAP_FAILED ){ return nullptr; }       // mmap failed

          MallocMetadata* block    = (MallocMetadata*)addr;
          block -> size            = size + sizeof(MallocMetadata);
          block -> is_free         = false;
          block -> is_mmap         = true;
          block -> order           = -1;
          block -> next            = mmap_list;
          if(mmap_list){
               mmap_list->prev     = block;
          }
          block -> prev            = nullptr;
          mmap_list                = block;

          return (void*)(block + 1);
     }
     int current_order, target_order = get_order_from_size(size);
     if( target_order == -1 ){ return nullptr; }  // Case of size too big

     current_order = target_order;

     while( (current_order <= MAX_ORDER) && !free_list[current_order] ){
          current_order++;
     }
     if( current_order > MAX_ORDER){ return nullptr; }

     while( current_order > target_order ){       // Buddy splitting proccess
          MallocMetadata* block = free_list[current_order];
          remove_from_free_list(block, current_order);
          current_order--;

          size_t new_block_size = ZERO_ORDER_BLOCK_SIZE << current_order;
          MallocMetadata* buddy = (MallocMetadata*)((char*)block + (new_block_size));

          block -> size            = new_block_size;
          block -> is_free         = true;
          block -> is_mmap         = false;
          block -> order           = current_order;

          buddy -> size            = new_block_size;
          buddy -> is_free         = true;
          buddy -> is_mmap         = false;
          buddy -> order           = current_order;

          insert_to_free_list(block, current_order);
          insert_to_free_list(buddy, current_order);
     }
     MallocMetadata* block = free_list[target_order];
     remove_from_free_list(block, target_order);
     block -> is_free = false;                    // Actual allocation

     return (void*)(block+1);
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
     if(block_Metadata->is_mmap){
          if(block_Metadata->prev){
               (block_Metadata->prev)->next  = block_Metadata->next;
          }
          else{
               mmap_list                     = block_Metadata -> next;
          }
          if(block_Metadata->next){
               (block_Metadata->next)->prev  = block_Metadata ->prev;
          }
          munmap((void*)block_Metadata, block_Metadata->size);
          return;
     }
     block_Metadata -> is_free = true;
     
     while( block_Metadata -> order < MAX_ORDER ){
          size_t block_size             = ZERO_ORDER_BLOCK_SIZE << block_Metadata->order;
          size_t offset                 = (char*)block_Metadata - (char*)base_address;
          size_t buddy_offset           = offset ^ block_size;
          MallocMetadata* buddy         = (MallocMetadata*)((char*)base_address + buddy_offset);

          if( !(buddy->is_free && (buddy->order == block_Metadata->order)) ){
               break;                                                 // Make sure buddy has same is_dree and order
          }
          remove_from_free_list(buddy, buddy->order);

          MallocMetadata* merged = (block_Metadata < buddy) ? block_Metadata : buddy;    // Merge be choosing the smaller
          merged->order++;
          merged->size = (ZERO_ORDER_BLOCK_SIZE << (merged->order));
          block_Metadata = merged;
     }
     insert_to_free_list(block_Metadata, block_Metadata->order);
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
     size_t required_size = size + sizeof(MallocMetadata);

     if(block_Metadata->is_mmap){
          if(block_Metadata->size == required_size){
               return oldp;
               }
               void* new_block = smalloc(size);
               if(!new_block) {
                    return NULL;
               }
               std::memmove(new_block, oldp, std::min(block_Metadata->size - sizeof(MallocMetadata), size));
               sfree(oldp);
               return new_block;
     }

     MallocMetadata* simulated = block_Metadata;  // Here we are not mmap
     int current_order = simulated->order;
     size_t offset = (char*)simulated - (char*)base_address;
 
     while(current_order < MAX_ORDER){
          size_t block_size = ZERO_ORDER_BLOCK_SIZE << current_order;
          size_t buddy_offset = offset ^ block_size;
          MallocMetadata* buddy = (MallocMetadata*)((char*)base_address + buddy_offset);
     
          if (!(buddy->is_free && buddy->order == current_order)) {
               break;
          }
 
          size_t merged_size = ZERO_ORDER_BLOCK_SIZE << (current_order + 1);
          if(merged_size >= required_size) {
               sfree(oldp);                            // sfree merge usage
               void* new_block = smalloc(size);
               if (new_block) {
                    std::memmove(new_block, oldp, std::min(block_Metadata->size - sizeof(MallocMetadata), size));
               }
               return new_block;
          }
          current_order++;
     }

     void* new_block = smalloc(size);             // If here we need a new memory allocation
     if(!new_block){
          return NULL;
     }

     std::memmove(new_block, oldp, block_Metadata->size);
     sfree(oldp);

     return new_block;

}
//============ malloc_3 implemintations end ============

size_t _num_free_blocks(){
     size_t free_blocks_count = 0;
     for(int i = 0 ; i<=MAX_ORDER ; i++){
          MallocMetadata* current_p = free_list[i];
          while(current_p){
               free_blocks_count++;
               current_p = current_p->next;
          }
     }
     return free_blocks_count;
}
size_t _num_free_bytes(){
     size_t free_bytes_count = 0;
     for(int i = 0 ; i<=MAX_ORDER ; i++){
          MallocMetadata* current_p = free_list[i];
          while(current_p){
               free_bytes_count += current_p->size;
               current_p = current_p->next;
          }
     }
     return free_bytes_count;
}
size_t _num_allocated_blocks(){
     if (base_address == nullptr) {
          return 0;
     }
     size_t alloced_blocks_count = 0;
     char* cursor = (char*)base_address;
     char* end = cursor + INITIAL_HEAP_SIZE;
     while(cursor < end){
          MallocMetadata* block = (MallocMetadata*)cursor;
          if( !block->is_free && !block->is_mmap ){
               alloced_blocks_count++;
          }
          cursor += block->size;
     }
     MallocMetadata* mmap_curr = mmap_list;
     while (mmap_curr) {
          alloced_blocks_count++;
         mmap_curr = mmap_curr->next;
     }
     return alloced_blocks_count;
}
size_t _num_allocated_bytes(){
     if (base_address == nullptr) {
          return 0;
     }
     size_t alloced_bytes_count = 0;
     char* cursor = (char*)base_address;
     char* end = cursor + INITIAL_HEAP_SIZE;
     while(cursor < end){
          MallocMetadata* block = (MallocMetadata*)cursor;
          if( !block->is_free && !block->is_mmap ){
               alloced_bytes_count += block->size;
          }
          cursor += block->size;
     }
     MallocMetadata* mmap_curr = mmap_list;
     while (mmap_curr) {
          alloced_bytes_count += mmap_curr->size;
         mmap_curr = mmap_curr->next;
     }
     return alloced_bytes_count;
}
size_t _num_meta_data_bytes(){
     return _num_allocated_blocks() * _size_meta_data();
}
size_t _size_meta_data(){
     return sizeof(MallocMetadata);
}
