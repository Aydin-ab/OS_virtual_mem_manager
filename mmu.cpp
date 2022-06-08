#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <stack>
#include <map>
#include <list>

using namespace std;
//-------------------- STEP 0 : Define the constant of the problem --------------------
int MAX_NUM_FRAMES; // Max number of frames in memory. Will be set when reading the arguments
const int MAX_NUM_PTE = 64; // Max number of PTE for each process. Will be set when reading the arguments

// Total cost output variable
unsigned long inst_count;
unsigned long ctx_switches;
unsigned long process_exits;
unsigned long cost;

// Cost constants
const int COST_READ = 1;
const int COST_WRITE = 1;
const int COST_CTX_SWITCH = 130;
const int COST_EXIT = 1250;
const int COST_MAP = 300;
const int COST_UNMAP = 400;
const int COST_IN = 3100;
const int COST_OUT = 2700;
const int COST_FIN = 2800;
const int COST_FOUT = 2400;
const int COST_ZERO = 140;
const int COST_SEGV = 340;
const int COST_SEGPROT = 420;

//-------------------- STEP 1 : Create Virtual Memory Area objects --------------------
// First, we write the Virtual Memory Area object because we will need it to build the Process objects
struct VMA {

    int vmaid; // id of the VMA
    int start_page; // start page of the VMA
    int end_page; // end page of the VMA
    bool write_protected; // bit if VMA is write protected
    bool file_mapped; // bit if VMA is mapped to a file

    VMA (int vmaid_, int start_page_, int end_page_, bool write_protected_, bool file_mapped_) {
        vmaid = vmaid_;
        start_page = start_page_;
        end_page = end_page_;
        write_protected = write_protected_;
        file_mapped = file_mapped_;
    }
};

//-------------------- STEP 2 : Create Page Table Entry PTE objects --------------------
struct PTE {
    unsigned int valid:1;
    unsigned int referenced:1;
    unsigned int modified:1;
    unsigned int write_protect:1;
    unsigned int pagedout:1;
    unsigned int physAddr:7;
    
    // Added info

    // We initialize the PTE as empty before the simulation
    PTE () {
        valid = 0;
        referenced = 0;
        modified = 0;
        write_protect = 0;
        pagedout = 0;
        physAddr = 0;
    }

};


//-------------------- STEP 3 : Create Processes objects --------------------
// Second we write the Process class
struct Process {

    int pid; // id of the process in the pool
    int num_vmas; // number of VMAs
    vector<VMA> vmas; // vector storing the VMAs 
    vector<PTE> pageTable; // page table array storing the PTEs

    map<string, unsigned long> pstats;

    Process(int pid_, int num_vmas_) {
        pid = pid_;
        num_vmas = num_vmas_;
        for (int i = 0; i < MAX_NUM_PTE; i++) {
            pageTable.push_back(PTE());
        }
        pstats["unmaps"] = 0;
        pstats["maps"] = 0;
        pstats["ins"] = 0;
        pstats["outs"] = 0;
        pstats["fins"] = 0;
        pstats["fouts"] = 0;
        pstats["zeros"] = 0;
        pstats["segv"] = 0;
        pstats["segprot"] = 0;
        
    }

    // Check if a virtual page is in a VMA
    bool isInVMA(int vpage) {
        for (vector<VMA>::const_iterator it = vmas.begin(); it != vmas.end(); it++) {
            if (it->start_page <= vpage && vpage <= it->end_page) {
                return true;
            }
        }
        // If we're here, it means the virtual page didn't fit in any VMA
        return false;
    }

    VMA* getVMA(int vpage) {
        for (vector<VMA>::iterator it = vmas.begin(); it != vmas.end(); it++) {
            if (it->start_page <= vpage && vpage <= it->end_page) {
                return &(*it);
            }
        }
        // If we're here, it means the virtual page didn't fit in any VMA
        // But normally, this should never happen because this case should have been taken care
        // by the SEGFAULT exception
        return 0;
    }

    void set_write_protection_bit(int vpage) {
        VMA* vma = getVMA(vpage);
        PTE* pte = &(pageTable[vpage]);
        if (vma->write_protected) {
            pte->write_protect = 1;
        } else {
            pte->write_protect = 0;
        }
    }

};

