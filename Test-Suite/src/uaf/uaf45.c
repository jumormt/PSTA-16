/*
 * use after free
 * Author: Jiawei Ren
 * Date: 03/03/2022
 */
#include "aliascheck.h"

/* TEMPLATE GENERATED TESTCASE FILE
Filename: CWE416_Use_After_Free__malloc_free_struct_12.c
Label Definition File: CWE416_Use_After_Free__malloc_free.label.xml
Template File: sources-sinks-12.tmpl.c
*/
/*
 * @description
 * CWE: 416 Use After Free
 * BadSource:  Allocate data using malloc(), initialize memory block, and Deallocate data using free()
 * GoodSource: Allocate data using malloc() and initialize memory block
 * Sinks:
 *    GoodSink: Do nothing
 *    BadSink : Use data
 * Flow Variant: 12 Control flow: if(globalReturnsTrueOrFalse())
 *
 * */

#include "std_testcase.h"

#include <wchar.h>


void CWE416_Use_After_Free__malloc_free_struct_12_bad()
{
    twoIntsStruct * data;
    /* Initialize data */
    data = NULL;
    if(globalReturnsTrueOrFalse())
    {
        data = (twoIntsStruct *)SAFEMALLOC(100*sizeof(twoIntsStruct));
        if (data == NULL) {exit(-1);}
        {
            size_t i;
            for(i = 0; i < 100; i++)
            {
                data[i].intOne = 1;
                data[i].intTwo = 2;
            }
        }
        /* POTENTIAL FLAW: Free data in the source - the bad sink attempts to use data */
        free(data);
    }
//    else // can report err, but cannot pass testcase
//    {
//        data = (twoIntsStruct *)SAFEMALLOC(100*sizeof(twoIntsStruct));
//        if (data == NULL) {exit(-1);}
//        {
//            size_t i;
//            for(i = 0; i < 100; i++)
//            {
//                data[i].intOne = 1;
//                data[i].intTwo = 2;
//            }
//        }
//        /* FIX: Do not free data in the source */
//    }
    if(globalReturnsTrueOrFalse())
    {
        /* POTENTIAL FLAW: Use of data that may have been freed */
        UAFFUNC(data);
        /* POTENTIAL INCIDENTAL - Possible memory leak here if data was not freed */
    }
    else
    {
        /* FIX: Don't use data that may have been freed already */
        /* POTENTIAL INCIDENTAL - Possible memory leak here if data was not freed */
        /* do nothing */
        ; /* empty statement needed for some flow variants */
    }
}


/* goodB2G() - use badsource and goodsink by changing the first "if" so that
   both branches use the BadSource and the second "if" so that both branches
   use the GoodSink */
static void goodB2G()
{
    twoIntsStruct * data;
    /* Initialize data */
    data = NULL;
    if(globalReturnsTrueOrFalse())
    {
        data = (twoIntsStruct *)SAFEMALLOC(100*sizeof(twoIntsStruct));
        if (data == NULL) {exit(-1);}
        {
            size_t i;
            for(i = 0; i < 100; i++)
            {
                data[i].intOne = 1;
                data[i].intTwo = 2;
            }
        }
        /* POTENTIAL FLAW: Free data in the source - the bad sink attempts to use data */
        free(data);
    }
    else
    {
        data = (twoIntsStruct *)SAFEMALLOC(100*sizeof(twoIntsStruct));
        if (data == NULL) {exit(-1);}
        {
            size_t i;
            for(i = 0; i < 100; i++)
            {
                data[i].intOne = 1;
                data[i].intTwo = 2;
            }
        }
        /* POTENTIAL FLAW: Free data in the source - the bad sink attempts to use data */
        free(data);
    }
    if(globalReturnsTrueOrFalse())
    {
        /* FIX: Don't use data that may have been freed already */
        /* POTENTIAL INCIDENTAL - Possible memory leak here if data was not freed */
        /* do nothing */
        ; /* empty statement needed for some flow variants */
    }
    else
    {
        /* FIX: Don't use data that may have been freed already */
        /* POTENTIAL INCIDENTAL - Possible memory leak here if data was not freed */
        /* do nothing */
        ; /* empty statement needed for some flow variants */
    }
}

/* goodG2B() - use goodsource and badsink by changing the first "if" so that
   both branches use the GoodSource and the second "if" so that both branches
   use the BadSink */
static void goodG2B()
{
    twoIntsStruct * data;
    /* Initialize data */
    data = NULL;
    if(globalReturnsTrueOrFalse())
    {
        data = (twoIntsStruct *)SAFEMALLOC(100*sizeof(twoIntsStruct));
        if (data == NULL) {exit(-1);}
        {
            size_t i;
            for(i = 0; i < 100; i++)
            {
                data[i].intOne = 1;
                data[i].intTwo = 2;
            }
        }
        /* FIX: Do not free data in the source */
    }
    else
    {
        data = (twoIntsStruct *)SAFEMALLOC(100*sizeof(twoIntsStruct));
        if (data == NULL) {exit(-1);}
        {
            size_t i;
            for(i = 0; i < 100; i++)
            {
                data[i].intOne = 1;
                data[i].intTwo = 2;
            }
        }
        /* FIX: Do not free data in the source */
    }
    if(globalReturnsTrueOrFalse())
    {
        /* POTENTIAL FLAW: Use of data that may have been freed */
        SAFEUAFFUNC(data);
        /* POTENTIAL INCIDENTAL - Possible memory leak here if data was not freed */
    }
    else
    {
        /* POTENTIAL FLAW: Use of data that may have been freed */
        SAFEUAFFUNC(data);
        /* POTENTIAL INCIDENTAL - Possible memory leak here if data was not freed */
    }
}

void CWE416_Use_After_Free__malloc_free_struct_12_good()
{
    goodB2G();
    goodG2B();
}


/* Below is the main(). It is only used when building this testcase on
   its own for testing or for building a binary to use in testing binary
   analysis tools. It is not used when compiling all the testcases as one
   application, which is how source code analysis tools are tested. */


int main(int argc, char * argv[])
{
    /* seed randomness */
    srand( (unsigned)time(NULL) );
    CWE416_Use_After_Free__malloc_free_struct_12_good();
    CWE416_Use_After_Free__malloc_free_struct_12_bad();
    return 0;
}
