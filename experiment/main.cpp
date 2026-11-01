#include <cstdio>

struct Base {};

struct Derived {
  int speed;
  Base member;
};

int main() {

  printf("\n%ld\n\n", sizeof(Derived));

  return 0;
}