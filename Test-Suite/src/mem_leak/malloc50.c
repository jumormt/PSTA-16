/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void tmp(){

  int *p = NFRMALLOC(1); // psafix pre: PLK
  int j;
  for(j = 0; j < 10; j++){
    if(j > 2){}
    else return;
    
  }

  free(p);

}


int main(){

  tmp();

}