//-------------------- STEP 4 : Create Instructions objects --------------------
// Third, we create the Instruction objects

struct Instruction {

    int iid; // id of the instruction
    char command; // command instruction c, r, w, e
    int arg; // argument of the command (process id, vpage)

    Instruction (int iid_, char command_, int arg_) {
        iid = iid_;
        command = command_;
        arg = arg_;
    }

    void print_instr() {
        printf("%d: ==> %c %d\n", iid, command, arg);
    }

};

//-------------------- STEP 5 : Read Input File and initialize the process pool and instruction queues --------------------
// Now, we can read the input file and initialize the instructions queue and the processes array
int NUM_PROCESSES = -1;
queue<Instruction> instructions;
vector<Process> processes;

void readInput(istream& input_file) {
    
    string line;
    // We skip the first comments lines
    while (getline(input_file, line)) {
        if (line[0] != '#') {
            break;
        }
    }

    // The first non comment line store the number of processes
    NUM_PROCESSES = stoi(line);

    // Now we can loop through all the processes :
    for (int i = 0; i < NUM_PROCESSES; i++) {
        int pid = i; // pid of the process in the pool
        // We skip the comments lines
        while (getline(input_file, line)) {
            if (line[0] != '#') {
                break;
            }
        }
        // Now we have the number of VMAs of the process
        int num_vmas = stoi(line);
        Process process = Process(pid, num_vmas); // create the process pointer
        // // Now we create each VMA and add them too the process VMA vector
        for (int j = 0; j < num_vmas; j++) {
            getline(input_file, line);
            int vmaid = j; // id of the vma in the table
            int start_page, end_page, write_protected, file_mapped;
            istringstream issVMA(line);
            issVMA >> start_page >> end_page >> write_protected >> file_mapped;
            VMA vma = VMA(vmaid, start_page, end_page, (bool) write_protected, (bool) file_mapped);
            process.vmas.push_back(vma);
        }
        processes.push_back(process);
    }


    // Now we deal with the instructions. We continue to ignore all comments
    int count = 0; // used to define the instruction id
    while (getline(input_file, line)) {
        if (line[0] == '#') {
            continue; // ignore and go to next line
        }
        // Now we know that the instruction line is like that : "command arg" so we parse it
        char command;
        int arg;
        stringstream issInstr(line);
        issInstr >> command >> arg;
        Instruction instruction = Instruction(count, command, arg);
        instructions.push(instruction);
        count++; 
    }

};


//-------------------- STEP 6 : Create Frame object and frame table --------------------
struct Frame {

    int fid; // Frame id which serves to index it in the frame table vector
    Process* process; // pointer of the process owning the frame
    int vpage; // vpage of the process page table owning the frame
    bool isFree; // check if the frame is free to use
    // Special Flag used for the exit instruciton : Differenciate between FOUT or "OUT" 
    // (in latter case, we need to put the frame in the free pool instead of the swap area)
    bool toFreePool; 

    unsigned int age; // Used for aging algorithm
    int time_last_used; // Used for working set algorithm

    Frame (int fid_) {
        fid = fid_;
        process = 0; 
        vpage = -1;
        isFree = true;
        toFreePool = false;
        age =  0;
        time_last_used = 0;
    }

    // Retrieve pte of frame
    PTE* get_pte() {
        PTE* pte = &(process->pageTable[vpage]);
        return pte;
    }

