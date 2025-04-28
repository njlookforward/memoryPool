#include <atomic>
using std::atomic;

#include <iostream>
using std::cout; using std::endl;

#include <cstddef>
using std::size_t;

struct Slot_origin {
    Slot_origin *next;
};

struct Slot_atomic {
    atomic<Slot_atomic*> next;
};

int main()
{
    Slot_origin slot1;
    Slot_atomic slot2;
    cout << "the size of pointer is " << sizeof(slot1) << endl
         << "the size of pointer wrapped with atomic is " << sizeof(slot2) << endl;
    
    Slot_atomic *pslot = &slot2;
    cout << "pointer type: " << pslot << endl
         << "pointer type in dec: " << std::fixed << pslot << endl
         << "size_t type: " << reinterpret_cast<size_t>(pslot) << endl;

}