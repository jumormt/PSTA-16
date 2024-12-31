/* TEMPLATE GENERATED TESTCASE FILE
Filename: CWE415_Double_Free__malloc_free_long_53a.c
Label Definition File: CWE415_Double_Free__malloc_free.label.xml
Template File: sources-sinks-53a.tmpl.c
*/
/*
 * @description
 * CWE: 415 Double Free
 * BadSource:  Allocate data using malloc() and Deallocate data using free()
 * GoodSource: Allocate data using malloc()
 * Sinks:
 *    GoodSink: do nothing
 *    BadSink : Deallocate data using free()
 * Flow Variant: 53 Data flow: data passed as an argument from one function through two others to a fourth; all four functions are in different source files
 *
 * */
#include "aliascheck.h"
#include "std_testcase.h"

#include <wchar.h>

#ifndef OMITBAD

void CWE415_Double_Free__malloc_free_long_53d_badSink(long * data)
{
    /* POTENTIAL FLAW: Possibly freeing memory twice */
    DOUBLEFREE(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_long_53d_goodG2BSink(long * data)
{
    /* POTENTIAL FLAW: Possibly freeing memory twice */
    SAFEFREE(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_long_53d_goodB2GSink(long * data)
{
    /* do nothing */
    /* FIX: Don't attempt to free the memory */
    ; /* empty statement needed for some flow variants */
}

#endif /* OMITGOOD */

#ifndef OMITBAD

/* bad function declaration */
void CWE415_Double_Free__malloc_free_long_53d_badSink(long * data);

void CWE415_Double_Free__malloc_free_long_53c_badSink(long * data)
{
    CWE415_Double_Free__malloc_free_long_53d_badSink(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_long_53d_goodG2BSink(long * data);

void CWE415_Double_Free__malloc_free_long_53c_goodG2BSink(long * data)
{
    CWE415_Double_Free__malloc_free_long_53d_goodG2BSink(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_long_53d_goodB2GSink(long * data);

void CWE415_Double_Free__malloc_free_long_53c_goodB2GSink(long * data)
{
    CWE415_Double_Free__malloc_free_long_53d_goodB2GSink(data);
}

#endif /* OMITGOOD */

#ifndef OMITBAD

/* bad function declaration */
void CWE415_Double_Free__malloc_free_long_53c_badSink(long * data);

void CWE415_Double_Free__malloc_free_long_53b_badSink(long * data)
{
    CWE415_Double_Free__malloc_free_long_53c_badSink(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_long_53c_goodG2BSink(long * data);

void CWE415_Double_Free__malloc_free_long_53b_goodG2BSink(long * data)
{
    CWE415_Double_Free__malloc_free_long_53c_goodG2BSink(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_long_53c_goodB2GSink(long * data);

void CWE415_Double_Free__malloc_free_long_53b_goodB2GSink(long * data)
{
    CWE415_Double_Free__malloc_free_long_53c_goodB2GSink(data);
}

#endif /* OMITGOOD */

#ifndef OMITBAD

/* bad function declaration */
void CWE415_Double_Free__malloc_free_long_53b_badSink(long * data);

void CWE415_Double_Free__malloc_free_long_53_bad()
{
    long * data;
    /* Initialize data */
    data = NULL;
    data = (long *)DOUBLEFREEMALLOC(100*sizeof(long));
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    CWE415_Double_Free__malloc_free_long_53b_badSink(data);
}

#endif /* OMITBAD */

#ifndef OMITGOOD

/* goodG2B uses the GoodSource with the BadSink */
void CWE415_Double_Free__malloc_free_long_53b_goodG2BSink(long * data);

static void goodG2B()
{
    long * data;
    /* Initialize data */
    data = NULL;
    data = (long *)SAFEMALLOC(100*sizeof(long));
    /* FIX: Do NOT free data in the source - the bad sink frees data */
    CWE415_Double_Free__malloc_free_long_53b_goodG2BSink(data);
}

/* goodB2G uses the BadSource with the GoodSink */
void CWE415_Double_Free__malloc_free_long_53b_goodB2GSink(long * data);

static void goodB2G()
{
    long * data;
    /* Initialize data */
    data = NULL;
    data = (long *)SAFEMALLOC(100*sizeof(long));
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    CWE415_Double_Free__malloc_free_long_53b_goodB2GSink(data);
}

void CWE415_Double_Free__malloc_free_long_53_good()
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
    CWE415_Double_Free__malloc_free_long_53_good();
    printLine("Finished good()");
#endif /* OMITGOOD */
#ifndef OMITBAD
    printLine("Calling bad()...");
    CWE415_Double_Free__malloc_free_long_53_bad();
    printLine("Finished bad()");
#endif /* OMITBAD */
    return 0;
}

#endif