    // We must define 2 unmap functions. One for read and write instructions and one for the exit instruction
    // I use the C++ default parameters feature for that
    void unmap(bool onExit = false) {
        printf(" UNMAP %d:%d\n", process->pid, vpage);
//        cout << " UNMAP " << process->pid << ":" << vpage << endl;
        process->pstats["unmaps"]++;

        // Unmap the frame
        PTE* pte = &(process->pageTable[vpage]);
        VMA* vma = process->getVMA(vpage);

        // If modified : either going to file device or to swap area
        if (pte->modified) {
            // If file mapped -> FOUT
            if (vma->file_mapped) {
                cost += COST_FOUT;
                printf(" FOUT\n");
//                cout << " FOUT" << endl;
                process->pstats["fouts"]++;
            }
            // If the instruction is exit -> go to free pool
            else if (onExit) {
                // The special case of exit instruction is taken care in the simulation loop
                // Because I can't access the free frame pool in this scope..
                // We use this flag to tell the simulator that the frame need to be put in the free pool
                toFreePool = true;
                pte->pagedout = 0; // Because sending to free pool, not swap device
            } 
            // Last case scenario is go to swap device -> OUT
            else {
                cost += COST_OUT;
                printf(" OUT\n");
//                cout << " OUT" << endl;
                process->pstats["outs"]++;
                // In this case, page is put in swap space, so we set the pagedout bit
                pte->pagedout = 1;

            }
        }
        
        // If not modified but we're in exit command, we send the frame to free pool using the parent function
        if (onExit) {
            toFreePool = true;
            pte->pagedout = 0; // Because sending to free pool, not swap device
        }

        // Reset the PTE valid bit
        pte->valid = 0;

        process = 0;
        vpage = -1;
        isFree = true;
    }

    void map(Process* process_, int vpage_) {
        isFree = false;
        process = process_;
        vpage = vpage_;

        PTE* pte = &(process->pageTable[vpage]);

        // Set the PTE valid bit
        pte->valid = 1;
        
        VMA* vma = process->getVMA(vpage);

        // If file mapped, it's always -> FIN
        if (vma->file_mapped) {
            cost += COST_FIN;
            printf(" FIN\n");
//            cout << " FIN" << endl;
            process->pstats["fins"]++;
            pte->modified = 0; // Reset modified bit
        }
        // else if it comes from swap area -> IN
        else if (pte->pagedout) {
            cost += COST_IN;
            printf(" IN\n");
//            cout << " IN" << endl;
            process->pstats["ins"]++;
            pte->modified = 0; // reset modified bit
        } 
        // else it comes from free pool or is still ZERO -> ZERO
        else {
            cost += COST_ZERO;
            printf(" ZERO\n");
//            cout << " ZERO" << endl;
            process->pstats["zeros"]++;
        }

        // reset the age and update clock time
        age = 0;
        time_last_used = inst_count - 1;

        printf(" MAP %d\n", fid);
//        cout << " MAP " << fid << endl;
    }

};
// Global Frame table
vector<Frame> frameTable;
// Free Frame pool
queue<Frame> frameFreePool;

// Initialize frame free pool with empty frames once we know the frame table size given in argument
void initFrameFreePool(int MAX_NUM_FRAMES_) {
    for (int i = 0; i < MAX_NUM_FRAMES_; i++) {
        frameFreePool.push( Frame(i) );
    }
}

// Initialize frame table with empty frames once we know the frame table size given in argument
void initFrameTable(int MAX_NUM_FRAMES_) {
    for (int i = 0; i < MAX_NUM_FRAMES_; i++) {
        frameTable.push_back( Frame(i) );
    }
}

// Check if free pool is empty and if not,  returns the first available one (in order they were released)
Frame* allocate_frame_from_free_list() {
    if ( frameFreePool.empty() ) {
        return 0;
    }
    else {
        Frame* free_frame = &(frameFreePool.front());
        frameFreePool.pop();
        return free_frame;
    }
}

//-------------------- STEP 7 : Create Abstract class for Pager Algorithms --------------------

class Pager {

    public:
        int hand; // hand of the pager
        int daemon_clock;
        int TAU;

        virtual Frame* select_victim_frame() = 0; // Return the allocated frame

        Pager() {
            hand = 0;
            daemon_clock = 0;
            TAU = 49;
        }

};


//-------------------- STEP 8 : Create the different Pager Algorithms --------------------

class FIFO: public Pager {

    // This function is called if the frame table is FULL (frameTable.size() == MAX_NUM_FRAMES)
    Frame* select_victim_frame() {
        Frame* victim_frame = &frameTable[hand];
        hand = (hand + 1) % MAX_NUM_FRAMES; 
        return victim_frame;
    }

};

class CLOCK: public Pager {

