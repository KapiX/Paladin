/*******************************************************************************//**
 *
 *  @file        kw_subst.c
 *
 *  <Description>
 *
 ***********************************************************************************/
#include <string>

/***********************************************************************************
  *  foo1
  *******************************************************************************//**
  *
  *  <Description>
  *
  *  @return TODO
  *
  **********************************************************************************/
int foo1()
{
}



/** header comment */
#if 2
    int foo2(void)
    {
    }



#endif

#if 1
    /***********************************************************************************
      *  foo3
      *******************************************************************************//**
      *
      *  <Description>
      *
      *  @param a TODO
      *
      **********************************************************************************/
    void foo3(int a)
    {
    }



#endif

/***********************************************************************************
  *  foo4
  *******************************************************************************//**
  *
  *  <Description>
  *
  *  @param a TODO
  * @param b TODO
  * @param c TODO
  * @return TODO
  *
  **********************************************************************************/
void * foo4(int a, int b, int c)
{
}



/**
  * CVS History:
  * $Log $
  *
  */

