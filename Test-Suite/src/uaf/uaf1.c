/*
 * Use after free
 * Author: Jiawei Ren
 * Date: 02/26/2022
 */

#include "aliascheck.h"
extern size_t fread2(int*);
void freea(int* a){
    int i = 1;
    if(i==1)
        free(a);
}
int *fgetgo()
{
    size_t size = 96;
    int *line = malloc(size*sizeof(int));
    if(size != fread2(line)){
        free(line);
        return 0;
    }

    return line;
}

int main(){
    int *a = SAFEMALLOC(sizeof(int));
    freea(a);
    UAFFUNC(a);
    fgetgo();
}