    PTE* get_pte_from_hand(int hand) {
        Frame* frame = &frameTable[hand];
        PTE* pte = frame->get_pte();
        return pte;
    }

    // This function is called if the frame table is FULL (frameTable.size() == MAX_NUM_FRAMES)
    Frame* select_victim_frame() {

        PTE* victim_pte = get_pte_from_hand(hand);

        // While the pages are referenced, we reset them to 0 and advance
        while ( victim_pte->referenced ) {
            victim_pte->referenced = 0;
            hand = (hand + 1) % MAX_NUM_FRAMES; 
            victim_pte = get_pte_from_hand(hand);
        }
        // We step out of the while loop once we found a page with referenced bit = 0
        // So the victim frame is the physical adress of the last victim_frame of the loop
        int frameID = victim_pte->physAddr;
        Frame* victim_frame = &(frameTable[frameID]);

        hand = (hand + 1) % MAX_NUM_FRAMES; // advance hand for next call

        return victim_frame;
    }

};

class EnhancedSecondChance: public Pager {

    PTE* get_pte_from_hand(int hand) {
        Frame* frame = &frameTable[hand];
        PTE* pte = frame->get_pte();
        return pte;
    }

    int get_class(PTE* pte) {
        // We define the class as 2*R + M just like in the lectures
        return 2*pte->referenced + pte->modified;
    }

    void daemon_reset() {
        for (vector<Frame>::iterator it_frame = frameTable.begin(); it_frame != frameTable.end(); it_frame++) {
            Process* process = it_frame->process;
            int victim_vpage = it_frame->vpage;
            PTE* pte = &(process->pageTable[victim_vpage]);
            if (pte->valid) {
                pte->referenced = 0;
            }
        }
    }

    // This function is called if the frame table is FULL (frameTable.size() == MAX_NUM_FRAMES)
    Frame* select_victim_frame() {

        Frame* victim_frame = 0;
        for (int class_ = 0; class_ < 4; class_++ ) {
            PTE* victim_pte = get_pte_from_hand(hand);
            int victim_class = get_class(victim_pte);

            // If the first element is already the victim frame, we can choose it and stop  searching
            if ( victim_class == class_ ) {
                int frameID = victim_pte->physAddr;
                victim_frame = &(frameTable[frameID]);
                break;
            }

            // We look for the first frame of that class.
            // If no frames exists, this means we made a full clock so exit the while loop and search for the next class
            int start_clock = hand;
            while ( victim_class != class_ ) {                
                hand = (hand + 1) % MAX_NUM_FRAMES;
                if (hand == start_clock) {
                    // This means we did a full clock turn, so no frames of this class exist => we exit and look for next class
                    break;
                }
                victim_pte = get_pte_from_hand(hand);
                victim_class = get_class(victim_pte);
            }

            // If we made a full clock, we check next class
            if (hand == start_clock) {
                continue;
            }

            // Else we return this Frame
            int frameID = victim_pte->physAddr;
            victim_frame = &(frameTable[frameID]);
            break;
        }

        // last hand update for next function call
        hand = (hand + 1) % MAX_NUM_FRAMES;

        // We call the daemon once we've found the victim 
        int num_instr_since_last = inst_count - daemon_clock;
        if (num_instr_since_last >= 50) {
            daemon_reset();
            daemon_clock = inst_count;
        }

        return victim_frame;

    }

};

class AGING: public Pager {

    PTE* get_pte_from_hand(int hand) {
        Frame* frame = &frameTable[hand];
        PTE* pte = frame->get_pte();
        return pte;
    }

