/**
 * Example usage of libmyoutput.so
 * 
 * To compile this example:
 * 
 * g++ example_usage.cpp \
 *     -I./include \
 *     -L./lib \
 *     -lmyoutput \
 *     -Wl,-rpath,'$ORIGIN/lib' \
 *     -o example_app
 * 
 * Then run: ./example_app
 */

#include "MyLib.h"
#include <iostream>

// Declare the function from libmyoutput
extern "C" {
    void OnBendersStart(SubProblemsIds, int);
    #include "libmylib/include/julia_init.h"
}

int main() {
    init_julia(0,NULL) ; 
    std::cout << "Example usage of libmyoutput.so" << std::endl;
    
    // Example: Create SubProblemsIds structure
    char* sub_ids[] = {(char*)"subproblem_1", (char*)"subproblem_2"};
    SubProblemsIds ids;
    ids.subProblems_ids = sub_ids;
    ids.n_subproblems = 2;
    
    int rank = 0;
    
    std::cout << "Calling OnBendersStart..." << std::endl;
    OnBendersStart(ids, rank);
    std::cout << "OnBendersStart completed!" << std::endl;
    
    shutdown_julia(0) ; 
    return 0;
}

