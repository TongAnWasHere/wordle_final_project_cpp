#include <algorithm>
#include <array>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>
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

string GetRandomWord(const vector<string> &words) {
  static mt19937 gen{random_device{}()};
  string word = words[uniform_int_distribution<>(0, words.size() - 1)(gen)];
  for (char &c : word)
    c = toupper(c);
  return word;
}

void CheckGuess(Row &row, string target) {
  string guess = "";
  for (auto &tile : row.tiles)
    guess += tile.first;
  for (int i = 0; i < 5; i++) {
    if (guess[i] == target[i]) {
      row.tiles[i].second = State::CORRECT;
      target[i] = '_';
    }
  }
  for (int i = 0; i < 5; i++) {
    if (row.tiles[i].second != State::CORRECT) {
      size_t found = target.find(guess[i]);
      if (found != string::npos) {
        row.tiles[i].second = State::MISPLACE;
        target[found] = '_';
      } else {
        row.tiles[i].second = State::INCORRECT;
      }
    }
  }
}

int main() {
  vector<string> wordBank;
  ifstream file("../../data/wordbank.json");
  string line;
  while (getline(file, line)) {
    if (line.find(':') != string::npos) continue;
    if (line.find('"') != string::npos) {
      size_t first = line.find('"') + 1;
      size_t last = line.find('"', first);
      string word = line.substr(first, last - first);
      if (word.size() == 5 && all_of(word.begin(), word.end(), ::isalpha))
        wordBank.push_back(word);
    }
  }

  string target = GetRandomWord(wordBank);

  Row rows[6];
  int currentRow = 0;

  auto RenderBoard = [&]() {
    auto board = vbox({
                     rows[0].Render(),
                     rows[1].Render(),
                     rows[2].Render(),
                     rows[3].Render(),
                     rows[4].Render(),
                     rows[5].Render(),
                 }) |
                 center;
    auto screen = Screen::Create(Dimension::Fixed(50));
    Render(screen, board);
    screen.Print();
  };

  while (currentRow < 6) {
    RenderBoard();

    cout << "\nGuess " << (currentRow + 1) << "/6: ";
    string guess;
    cin >> guess;
    for (char &c : guess)
      c = toupper(c);

    if (guess.size() != 5) {
      cout << "Please enter a 5-letter word!" << endl;
      continue;
    }

    for (int i = 0; i < 5; i++)
      rows[currentRow].tiles[i].first = string(1, guess[i]);

    CheckGuess(rows[currentRow], target);
    currentRow++;

    if (guess == target) {
      RenderBoard();
      cout << "\nYou got it!" << endl;
      return 0;
    }
  }

  RenderBoard();
  cout << "\nThe word was: " << target << endl;
  return 0;
}