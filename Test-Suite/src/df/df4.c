/*
 * double free
 * Author: Jiawei Ren
 * Date: 02/26/2022
 */

#include "aliascheck.h"

int main(){
    int *i;
    int *b;
    if(i)
        i = SAFEMALLOC(1);
    else
        i = SAFEMALLOC(1);
    b = i;
    SAFEFREE(i);
    DOUBLEFREE(b);
}