    // This function is called if the frame table is FULL (frameTable.size() == MAX_NUM_FRAMES)
    Frame* select_victim_frame() {

        // First we age the frames' PTE 
        for ( vector<Frame>::iterator it_frame = frameTable.begin(); it_frame != frameTable.end(); it_frame++ ) {
            // shift age
            it_frame->age = it_frame->age>>1;

            // Check if referenced
            PTE* pte = it_frame->get_pte();
            if (pte->referenced) {
                it_frame->age = (it_frame->age | 0x80000000); // set leading bit to 1
                pte->referenced = 0; // reset R bit
            }
            // else we do nothing, the leading bit is already 0 because of the shift
        }

        // Now we pick the frame with the lowest age.
        Frame* victim_frame = &frameTable[hand];
        unsigned int min_age = victim_frame->age;
        // Now we make a full clock turn and look for the youngest frame.
        // In case of equality, we pick the first one relative to the hand counter we had in the beginning
        int curr_hand = (hand + 1) % MAX_NUM_FRAMES;
        Frame* curr_frame = 0;
        
        while ( curr_hand != hand ) {
            curr_frame = &frameTable[curr_hand];
            if (curr_frame->age < min_age) {
                min_age = curr_frame->age;
                victim_frame = curr_frame;
            }
            curr_hand = (curr_hand + 1) % MAX_NUM_FRAMES;
        }

        // hand update for next function call -> We start at the frame after our victim
        hand = (victim_frame->fid + 1) % MAX_NUM_FRAMES;


        return victim_frame;

    }

};

class WORKING_SET: public Pager {

    PTE* get_pte_from_hand(int hand) {
        Frame* frame = &frameTable[hand];
        PTE* pte = frame->get_pte();
        return pte;
    }

    // Check if a frame is eligible to be replaced
    bool is_eligible(Frame* frame) {
        PTE* pte = frame->get_pte();

        // If the pte was referenced, it's not eligible
        if ( pte->referenced ) {
            return false;
        }

        // If the pte was not referenced but its time since last R reset is < TAU, it's not eligible
        if ( inst_count - 2 - frame->time_last_used < TAU ) {
            return false;
        }
        // If the pte was not referenced but its time since last R reset is >= TAU, it is eligible
        else {
            return true;
        }
    }

    Frame* get_oldest_frame() {
        // If no frame verifies the condition, we pick the oldest one
        // 2 Cases : If all frames are referenced (R = 1) the oldest one is .. the oldest one (= global_oldest_frame)
        //           If some frames are not referenced (R = 1) the oldest one is the oldest one among the unreferenced frames (= unref_oldest_frame)
        Frame* global_oldest_frame =  &frameTable[hand]; 
        int global_oldest_time = inst_count - global_oldest_frame->time_last_used;

        Frame* unref_oldest_frame = 0;
        int unref_oldest_time = -1;
        PTE* pte = frameTable[hand].get_pte();
        if (pte->referenced == 0) {
            unref_oldest_frame = &frameTable[hand];
            unref_oldest_time = inst_count - unref_oldest_frame->time_last_used;
        } else {
            unref_oldest_frame = 0;
            unref_oldest_time = -1;
        }
        // loop to find the oldest frame
        int start_hand = hand;
        hand = (hand + 1) % MAX_NUM_FRAMES;

        while ( hand != start_hand ) {

            Frame* frame = &frameTable[hand];
            int time = inst_count - frame->time_last_used;
            if (time > global_oldest_time) {
                global_oldest_frame = frame;
                global_oldest_time = time;
            }
            
            PTE* pte = frame->get_pte();
            if ( pte->referenced == 0 ) {
                if ( unref_oldest_frame == 0 || time > unref_oldest_time) {
                    unref_oldest_frame = frame;
                    unref_oldest_time = time;
                }
            }
            hand = (hand + 1) % MAX_NUM_FRAMES;
        }
        // We return the unref_oldest_time if it exists, else the global_oldest_frame
        if ( unref_oldest_frame == 0 ) {
            return global_oldest_frame;
        }
        else {
            return unref_oldest_frame;
        }

    }

    // This function is called if the frame table is FULL (frameTable.size() == MAX_NUM_FRAMES)
    Frame* select_victim_frame() {
        
        Frame* victim_frame = &frameTable[hand];

        // We go through the frame list to find the frist eligible frame + we update the ref bits
        int num_scan = 0;
        while ( !is_eligible(victim_frame) && num_scan != MAX_NUM_FRAMES ) {
            num_scan++; // TODELETE
            // If the reference bit is set, we reset it and update the time of the frame
            PTE* pte = victim_frame->get_pte();

            if ( pte->referenced ) {
                pte->referenced = 0;
                victim_frame->time_last_used = inst_count - 1;
            }
            // Increment hand
            hand = (hand + 1) % MAX_NUM_FRAMES;
            victim_frame = &frameTable[hand];
        }

        // Check if we made a full clock turn without finding an eligible frame
        // If that's the case, we select the oldest frame
        if (num_scan == MAX_NUM_FRAMES) {
            // If full clock turn, we pick the oldest frame
            victim_frame = get_oldest_frame();
        }

        // hand update for next function call -> We start at the frame after our victim
        hand = (victim_frame->fid + 1) % MAX_NUM_FRAMES;

        return victim_frame;

    }

};

