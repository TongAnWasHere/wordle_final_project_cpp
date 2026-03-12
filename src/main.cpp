#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <random>
#include <fstream>

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


string GetRandomWord(const vector<string>& words) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, words.size() - 1);
    string word = words[dis(gen)];
    // Convert to uppercase to match user input easily
    for(char &c : word) c = toupper(c); 
    return word;
}

void CheckGuess(Row& row, string target) {
    string guess = "";
    for(auto& tile : row.tiles) guess += tile.first;

    // First pass: Find Correct (Green)
    for (int i = 0; i < 5; i++) {
        if (guess[i] == target[i]) {
            row.tiles[i].second = State::CORRECT;
            target[i] = '_'; // Mark as used so we don't double-count for yellow
        }
    }

    // Second pass: Find Misplaced (Gold)
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

  vector<string> wordList;
  ifstream file("../../data/wordbank.json"); 
  string line;
  while (getline(file, line)) {
    if (line.find('"') != string::npos) {
      size_t first = line.find('"') + 1;
      size_t last = line.find('"', first);
      string word = line.substr(first, last - first);
      if (word.size() == 5)
        wordList.push_back(word);
    }
  }

  string target = GetRandomWord(wordList);

  Row rows[6];
  int currentRow = 0; 
  bool won = false;

  while (currentRow < 6 && !won) {

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

  cout << "\nGuess " << (currentRow + 1) << "/6: ";
  string guess;
  cin >> guess;
  for (char& c : guess) c = toupper(c);

  if (guess.size() != 5) {
    cout << "Please enter a 5-letter word!\n";
    continue;
  }

  for (int i = 0; i < 5; i++)
    rows[currentRow].tiles[i].first = string(1, guess[i]);

  CheckGuess(rows[currentRow], target);

   if (guess == target) won = true;
    currentRow++;

  }

  auto board = vbox({
    rows[0].Render(), rows[1].Render(),
    rows[2].Render(), rows[3].Render(),
    rows[4].Render(), rows[5].Render(),
  }) | center;
  auto screen = Screen::Create(Dimension::Fixed(50));
  Render(screen, board);
  screen.Print();


  if (won)
    cout << "\n🎉 You got it!\n";
  else
    cout << "\n😞 The word was: " << target << "\n";


  return 0;
}