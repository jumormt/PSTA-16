/*
 * double free false positive
 * Author: Jiawei Ren
 * Date: 02/26/2022
 */

#include "aliascheck.h"

void foo(int *p){

    SAFEFREE_FP(p);
}
void foo2(int *p){

    DOUBLEFREE(p);
}
int main(){
    int *i = SAFEMALLOC(sizeof(int));
//    int *b = SAFEMALLOC(sizeof(int));
    foo(i);
    int *j = SAFEMALLOC(sizeof(int));
    foo(j);
//    foo2(i);
//    foo(b);
}