class RANDOM: public Pager {
    public :
        int* random_nums; // This array will store all the random numbers
        int total_random_num;
        int ofs;

        RANDOM(istream& rand_file) {
            ofs = 0;
            initialize_random_array(rand_file);
        }

        void initialize_random_array(istream& rand_file){

            rand_file >> total_random_num; // Read first line where there is the total number of random numbers

            random_nums = new int[total_random_num] ;
            int curr_num;
            int incr = 0;
            while (rand_file >> curr_num) {
                random_nums[incr] = curr_num;
                incr++;
            }

        }


        int get_random_number() { 

            int randomVal = random_nums[ofs] % MAX_NUM_FRAMES;
            ofs++;
            if (ofs == total_random_num) {
                ofs = 0;
            }
            return randomVal;

        }


        // This function is called if the frame table is FULL (frameTable.size() == MAX_NUM_FRAMES)
        Frame* select_victim_frame() {
            int random_frame_id = get_random_number();
            Frame* victim_frame = &frameTable[random_frame_id];
            //hand = (hand + 1) % MAX_NUM_FRAMES; 
            return victim_frame;
        }

};


//-------------------- STEP 9 : Create the Simulator --------------------

struct Simulator {

    Pager* pager; // pointer to Pager algorithm
    Process* curr_process; // pointer to current process

    Simulator(Pager* pager_) {
        pager = pager_;
        curr_process = 0;
    }

    Frame* get_frame() {
        Frame* new_frame = allocate_frame_from_free_list();
        if (new_frame == 0) {
            new_frame = pager->select_victim_frame();
        }
        return new_frame;
    }

    Instruction get_next_instruction() {
        Instruction next_instruction = instructions.front();
        instructions.pop();
        return next_instruction;
    }

    void page_fault_handler(Process* curr_process, PTE* pte, int vpage) {
        // Page fault exception
        // Get new frame to allocate
        Frame* newFrame = get_frame();

        // If new frame was already mapped, we unmap it
        if (! newFrame->isFree) {
            cost += COST_UNMAP;
            newFrame->unmap();
        }
        // Now we map the frame
        cost += COST_MAP;
        newFrame->map( curr_process, vpage );
        curr_process->pstats["maps"]++;
        frameTable[newFrame->fid] = *newFrame;

        // Update PTE
        pte->physAddr = newFrame->fid;
        pte->valid = 1;

    }

