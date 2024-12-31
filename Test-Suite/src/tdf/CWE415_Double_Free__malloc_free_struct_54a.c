/* TEMPLATE GENERATED TESTCASE FILE
Filename: CWE415_Double_Free__malloc_free_struct_54a.c
Label Definition File: CWE415_Double_Free__malloc_free.label.xml
Template File: sources-sinks-54a.tmpl.c
*/
/*
 * @description
 * CWE: 415 Double Free
 * BadSource:  Allocate data using malloc() and Deallocate data using free()
 * GoodSource: Allocate data using malloc()
 * Sinks:
 *    GoodSink: do nothing
 *    BadSink : Deallocate data using free()
 * Flow Variant: 54 Data flow: data passed as an argument from one function through three others to a fifth; all five functions are in different source files
 *
 * */
#include "aliascheck.h"
#include "std_testcase.h"

#include <wchar.h>

#ifndef OMITBAD

void CWE415_Double_Free__malloc_free_struct_54e_badSink(twoIntsStruct * data)
{
    /* POTENTIAL FLAW: Possibly freeing memory twice */
    DOUBLEFREE(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_struct_54e_goodG2BSink(twoIntsStruct * data)
{
    /* POTENTIAL FLAW: Possibly freeing memory twice */
    SAFEFREE(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_struct_54e_goodB2GSink(twoIntsStruct * data)
{
    /* do nothing */
    /* FIX: Don't attempt to free the memory */
    ; /* empty statement needed for some flow variants */
}

#endif /* OMITGOOD */

#ifndef OMITBAD

/* bad function declaration */
void CWE415_Double_Free__malloc_free_struct_54e_badSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54d_badSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54e_badSink(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_struct_54e_goodG2BSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54d_goodG2BSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54e_goodG2BSink(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_struct_54e_goodB2GSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54d_goodB2GSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54e_goodB2GSink(data);
}

#endif /* OMITGOOD */

#ifndef OMITBAD

/* bad function declaration */
void CWE415_Double_Free__malloc_free_struct_54d_badSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54c_badSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54d_badSink(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_struct_54d_goodG2BSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54c_goodG2BSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54d_goodG2BSink(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_struct_54d_goodB2GSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54c_goodB2GSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54d_goodB2GSink(data);
}

#endif /* OMITGOOD */

#ifndef OMITBAD

/* bad function declaration */
void CWE415_Double_Free__malloc_free_struct_54c_badSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54b_badSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54c_badSink(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_struct_54c_goodG2BSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54b_goodG2BSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54c_goodG2BSink(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_struct_54c_goodB2GSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54b_goodB2GSink(twoIntsStruct * data)
{
    CWE415_Double_Free__malloc_free_struct_54c_goodB2GSink(data);
}

#endif /* OMITGOOD */

#ifndef OMITBAD

/* bad function declaration */
void CWE415_Double_Free__malloc_free_struct_54b_badSink(twoIntsStruct * data);

void CWE415_Double_Free__malloc_free_struct_54_bad()
{
    twoIntsStruct * data;
    /* Initialize data */
    data = NULL;
    data = (twoIntsStruct *)DOUBLEFREEMALLOC(100*sizeof(twoIntsStruct));
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    CWE415_Double_Free__malloc_free_struct_54b_badSink(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_struct_54b_goodG2BSink(twoIntsStruct * data);

static void goodG2B()
{
    twoIntsStruct * data;
    /* Initialize data */
    data = NULL;
    data = (twoIntsStruct *)SAFEMALLOC(100*sizeof(twoIntsStruct));
    /* FIX: Do NOT free data in the source - the bad sink frees data */
    CWE415_Double_Free__malloc_free_struct_54b_goodG2BSink(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_struct_54b_goodB2GSink(twoIntsStruct * data);

static void goodB2G()
{
    twoIntsStruct * data;
    /* Initialize data */
    data = NULL;
    data = (twoIntsStruct *)SAFEMALLOC(100*sizeof(twoIntsStruct));
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    CWE415_Double_Free__malloc_free_struct_54b_goodB2GSink(data);
}

void CWE415_Double_Free__malloc_free_struct_54_good()
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
    CWE415_Double_Free__malloc_free_struct_54_good();
    printLine("Finished good()");
#endif /* OMITGOOD */
#ifndef OMITBAD
    printLine("Calling bad()...");
    CWE415_Double_Free__malloc_free_struct_54_bad();
    printLine("Finished bad()");
#endif /* OMITBAD */
    return 0;
}

#endif
