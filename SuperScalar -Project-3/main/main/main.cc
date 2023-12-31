/*
 * sim_ds
 * Simulator for out-of-order superscalar processor.
 * Default mode assumes perfect caches and perfect branch prediction
 * to simply simulate dynamic scheduling.
 * An imperfect instruction cache and/or next-line prefetching may be available.
 *
 * Daniel Sami
 * Fall 2016
 */
#define _CRT_SECURE_NO_DEPRECATE

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <deque>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include "sim_ds.h"

using namespace std;

// These variables must be consistent throughout the simulation.
u_int ROBSIZE, IQ_SIZE, WIDTH, CACHE_SIZE, P, dyn_instr_count = 0, cycles;
char* TRACE_FILE;
ifstream infile;    // Input file stream

// ROB, as well head and tail
u_int head = 3, tail = 3;
vector<rob_entry> ROB;
vector<rmt_entry> RMT(67);

// Issue Queue
vector<iq_entry> IQ;

// Execution list
vector<ex_entry> execute_list;

// FE_continue indicates where we have reached the end of the sequence.
u_int FE_continue = 0;

// Global pipeline registers themselves.
deque<bundle> DE_register;
deque<bundle> RN_register;
deque<bundle> RR_register;
deque<bundle> DI_register;
deque<bundle> WB_register;

// Main routine
int main(int argc, char** argv) {
    

    FILE* FP;               // File handler
    char* trace_file;       // Variable that holds trace file name;
    proc_params params;       // look at sim_bp.h header file for the the definition of struct proc_params
    int op_type, dest, src1, src2;  // Variables are read from trace file
    uint64_t pc; // Variable holds the pc read from input file

    if (argc != 5)
    {
        printf("Error: Wrong number of inputs:%d\n", argc - 1);
        exit(EXIT_FAILURE);
    }
    dyn_instr_count = 0,   // Dynamic instruction count
        cycles = 0;   // Total number of cycles. Always at least 1.
    double ipc = 0;   // Average instructions per cycle
    params.rob_size = strtoul(argv[1], NULL, 10);
    params.iq_size = strtoul(argv[2], NULL, 10);
    params.width = strtoul(argv[3], NULL, 10);
    trace_file = argv[4];
    printf("rob_size:%lu "
        "iq_size:%lu "
        "width:%lu "
        "tracefile:%s\n", params.rob_size, params.iq_size, params.width, trace_file);
    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if (FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }

    infile.open(trace_file);

    ROBSIZE = params.rob_size;
    IQ_SIZE = params.iq_size;
    WIDTH = params.width;
    ROB.resize(ROBSIZE);
    IQ.resize(IQ_SIZE);


    // Pipeline routine
    do {
        retire();
        //cout << "1" << endl;
        writeback();
        //cout << "2" << endl;
        execute();
        //cout << "3" << endl;
        issue();
        //cout << "4" << endl;
        dispatch();
        //cout << "5" << endl;
        regRead();
        //cout << "6" << endl;
        rename();
        //cout << "7" << endl;
        decode();
        //cout << "8" << endl;
        FE_continue = fetch();
        //cout << "9" << endl;
    } while (advance_cycle());

    ipc = (double)dyn_instr_count / (double)cycles;

    cout << "# === Simulator Command =========" << endl
        << "# ./sim_ds " << argv[1] << " " <<
        argv[2] << " " << argv[3]<<  " " << argv[4]<<endl;
    cout << "# === Processor Configuration ===" << endl;
    cout << "# ROB_SIZE  = " << argv[1] << endl;
    cout << "# IQ_SIZE   = " << argv[2] << endl;
    cout << "# WIDTH     = " << argv[3] << endl;
    //cout << "# CACHE_SIZE    = " << argv[4] << endl;
   // cout << "# PREFETCHING   = " << argv[5] << endl;
    cout << "# === Simulation Results ========" << endl;
    cout << "# Dynamic Instruction Count      = " << dyn_instr_count << endl;
    cout << "# Cycles                         = " << cycles << endl;
    cout << "# Instructions Per Cycle (IPC)   = " << fixed << setprecision(2) << ipc << endl;

    return 0;
}

bool advance_cycle() {
    cycles++;
    //status();
    if (FE_continue || DE_register.size() || RN_register.size() || RR_register.size()
        || DI_register.size() || !isIQempty() || execute_list.size()
        || WB_register.size() || !isROBempty()) return true;
    else return false;
}

bool isIQempty() {
    bool empty = true;
    for (u_int i = 0; i < IQ_SIZE; i++)
        if (IQ.at(i).valid) empty = false;
    return empty;
}
bool isROBempty() {
    bool empty = true;
    for (u_int i = 0; i < ROBSIZE; i++)
        if (ROB.at(i).valid) empty = false;
    return empty;
}

void status() {

    cout << "DE_register: " << DE_register.size() << endl;
    cout << "RN_register: " << RN_register.size() << endl;
    cout << "RR_register: " << RR_register.size() << endl;
    cout << "DI_register: " << DI_register.size() << endl;
    cout << "Is IQ Empty: " << isIQempty() << endl;
    cout << "execute_list: " << execute_list.size() << endl;
    cout << "WB_register: " << WB_register.size() << endl;
    cout << "Is ROB Empty: " << isROBempty() << endl;
    cout << "cycles: " << cycles << endl;
    robstatus();
}

void robstatus() {
    cout << "ROB -------------------" << endl;
    for (u_int i = 0; i < ROB.size(); i++) {
        cout << i << ": " << "val " << ROB.at(i).valid << " ready "
            << ROB.at(i).ready;
        if (head == i) cout << " HEAD ";
        if (tail == i) cout << " TAIL ";
        cout << endl;
    }
    cout << "the address of this is..." << ROB.at(head).address << endl;

}