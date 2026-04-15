#include <iostream>
#include "MyLib.h"


extern "C" {
    #include "libmylib/include/julia_init.h"
}


int main() 
{
    init_julia(0, NULL) ;

}