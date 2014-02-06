#include "kaleidoscope.yy.hpp"
#include "driver.hpp"

int main()
{
  driver context;
  yy::parser parser(context);
  int result = parser.parse();

  return result;
}

