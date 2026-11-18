// Test oracle links the original, unmodified CrossPlay MIT C++ core/engine.
#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "ChessCore.h"
#include "ChessEngine.h"
using namespace chess;
static void emit(Position& p, int id, int depth) {
  char fen[100];
  positionToFen(p, fen);
  printf("CASE\t%d\t%s\t%d\t%d\t%llu\n", id, fen, evaluate(p), isInCheck(p), (unsigned long long)perft(p, depth));
  printf("ATTACK\t");
  for (int c = 0; c < 2; ++c)
    for (int sq = 0; sq < 64; ++sq) putchar(isSquareAttacked(p, sq, c == 1) ? '1' : '0');
  putchar('\n');
  MoveList list;
  generateLegalMoves(p, list);
  for (int i = 0; i < list.count; ++i) {
    Move m = list.moves[i];
    Undo u;
    char name[10];
    moveToString(m, name);
    makeMove(p, m, u);
    positionToFen(p, fen);
    unmakeMove(p, m, u);
    printf("MOVE\t%s\t%d\t%d\t%d\t%d\t%s\n", name, m.from, m.to, m.promotion, m.flags, fen);
  }
  SearchBuffers b;
  for (int d : {0, 1, 2, 4, 9})
    for (uint32_t budget : {uint32_t(0), uint32_t(17)}) {
      if ((d == 4 || d == 9) && budget == 0) continue;
      auto r = search(p, d, b, budget);
      char name[10] = "-";
      if (r.hasMove) moveToString(r.best, name);
      printf("SEARCH\t%d\t%u\t%s\t%d\t%u\t%d\t%d\n", d, budget, name, r.score, r.nodes, r.budgetExhausted, r.hasMove);
    }
  puts("END");
}
int main() {
  const char* cases[] = {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                         "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                         "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                         "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                         "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                         "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
                         "7k/P7/8/8/8/8/7p/K7 w - - 0 1",
                         "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
                         "k7/8/8/4KPpr/8/8/8/8 w - g6 0 1",
                         "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",
                         "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1",
                         "7k/8/5KQ1/8/8/8/8/8 w - - 99 41"};
  int id = 0;
  for (auto fen : cases) {
    Position p;
    if (!parseFen(fen, p)) return 2;
    emit(p, id++, 2);
  }
  uint32_t rng = 0x61587231;
  for (int game = 0; game < 3; ++game) {
    Position p;
    setStartPosition(p);
    for (int ply = 0; ply < 100; ++ply) {
      MoveList list;
      generateLegalMoves(p, list);
      if (!list.count) break;
      rng ^= rng << 13;
      rng ^= rng >> 17;
      rng ^= rng << 5;
      Undo u;
      makeMove(p, list.moves[rng % list.count], u);
      if (ply % 5 == 4) emit(p, id++, 1);
    }
  }
}
