/*
 * Use after free
 * Author: Jiawei Ren
 * Date: 02/26/2022
 */

#include "aliascheck.h"

int main(){
    int *a = SAFEMALLOC(sizeof(int));
    SAFEUAFFUNC(a);
    free(a);
}