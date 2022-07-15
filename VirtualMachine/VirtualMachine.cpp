#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <fstream>
using namespace std;

//vector<int> Virtual_Memory;
vector<int> Physical_Memory;
vector<vector<int>> Swap_Space;
vector<int> Page_Table;
vector<int> Dirty;
vector<int> Second_Chance;
vector<int> Frames;

vector<int> log_to_phy_address(int logical_address, int p) {
    int page = logical_address / p; //we divide the logical address by the size of the page and the quotient is the page number
    int offset = logical_address % p; // taking the remainder of dividing logical address by the size of the page we get offset
    // For example, 23456 / 4096 = 5, 23456 % 4096 = 2976
    vector<int> result(2);
    result[0] = page; result[1] = offset;
    return result; // results are stored and returned in a vector
}

int main()
{
    string op;
    int Virtual_Memory_Size, Physical_Memory_Size, Swap_Space_Size, Page_Size, num_of_frames, num_of_pages, mem_full = 0, pointer = 0;
    fstream file_r, file;
    file_r.open("input.txt", ios::in); // initializing constructor for reading input.txt file
    file.open("log.txt", ios::out); // initializing constructor for writing to log.txt file
    while (true) { // while cycle runs till "Exit" command is executed
        file_r >> op; // reading the operation
        if (op == "Init") { // First operation - Initialization of Memory Structure
            file_r >> Virtual_Memory_Size >> Physical_Memory_Size >> Swap_Space_Size >> Page_Size;
            num_of_frames = Physical_Memory_Size / Page_Size; // Finding the number of frames
            num_of_pages = Virtual_Memory_Size / Page_Size; // Finding the number of pages (Although size of page = size of frame, Virtual Memory is often larger than Physical, thus we need both variables
            Page_Size *= 1024;
            Physical_Memory.resize(Physical_Memory_Size*1024);
            Swap_Space.resize(Swap_Space_Size*1024 / Page_Size); 
            /*Swap Space contains blocks where pages are stored with the values inside them,
            thus the structure we are using is vector of vectors, where the inner vector size is the size of the page*/
            Page_Table.resize(num_of_pages, -1); // Page table is initially filled with -1, indicating that page does not have assigned frame in memory
            Dirty.resize(num_of_pages, 0); // Track the write operation on a page, used in updating the swap block
            Second_Chance.resize(num_of_pages, 0); // Keeps reference bits of pages, used in Second Chance(Clock) Page Replacement Algorithm 
            Frames.resize(num_of_frames); // Tracks frames and pages assigned to them, used in Second Chance Page Replacement Algorithm
            file << ">>> Initialized" << '\n';
        }
        else if (op == "Read") { // Read operation
            int logical_address, physical_address, frame, value;
            file_r >> logical_address;
            vector<int> address_data = log_to_phy_address(logical_address, Page_Size); // Obtaining page number and offset
            int page = address_data[0];
            int offset = address_data[1];
            if (Page_Table[page] != -1) { // Checking if page has assigned frame in memory
                frame = Page_Table[page];
                Second_Chance[page] = 1; // Updating reference bit
                physical_address = frame * Page_Size + offset; // Translating logical address to physical address (Physical address < Physical Memory Size)
                value = Physical_Memory[physical_address]; // Finding value in physical address
                file << ">>> Value at logical address " << logical_address << " (physical address " << physical_address << ") is " << value << '\n';
            }
            else { // If page has no frame assigned - Page Fault
                file << ">>> Page fault at address " << logical_address << '\n';
                if (mem_full != num_of_frames) { // Checking if there are free frames in memory, mem_full variable counts the number of frames used, max_value of mem_full = number of frames
                    frame = mem_full; // we assign frames starting from 0 till num_of_frames-1
                    Page_Table[page] = frame; // Update Page Table, assign new frame to a page
                    Frames[frame] = page; // Since we assign frames in ascending order, it will be helpful in Second Chance Algorithm
                    Second_Chance[page] = 1;
                    mem_full++; // update the counter
                    file << ">>> Loading page " << page << " to frame " << frame << '\n';
                    physical_address = frame * Page_Size + offset;
                    value = Physical_Memory[physical_address];
                    file << ">>> Value at logical address " << logical_address << " (physical address " << physical_address << ") is " << value << '\n';
                }
                else { // no free frames, page replacement algorithm must be used
                    int evicted_page = -1; // variable will store the page that will be evicted, for now it is -1
                    // Second Chance Algorithm
                    while (true) { // global pointer is firstly set to 0, the first page that has been assigned a frame
                        // We traverse through Frames array and find if the page's reference bit is set to 0
                        if (!Second_Chance[Frames[pointer]]) { // if reference bit is 0
                            evicted_page = Frames[pointer]; // found the page that will be evicted
                            pointer = (pointer + 1) % num_of_frames; // move the pointer to the next page, as the concept of second chance implies the circular flow of pages, we reset the pointer after the last page
                            break;
                        }
                        Second_Chance[Frames[pointer]] = 0; // if reference bit of a page is 1, we reset it
                        pointer = (pointer+1) % num_of_frames; // move the pointer
                    }
                    frame = Page_Table[evicted_page]; // obtain the freed frame
                    if (Dirty[evicted_page]) { // if the page had modified data
                        int off = 0;
                        for (int i = 0; i < Page_Size; i++) { // This is the implementation of swapping the page, to save the data and reuse it if needed, we move every entry in a page to swap block correspondent to a page
                            Swap_Space[evicted_page].push_back(Physical_Memory[frame * Page_Size + off++]);
                        }
                        file << ">>> Saving page " << evicted_page << " to swap block " << evicted_page << '\n';
                    }                   
                    Page_Table[evicted_page] = -1; 
                    file << ">>> Evicting page " << evicted_page << " from frame " << frame << '\n';
                    Page_Table[page] = frame; // assigned freed frame to a incoming page
                    Frames[frame] = page; // updating Frames table
                    Second_Chance[page] = 1;
                    physical_address = frame * Page_Size + offset;
                    if (!Swap_Space[page].empty()) { // Check if Swap Space has the page in store
                        int off = 0;
                        for (int i = 0; i < Page_Size; i++) { // moving page data to memory
                            value = Swap_Space[page][i];
                            Physical_Memory[frame * Page_Size + off++] = value;
                        }
                        file << ">>> Loading page " << page << " from swap block " << page << " to frame " << frame << '\n';
                        Swap_Space[page].clear(); // clear the block from page
                    }
                    else { // If Swap Space does not have the page (Occurs in starting instruction of the program)
                        file << ">>> Loading page " << page << " to frame " << frame << '\n';
                        for (int i = 0; i < Page_Size; i++) {
                           Physical_Memory[frame * Page_Size + i] = 0; // Removing evicted page data from memory
                        }
                        value = Physical_Memory[physical_address];
                    }
                    file << ">>> Value at logical address " << logical_address << " (physical address " << physical_address << ") is " << Physical_Memory[physical_address] << '\n';
                }
            }
        }
        else if (op == "Write") { // Write operation
            int logical_address, physical_address, frame, value;
            file_r >> logical_address >> value;
            vector<int> address_data = log_to_phy_address(logical_address, Page_Size);
            int page = address_data[0];
            int offset = address_data[1];
            if (Page_Table[page] != -1) { // Same as above
                frame = Page_Table[page];
                Second_Chance[page] = 1;
                physical_address = frame * Page_Size + offset;
                Physical_Memory[physical_address] = value;
            }
            else {
                file << ">>> Page fault at address " << logical_address << '\n';
                if (mem_full != num_of_frames) {
                    frame = mem_full;
                    Page_Table[page] = frame;
                    Second_Chance[page] = 1;
                    Frames[frame] = page;
                    mem_full++;
                    file << ">>> Loading page " << page << " to frame " << frame << '\n';
                    physical_address = frame * Page_Size + offset;
                    Physical_Memory[physical_address] = value;
                }
                else { // The same proccess of page replacement as in "Read" operation
                    int evicted_page = -1;
                    while (true) {
                        if (!Second_Chance[Frames[pointer]]) {
                            evicted_page = Frames[pointer];
                            pointer = (pointer + 1) % num_of_frames;
                            break;
                        }
                        Second_Chance[Frames[pointer]] = 0;
                        pointer = (pointer+1) % num_of_frames;
                    }
                    frame = Page_Table[evicted_page];
                    if (Dirty[evicted_page]) {
                        int off = 0;
                        for (int i = 0; i < Page_Size; i++) {
                            Swap_Space[evicted_page].push_back(Physical_Memory[frame * Page_Size + off++]);
                        }
                        file << ">>> Saving page " << evicted_page << " to swap block " << evicted_page << '\n';
                    }                   
                    Page_Table[evicted_page] = -1;
                    file << ">>> Evicting page " << evicted_page << " from frame " << frame << '\n';
                    Page_Table[page] = frame;
                    Frames[frame] = page;
                    Second_Chance[page] = 1;
                    physical_address = frame * Page_Size + offset;
                    if (!Swap_Space[page].empty()) {
                        int off = 0;
                        for (int i = 0; i < Page_Size; i++) {
                            value = Swap_Space[page][i];
                            Physical_Memory[frame * Page_Size + off++] = value;
                        }
                        file << ">>> Loading page " << page << " from swap block " << page << " to frame " << frame << '\n';
                        Swap_Space[page].clear();
                    }
                    else {
                        file << ">>> Loading page " << page << " to frame " << frame << '\n';
                        Physical_Memory[physical_address] = value;
                    }
                }
            }
            Dirty[page] = 1; // After writing, page becomes dirty, thus Dirty bit set to 1
            file << ">>> Written " << value << " to address " << logical_address << " (physical address " << physical_address << ")" << '\n';
        }
        else { // Exit operation - Clear all the data, finish the simulation, close the files
            Physical_Memory.clear();
            Swap_Space.clear();
            Page_Table.clear();
            Dirty.clear();
            Second_Chance.clear();
            Frames.clear();
            file_r.close();
            file.close();
            break;
        }
    }
}
