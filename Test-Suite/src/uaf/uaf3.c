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
    int *k;
    int *a = SAFEMALLOC(sizeof(int));
    foo(a);
    UAFFUNC(a);
}