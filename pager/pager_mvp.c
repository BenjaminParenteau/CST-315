#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 4
#define MAX_PAGES 8
#define NUM_FRAMES 4
#define PAGE_SIZE 4096

typedef struct {
    int frame;
    int valid;
    int dirty;
    int read_only;
    int in_swap;
} PageEntry;

typedef struct {
    int pid;
    char name[32];
    int active;
    int mem_size;
    int num_pages;
    PageEntry pages[MAX_PAGES];
} Process;

typedef struct {
    int used;
    int pid;
    char process_name[32];
    int page_number;
} Frame;

Process processes[MAX_PROCESSES];
Frame frames[NUM_FRAMES];
int next_victim = 0;

/* -------------------- setup -------------------- */

void init_system() {
    int i, j;

    for (i = 0; i < MAX_PROCESSES; i++) {
        processes[i].active = 0;
        processes[i].pid = -1;
        processes[i].name[0] = '\0';
        processes[i].mem_size = 0;
        processes[i].num_pages = 0;

        for (j = 0; j < MAX_PAGES; j++) {
            processes[i].pages[j].frame = -1;
            processes[i].pages[j].valid = 0;
            processes[i].pages[j].dirty = 0;
            processes[i].pages[j].read_only = 0;
            processes[i].pages[j].in_swap = 0;
        }
    }

    for (i = 0; i < NUM_FRAMES; i++) {
        frames[i].used = 0;
        frames[i].pid = -1;
        frames[i].process_name[0] = '\0';
        frames[i].page_number = -1;
    }
}

Process* get_process(int pid) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].active && processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return NULL;
}

Process* get_process_by_name(const char *name) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].active && strcmp(processes[i].name, name) == 0) {
            return &processes[i];
        }
    }
    return NULL;
}

void create_process(int pid, const char *name, int mem_size) {
    int i, j, num_pages;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (!processes[i].active) {
            processes[i].active = 1;
            processes[i].pid = pid;
            strncpy(processes[i].name, name, sizeof(processes[i].name) - 1);
            processes[i].name[sizeof(processes[i].name) - 1] = '\0';
            processes[i].mem_size = mem_size;

            num_pages = (mem_size + PAGE_SIZE - 1) / PAGE_SIZE;
            if (num_pages > MAX_PAGES) {
                num_pages = MAX_PAGES;
            }
            processes[i].num_pages = num_pages;

            for (j = 0; j < num_pages; j++) {
                processes[i].pages[j].frame = -1;
                processes[i].pages[j].valid = 0;
                processes[i].pages[j].dirty = 0;
                processes[i].pages[j].in_swap = 0;

                /* first page is executable/read-only */
                if (j == 0) {
                    processes[i].pages[j].read_only = 1;
                } else {
                    processes[i].pages[j].read_only = 0;
                }
            }

            printf("Created process %s (PID %d) with %d bytes (%d pages)\n",
                   processes[i].name, pid, mem_size, num_pages);
            return;
        }
    }

    printf("Could not create process %s\n", name);
}

/* -------------------- printing -------------------- */

void print_frames() {
    int i;
    printf("\nFRAME TABLE\n");
    for (i = 0; i < NUM_FRAMES; i++) {
        if (frames[i].used) {
            printf("Frame %d -> %s, Page %d\n",
                   i, frames[i].process_name, frames[i].page_number);
        } else {
            printf("Frame %d -> FREE\n", i);
        }
    }
}

void print_page_table(const char *name) {
    Process *p = get_process_by_name(name);
    if (p == NULL) return;

    printf("\nPAGE TABLE FOR %s\n", p->name);
    for (int i = 0; i < p->num_pages; i++) {
        printf("Page %d -> valid=%d frame=%d dirty=%d ro=%d swap=%d\n",
               i,
               p->pages[i].valid,
               p->pages[i].frame,
               p->pages[i].dirty,
               p->pages[i].read_only,
               p->pages[i].in_swap);
    }
}

/* -------------------- paging -------------------- */

int find_free_frame() {
    int i;
    for (i = 0; i < NUM_FRAMES; i++) {
        if (!frames[i].used) {
            return i;
        }
    }
    return -1;
}

void evict_frame(int frame_index) {
    int old_pid = frames[frame_index].pid;
    int old_page = frames[frame_index].page_number;

    Process *p = get_process(old_pid);
    if (p == NULL) return;

    printf("Evicting %s page %d from frame %d\n",
           p->name, old_page, frame_index);

    if (p->pages[old_page].dirty && !p->pages[old_page].read_only) {
        printf("  Dirty page -> write to swap\n");
        p->pages[old_page].in_swap = 1;
    } else {
        printf("  Clean/read-only page -> no swap write\n");
    }

    p->pages[old_page].valid = 0;
    p->pages[old_page].frame = -1;
    p->pages[old_page].dirty = 0;

    frames[frame_index].used = 0;
    frames[frame_index].pid = -1;
    frames[frame_index].process_name[0] = '\0';
    frames[frame_index].page_number = -1;
}