     void simulation() {


         while( ! instructions.empty() ) {
             Instruction curr_instruction = get_next_instruction();
             inst_count++;
             curr_instruction.print_instr();
             if (curr_instruction.iid == 40) {
                 int caca = 0;
             }
             switch (curr_instruction.command) {

                 // CONTEXT SWITCH
                 case 'c' : {
                    ctx_switches++;
                    cost += COST_CTX_SWITCH;
                    int pid_to_switch = curr_instruction.arg; // pid of process to switch to
                    curr_process = &processes[pid_to_switch]; // pointer to process to switch to
                    break;
                 }
                 // READ
                 case 'r' : {
                    cost += COST_READ;

                    int vpage = curr_instruction.arg;
                    PTE* pte = &(curr_process->pageTable[vpage]);

                    if (!pte->valid) {
                        // Verify it is in a valid VMA
                        if (! curr_process->isInVMA(vpage)) {
                            // SEGV exception
                            cost += COST_SEGV;
                            curr_process->pstats["segv"]++;

                            printf(" SEGV\n");
//                            cout << " SEGV" << endl;
                            break;
                        }
                        page_fault_handler(curr_process, pte, vpage);
                    }

                    // Simuate hardware read
                    pte->referenced = 1;
                    break;
                 }

                case 'w' : {
                    cost += COST_WRITE;

                    int vpage = curr_instruction.arg;
                    PTE* pte = &(curr_process->pageTable[vpage]);
                    if (!pte->valid) {
                        // Verify it is in a valid VMA
                        if (! curr_process->isInVMA(vpage)) {
                            // SEGV exception
                            cost += COST_SEGV;
                            curr_process->pstats["segv"]++;
  
                            printf(" SEGV\n");
//                            cout << " SEGV" << endl;
                            break;
                        }
                        page_fault_handler(curr_process, pte, vpage);
                    }

                    // Simuate hardware write
                    pte->referenced = 1;
                    // Check if write protected
                    curr_process->set_write_protection_bit(vpage);
                    if (pte->write_protect == 1) {
                        // SEGPROT Exception
                        cost += COST_SEGPROT;
                        printf( " SEGPROT\n");
//                        cout << " SEGPROT" << endl;
                        curr_process->pstats["segprot"]++;
                    } else {
                        pte->modified = 1;
                    }
                    break;
                 }

                case 'e' : {
                    process_exits++;
                    cost += COST_EXIT;
                    printf("EXIT current process %d\n", curr_process->pid);
//                    cout << "EXIT current process " << curr_process->pid << endl;

                    vector<PTE>* pageTable = &(curr_process->pageTable);
                    for ( vector<PTE>::iterator it_pte = pageTable->begin(); it_pte != pageTable->end(); it_pte++ ) {
                        // If page valid
                        if (it_pte->valid) {
                            int frameNumber = it_pte->physAddr;
                            cost += COST_UNMAP;
                            bool onExit = true;
                            Frame* frame = &(frameTable[frameNumber]);
                            frame->unmap(onExit);
                            // Careful. If the frame is a dirty non-fmapped, we must add it to the free pool
                            // We used the onExit flag to tell the unmap function to NOT put the dirty non-fmapped in the swap area
                            // The unmap function set the toFreePool flag to tell us that the frame needs to be put in free pool
                            // We use this complicated flag system because the free frame pool is not available in the unmap scope
                            // So we have to do the manipulation here
                            if (frame->toFreePool) {
                                frameFreePool.push(*frame);
                                frame->toFreePool = false;
                            }
                        }
                        // If not valid, cancel the page from swap device
                        else {
                            it_pte->pagedout = 0;
                        }
                    }

                    curr_process = 0;
                } // end case 'e'

             } // end switch

         }// end while

    } // end simulation 

    void print_pagetables() {

        for (vector<Process>::iterator it_proc = processes.begin(); it_proc != processes.end(); it_proc++) {
            printf("PT[%d]:", it_proc->pid);
//            cout << "PT[" << it_proc->pid << "]:";
            vector<PTE>* pageTable = &(it_proc->pageTable);
            int incr = 0; // index the cirtual page in the page table
            for ( vector<PTE>::iterator it_pte = pageTable->begin(); it_pte != pageTable->end(); it_pte++ ) {
                // If not valid
                if ( !it_pte->valid ) {
                    // We check wrether or not it is paged out
                    if (it_pte->pagedout) {
                        printf(" #");
//                        cout << " #";
                   } else {
                       printf(" *");
//                        cout << " *";
                   }
                }
                else {
                    printf(" %d:", incr);
//                    cout << " " << incr << ":";
                    if (it_pte->referenced) {cout << "R";}
                    else {
                        printf("-");
//                        cout << "-";
                    }

                    if (it_pte->modified) {
                        printf("M");
//                        cout << "M";
                    }
                    else {
                        printf("-");
//                        cout << "-";
                    }

                    if (it_pte->pagedout) {
                        printf("S");
//                        cout << "S";
                    }
                    else {
                        printf("-");
//                       cout << "-";
                    }
                }
                incr++;

            }
            printf("\n");
//            cout << endl; // end of printing one page table
        }

    }

