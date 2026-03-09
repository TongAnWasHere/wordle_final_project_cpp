#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <string>
#include <utility>

using namespace std;
using namespace ftxui;

enum class State { BLANK, CORRECT, MISPLACE, INCORRECT };

Color TileColor(State s) {
  switch (s) {
  case State::CORRECT:
    return Color::Green;
  case State::MISPLACE:
    return Color::Gold1;
  case State::INCORRECT:
    return Color::Red;
  default:
    return Color::GrayDark;
  }
}

Element Tile(string c, State s) {
  return text(c) | bold | center | size(WIDTH, EQUAL, 7) |
         size(HEIGHT, EQUAL, 3) | bgcolor(TileColor(s)) | color(Color::White) |
         borderEmpty;
}

struct Row {
  array<pair<string, State>, 5> tiles;

  Row() {
    tiles.fill({" ", State::BLANK});
  }

  Element Render() {
    Elements elems;
    for (auto &[c, s] : tiles)
      elems.push_back(Tile(c, s));
    return hbox(elems);
  }
};

int main() {
  Row rows[6];

  auto board = vbox({
                   rows[0].Render(),
                   rows[1].Render(),
                   rows[2].Render(),
                   rows[3].Render(),
                   rows[4].Render(),
                   rows[5].Render(),
               }) |
               center;

  auto screen = Screen::Create(Dimension::Full());
  Render(screen, board);
  screen.Print();
  return 0;
}