// Independent source oracle: the unmodified CrossPlay C++ core is built beside
// this test driver. Its MIT license and exact source hashes are checked in.
#include <cstdio>
#include <cstdlib>

#include "oracle/SudokuCore.h"

static void digits(const uint8_t* v) {
  putchar('"');
  for (int i = 0; i < 81; ++i) putchar('0' + v[i]);
  putchar('"');
}
int main() {
  sudoku::Workspace work{};
  uint32_t rng = 0xC0FFEE;
  puts("return {");
  for (int level = 0; level < 4; ++level) {
    for (int trial = 0; trial < 4; ++trial) {
      if (level == 0 && trial == 0) rng = 0;
      sudoku::Puzzle p;
      if (!sudoku::generate(p, static_cast<sudoku::Level>(level), work, rng, 1000)) return 1;
      for (int ceiling = 0; ceiling <= 10; ++ceiling) {
        sudoku::Grid g;
        if (!sudoku::load(g, p.given)) return 2;
        auto r = sudoku::solve(g, static_cast<sudoku::Technique>(ceiling));
        printf("{given=");
        digits(p.given);
        printf(",solution=");
        digits(p.solution);
        printf(",grid=");
        digits(g.value);
        const auto hint = sudoku::nextHint(p.given, static_cast<sudoku::Technique>(ceiling));
        printf(
            ",seed=%d,rng=%d,level=%d,ceiling=%d,solved=%s,broken=%s,hardest=%d,steps=%d,"
            "hint={found=%s,cell=%d,digit=%d,technique=%d,unit=%d}},\n",
            static_cast<int32_t>(p.seed), static_cast<int32_t>(rng), level, ceiling, r.solved ? "true" : "false",
            r.broken ? "true" : "false", static_cast<int>(r.hardest), r.steps, hint.found ? "true" : "false",
            hint.cell + 1, hint.digit, static_cast<int>(hint.technique), hint.unit);
      }
    }
  }
  puts("}");
}
