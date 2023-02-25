/* A 'string set' is defined as a set of strings stored
 * in sorted order in a drop list.  See the class video
 * for details.
 *
 * For lists that do not exceed 2 ^ (max_width + 1)
 * elements, the add, remove, and contains functions are 
 * O(lg size) on average.  The operator= and get_elements 
 * functions are O(size).   
 * 
 * Peter Jensen
 * February 12, 2021
 */

#include "string_set.h"
#include <iostream>  // For debugging, if needed.

namespace cs3505
{
  /*******************************************************
   * string_set member function definitions
   ***************************************************** */
  
  /** Constructor:  The parameter indicates the maximum
    * width of the next pointers in drop list nodes.
    */
  string_set::string_set(int max_width)
  {
    // TODO:  Complete this constructor
  }

  
  /** Copy constructor:  Initialize this set
    * to contain exactly the same elements
    * as another set.
    */
  string_set::string_set (const string_set &other)
  {
    // TODO:  Complete this constructor
  }


  /** Destructor:  Release any memory allocated
    * for this object.
    */
  string_set::~string_set()
  {
    // TODO:  Complete this destructor to correctly deallocate 
    // any heap-allocated memory for this object.    
  }




  /* Additional public and private helper function definitions needed here */

}
