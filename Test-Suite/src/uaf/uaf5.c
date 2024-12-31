/*
 * Use after free
 * Author: Jiawei Ren
 * Date: 02/26/2022
 */

#include "aliascheck.h"
void foo(int *p){

    free(p);
}

int main(){
    int *a,*b,*c;
    if(a)
        a = SAFEMALLOC(1);
    else
        a = SAFEMALLOC(1);
    b = a;
    SAFEUAFFUNC(a);
    SAFEUAFFUNC(b);
    foo(b);
    UAFFUNC(a);
    UAFFUNC(b);
}


