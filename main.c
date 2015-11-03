/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

char *algorithm;
int used_frames = 0;
int last_frame_written = 0;
int oldest_page;
struct disk *disk;

void page_fault_handler( struct page_table *pt, int page )
{
    int frames = page_table_get_nframes(pt);
    int pages = page_table_get_npages(pt);
    int bits_ptr;
    int frame_ptr;
    char *physmem = page_table_get_physmem(pt);
    int remove_page = 0;
	
    printf("page fault on page #%d\n",page);
    page_table_get_entry(pt, page, &frame_ptr, &bits_ptr);

    // Seed random number generator
    time_t t;
    srand((unsigned) time(&t));
        
    // Page not in memory
    if (bits_ptr == NULL) {
            
        fprintf(stderr, "Page not in memory.\n");
            
        // Check if frames full   
        if (used_frames == frames) {
                
            // If full, kick out random page
            if (strcmp(algorithm, "rand") == 0) {
                remove_page = (pages-1) * (double)rand() / (double)RAND_MAX + 0.5;
                page_table_get_entry(pt, remove_page, &frame_ptr, &bits_ptr);
                
                while (bits_ptr == NULL) {
                    fprintf(stderr, "Trying again!\n");
                    // Page not in table, try again
                    remove_page = (pages-1) * (double) rand() / (double)RAND_MAX + 0.5;
                    page_table_get_entry(pt, remove_page, &frame_ptr, &bits_ptr);
                }
            }
            else if (strcmp(algorithm, "fifo") == 0) {
            }
            fprintf(stderr, "Kicked out page %d\n", remove_page);

            // Check if frame written
            if (bits_ptr&PROT_WRITE) {
                fprintf(stderr, "Dirty page, writing back.\n");
                disk_write(disk, remove_page, &physmem[frame_ptr * BLOCK_SIZE]);
            }   
                
            // Read new page into memory
            fprintf(stderr, "Reading page %d into memory.\n", page);
            disk_read(disk, page, &physmem[frame_ptr * BLOCK_SIZE]);
                
            // Set new page entry, remove old one
            fprintf(stderr, "Setting page table entry.\n");
            page_table_set_entry(pt, page, frame_ptr, PROT_READ);
            page_table_set_entry(pt, remove_page, frame_ptr, 0);
                          
        }   
                
        // Else frames available
        else { 
                
            fprintf(stderr, "Frames available!\n");
            last_frame_written++;
                
            if (last_frame_written == frames) {
                last_frame_written = 0;
            }
                
            fprintf(stderr, "Reading page into memory and setting page table entry.\n");
            disk_read(disk, page, &physmem[last_frame_written * BLOCK_SIZE]);
            page_table_set_entry(pt, page, last_frame_written, PROT_READ);
            used_frames++;
        } 
    }           
        
    // Page in memory, so fault is due to write permissions not set
    else if (!(bits_ptr&PROT_WRITE)) {
        fprintf(stderr, "Setting write permissions for page %d\n", page);
        page_table_set_entry(pt, page, frame_ptr, PROT_READ|PROT_WRITE);
    }
    else { // Who knooooows
        fprintf(stderr, "Whoops! We don't know why this faulted.\n");
    }
          
    page_table_print(pt);
    return;
}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	algorithm = argv[3];
	const char *program = argv[4];

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	//char *physmem = page_table_get_physmem(pt);

	if((strcmp(algorithm,"rand") != 0) && (strcmp(algorithm, "fifo") != 0) && (strcmp(algorithm, "custom")) != 0) {
		fprintf(stderr, "unknown algorithm: %s\n", argv[3]);
	}

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[4]);

	}

	page_table_delete(pt);
	disk_close(disk);

	return 0;
}

