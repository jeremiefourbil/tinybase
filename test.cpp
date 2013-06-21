#include <iostream>  
#include <typeinfo>  //for 'typeid' to work  

int main () {  
    int a = 3;  
    float b = 3.2;
    // char* s = "abc";

    // std::cout << typeid(a) << std::endl;  
    std::cout << a << std::endl;  
    // std::cout << typeid(b) << std::endl;
    // std::cout << typeid(s) << std::endl;

    return 0;
}  