    void print_frametable() {
        
        printf("FT:");
//        cout << "FT:";
        for (vector<Frame>::iterator it_frame = frameTable.begin(); it_frame != frameTable.end(); it_frame++) {
            if (it_frame->isFree) {
                printf(" *");
//                cout << " *";
            } else {
                printf(" %d:%d", it_frame->process->pid, it_frame->vpage);
//                cout << " " << it_frame->process->pid << ":" << it_frame->vpage;
            }
        }
        printf("\n");
//        cout << endl;

    }

    void print_summary() {

        for (vector<Process>::iterator it_proc = processes.begin(); it_proc != processes.end(); it_proc++) {
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                    it_proc->pid,
                    it_proc->pstats["unmaps"], it_proc->pstats["maps"], it_proc->pstats["ins"], it_proc->pstats["outs"],
                    it_proc->pstats["fins"], it_proc->pstats["fouts"], it_proc->pstats["zeros"],
                    it_proc->pstats["segv"], it_proc->pstats["segprot"]
                    );
        }

    }

    void print_cost() {

        printf("TOTALCOST %lu %lu %lu %lu %lu\n",
        inst_count, ctx_switches, process_exits, cost, sizeof(PTE));
        
    }

};




int main(int argc, char *argv[]) {
    bool fflag = false;
    bool aflag = false;
    bool oflag = false;
    char *fvalue = NULL;
    char *avalue = NULL;
    char *ovalue = NULL;
    int o;

    
    opterr = 0;

    while ((o = getopt (argc, argv, "f:a:o:")) != -1)
        switch (o)
        {
        case 'f':
            fflag = true;
            if (optarg[0] == '-'){
                fprintf (stderr, "Option -f requires an argument.\n");
                return -1;
            }
            fvalue = optarg;
            break;
        case 'a':
            aflag = true;
            if (optarg[0] == '-'){
                fprintf (stderr, "Option -a requires an argument.\n");
                return -1;
            }
            avalue = optarg;
            break;
        case 'o':
            oflag = true;
            if (optarg[0] == '-') {
                fprintf (stderr, "Option -o requires an argument.\n");
                return -1;
            }
            ovalue = optarg;
            break;
        case '?':
            if (optopt == 'f' || optopt == 'a' || optopt == 'o') {
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            }
            else if (isprint (optopt)) {
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            }
            else {
            fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
            }
            return 1;
        default:
            abort ();
        }

    MAX_NUM_FRAMES = stoi(fvalue); // set the frame table size
    initFrameFreePool(MAX_NUM_FRAMES); // Initialize the empty frame table
    initFrameTable(MAX_NUM_FRAMES);

    if (argc - optind < 2 ) { 
        printf("Please give an input file AND a random file\n"); 
        return -1; 
    }
    else if (argc - optind > 2) { 
        printf("Please put only 1 input file and only 1 random file\n"); 
        return -1; 
    }
    // Now we know we have an input file and a random file as non-option arguments
    ifstream input_file ( argv[optind] ); // input file
    ifstream rand_file ( argv[optind + 1] ); // rand file

    // Check if file opening succeeded
    if ( !input_file.is_open() ) {
        cout<< "Could not open the input file \n"; 
        return -1;
    }
    else if ( !rand_file.is_open() ) {
        cout<< "Could not open the rand file \n"; 
        return -1;
    }

    // Process input file to initialize the processes, instructions etc
    readInput(input_file);

    // Define the pager
    Pager* pager;
    switch (avalue[0]) {

        case 'f' : {
            pager = new FIFO();
            break;
        }
        case 'c' : {
            pager = new CLOCK();
            break;
        }
        case 'e' : {
            pager = new EnhancedSecondChance();
            break;
        }
        case 'a' : {
            pager = new AGING();
            break;
        }
        case 'w' : {
            pager = new WORKING_SET();
            break;
        }
        case 'r' : {
            pager = new RANDOM(rand_file);
            break;
        }
    }

    Simulator simulator = Simulator(pager);
    simulator.simulation();

    string ovalue_str (ovalue);
    if (ovalue_str.find('P') != string::npos) {
        simulator.print_pagetables();
    }
    if (ovalue_str.find('F') != string::npos) {
        simulator.print_frametable();
    }
    if (ovalue_str.find('S') != string::npos) {
        simulator.print_summary();
        simulator.print_cost();
    }


    return 0;

}