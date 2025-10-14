#include <stacktrace>
#include <string>

int main()
{
  (void)std::to_string(std::stacktrace::current());
}
