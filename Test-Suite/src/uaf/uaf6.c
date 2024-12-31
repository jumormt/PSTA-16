/*
 * Use after free
 * Author: Jiawei Ren
 * Date: 02/26/2022
 */

#include "aliascheck.h"
void foo(int *p){
    SAFEUAFFUNC(p);
}

int main(){
    int *a = SAFEMALLOC(sizeof(int));
    foo(a);
    int *b = SAFEMALLOC(sizeof(int));
    foo(b);
    free(a);
    free(b);
}

