/* TEMPLATE GENERATED TESTCASE FILE
Filename: CWE415_Double_Free__malloc_free_wchar_t_34.c
Label Definition File: CWE415_Double_Free__malloc_free.label.xml
Template File: sources-sinks-34.tmpl.c
*/
/*
 * @description
 * CWE: 415 Double Free
 * BadSource:  Allocate data using malloc() and Deallocate data using free()
 * GoodSource: Allocate data using malloc()
 * Sinks:
 *    GoodSink: do nothing
 *    BadSink : Deallocate data using free()
 * Flow Variant: 34 Data flow: use of a union containing two methods of accessing the same data (within the same function)
 *
 * */
#include "aliascheck.h"
#include "std_testcase.h"

#include <wchar.h>

typedef union
{
    wchar_t * unionFirst;
    wchar_t * unionSecond;
} CWE415_Double_Free__malloc_free_wchar_t_34_unionType;

#ifndef OMITBAD

void CWE415_Double_Free__malloc_free_wchar_t_34_bad()
{
    wchar_t * data;
    CWE415_Double_Free__malloc_free_wchar_t_34_unionType myUnion;
    /* Initialize data */
    data = NULL;
    data = (wchar_t *)DOUBLEFREEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    myUnion.unionFirst = data;
    {
        wchar_t * data = myUnion.unionSecond;
        /* POTENTIAL FLAW: Possibly freeing memory twice */
        DOUBLEFREE(data);
    }
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B() uses the GoodSource with the BadSink */
static void goodG2B()
{
    wchar_t * data;
    CWE415_Double_Free__malloc_free_wchar_t_34_unionType myUnion;
    /* Initialize data */
    data = NULL;
    data = (wchar_t *)SAFEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* FIX: Do NOT free data in the source - the bad sink frees data */
    myUnion.unionFirst = data;
    {
        wchar_t * data = myUnion.unionSecond;
        /* POTENTIAL FLAW: Possibly freeing memory twice */
        SAFEFREE(data);
    }
}

/* goodB2G() uses the BadSource with the GoodSink */
static void goodB2G()
{
    wchar_t * data;
    CWE415_Double_Free__malloc_free_wchar_t_34_unionType myUnion;
    /* Initialize data */
    data = NULL;
    data = (wchar_t *)SAFEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    myUnion.unionFirst = data;
    {
        wchar_t * data = myUnion.unionSecond;
        /* do nothing */
        /* FIX: Don't attempt to free the memory */
        ; /* empty statement needed for some flow variants */
    }
}

void CWE415_Double_Free__malloc_free_wchar_t_34_good()
{
    goodG2B();
    goodB2G();
}

#endif /* OMITGOOD */

/* Below is the main(). It is only used when building this testcase on
   its own for testing or for building a binary to use in testing binary
   analysis tools. It is not used when compiling all the testcases as one
   application, which is how source code analysis tools are tested. */
#ifdef INCLUDEMAIN

int main(int argc, char * argv[])
{
    /* seed randomness */
    srand( (unsigned)time(NULL) );
#ifndef OMITGOOD
    printLine("Calling good()...");
    CWE415_Double_Free__malloc_free_wchar_t_34_good();
    printLine("Finished good()");
#endif /* OMITGOOD */
#ifndef OMITBAD
    printLine("Calling bad()...");
    CWE415_Double_Free__malloc_free_wchar_t_34_bad();
    printLine("Finished bad()");
#endif /* OMITBAD */
    return 0;
}

#endif