int get_frame_for_page(Process *p, int page_number) {
    int frame = find_free_frame();

    if (frame == -1) {
        frame = next_victim;
        next_victim = (next_victim + 1) % NUM_FRAMES;
        evict_frame(frame);
    }

    if (p->pages[page_number].in_swap) {
        printf("Loading %s page %d from swap into frame %d\n",
               p->name, page_number, frame);
    } else {
        printf("Demand paging %s page %d from executable into frame %d\n",
               p->name, page_number, frame);
    }

    frames[frame].used = 1;
    frames[frame].pid = p->pid;
    strncpy(frames[frame].process_name, p->name, sizeof(frames[frame].process_name) - 1);
    frames[frame].process_name[sizeof(frames[frame].process_name) - 1] = '\0';
    frames[frame].page_number = page_number;

    p->pages[page_number].valid = 1;
    p->pages[page_number].frame = frame;
    p->pages[page_number].dirty = 0;

    return frame;
}

/* -------------------- memory access -------------------- */

void access_memory_by_name(const char *name, int address, int write) {
    Process *p = get_process_by_name(name);
    int page_number, offset, frame, physical_address;

    if (p == NULL) {
        printf("Process %s does not exist\n", name);
        return;
    }

    printf("\n%s %s address %d\n",
           p->name, write ? "WRITE" : "READ", address);

    /* segmentation fault: outside process memory */
    if (address < 0 || address >= p->mem_size) {
        printf("SEGMENTATION FAULT: invalid address\n");
        return;
    }

    page_number = address / PAGE_SIZE;
    offset = address % PAGE_SIZE;

    /* demand paging */
    if (!p->pages[page_number].valid) {
        printf("PAGE FAULT on %s page %d\n", p->name, page_number);
        get_frame_for_page(p, page_number);
    }

    /* write protection fault */
    if (write && p->pages[page_number].read_only) {
        printf("SEGMENTATION FAULT: write to read-only page\n");
        return;
    }

    frame = p->pages[page_number].frame;
    physical_address = frame * PAGE_SIZE + offset;

    if (write) {
        p->pages[page_number].dirty = 1;
    }

    printf("Virtual address %d -> page %d, offset %d -> frame %d -> physical address %d\n",
           address, page_number, offset, frame, physical_address);
}

/* -------------------- cleanup -------------------- */

void terminate_process(const char *name) {
    Process *p = get_process_by_name(name);
    if (p == NULL) return;

    printf("\nTerminating %s\n", p->name);

    for (int i = 0; i < p->num_pages; i++) {
        if (p->pages[i].valid) {
            int f = p->pages[i].frame;
            frames[f].used = 0;
            frames[f].pid = -1;
            frames[f].process_name[0] = '\0';
            frames[f].page_number = -1;
        }
        p->pages[i].valid = 0;
        p->pages[i].frame = -1;
        p->pages[i].dirty = 0;
        p->pages[i].in_swap = 0;
    }

    p->active = 0;
    p->pid = -1;
    p->name[0] = '\0';
    p->mem_size = 0;
    p->num_pages = 0;
}

/* -------------------- main test -------------------- */

int main() {
    init_system();

    create_process(1, "MATT", 12000);   /* 3 pages */
    create_process(2, "MARK", 20000);   /* 5 pages */
    create_process(3, "LUKE", 16000);   /* 4 pages */

    /* demand paging and normal access */
    access_memory_by_name("MATT", 0, 0);
    access_memory_by_name("MATT", 5000, 1);
    access_memory_by_name("MARK", 0, 0);
    access_memory_by_name("MARK", 9000, 1);

    /* force replacement */
    access_memory_by_name("LUKE", 0, 0);
    access_memory_by_name("LUKE", 5000, 0);
    access_memory_by_name("MARK", 15000, 0);
    access_memory_by_name("MATT", 9000, 0);

    /* segmentation faults */
    access_memory_by_name("MATT", 50000, 0);
    access_memory_by_name("MATT", 100, 1);   /* page 0 is read-only */

    print_frames();
    print_page_table("MATT");
    print_page_table("MARK");
    print_page_table("LUKE");

    terminate_process("MATT");
    terminate_process("MARK");
    terminate_process("LUKE");

    print_frames();

    return 0;
}