/*
 * Use after free
 * Author: Jiawei Ren
 * Date: 02/26/2022
 */

#include "aliascheck.h"


int main(){
    int *a = SAFEMALLOC(sizeof(int));
    int *b = SAFEMALLOC(sizeof(int));
    int *c = SAFEMALLOC(sizeof(int));
    free(a);
    free(b);
    UAFFUNC(a);
    UAFFUNC(b);
    SAFEUAFFUNC(c);
}


