#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>
using namespace std;
using namespace ftxui;

// ── Players
// ───────────────────────────────────────────────────────────────────

struct Player {
  string username = "Guest";
  string password = "";
  int wins = 0;
  int streak = 0;
  bool isGuest = true;
};

vector<Player> LoadPlayers() {
  vector<Player> result;
  ifstream f("players.json");
  if (!f)
    return result;

  auto getStr = [](const string &line, const string &key) -> string {
    size_t k = line.find("\"" + key + "\"");
    if (k == string::npos)
      return "";
    size_t q1 = line.find('"', line.find(':', k) + 1);
    size_t q2 = line.find('"', q1 + 1);
    return line.substr(q1 + 1, q2 - q1 - 1);
  };

  auto getInt = [](const string &line, const string &key) -> int {
    size_t k = line.find("\"" + key + "\"");
    if (k == string::npos)
      return 0;
    size_t i = line.find(':', k) + 1;
    while (i < line.size() && !isdigit(line[i]))
      i++;
    size_t j = i;
    while (j < line.size() && isdigit(line[j]))
      j++;
    return i == j ? 0 : stoi(line.substr(i, j - i));
  };

  string line;
  while (getline(f, line)) {
    if (line.find("\"username\"") == string::npos)
      continue;
    Player p;
    p.isGuest = false;
    p.username = getStr(line, "username");
    p.password = getStr(line, "password");
    p.wins = getInt(line, "wins");
    p.streak = getInt(line, "streak");
    result.push_back(p);
  }
  return result;
}

void SavePlayers(const vector<Player> &players) {
  ofstream f("players.json");
  f << "{" << endl << "  \"players\": [" << endl;
  for (size_t i = 0; i < players.size(); i++) {
    const auto &p = players[i];
    f << "    {\"username\": \"" << p.username << "\", \"password\": \""
      << p.password << "\", \"wins\": " << p.wins
      << ", \"streak\": " << p.streak << "}";
    if (i + 1 < players.size())
      f << ",";
    f << endl;
  }
  f << "  ]" << endl << "}" << endl;
}

void ShowLeaderboard(const vector<Player> &players) {
  vector<Player> sorted = players;
  sort(sorted.begin(), sorted.end(),
       [](const Player &a, const Player &b) { return a.streak > b.streak; });
  cout << endl << "===== LEADERBOARD =====" << endl;
  int n = min((int)sorted.size(), 5);
  for (int i = 0; i < n; i++)
    cout << (i + 1) << ". " << sorted[i].username
         << "  |  Streak: " << sorted[i].streak
         << "  |  Wins: " << sorted[i].wins << endl;
  if (sorted.empty())
    cout << "No players yet." << endl;
  cout << "======================================" << endl << endl;
}

// ── Game
// ──────────────────────────────────────────────────────────────────────

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
  mt19937 gen{(unsigned)chrono::steady_clock::now().time_since_epoch().count()};
  string word = words[uniform_int_distribution<>(0, words.size() - 1)(gen)];
  for (char &c : word)
    c = toupper(c);
  return word;
}

void CheckGuess(Row &row, string target) {
  string guess;
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

bool PlayGame(const vector<string> &wordBank) {
  string target = GetRandomWord(wordBank);
  Row rows[6];
  int currentRow = 0, currentCol = 0;
  string message = "";
  bool gameOver = false, won = false;

  auto screen = ScreenInteractive::Fullscreen();

  auto component = Renderer([&] {
    return vbox({
               vbox({
                   rows[0].Render(),
                   rows[1].Render(),
                   rows[2].Render(),
                   rows[3].Render(),
                   rows[4].Render(),
                   rows[5].Render(),
               }) | center,
               text(message) | center,
           }) |
           center;
  });

  component = CatchEvent(component, [&](Event event) {
    if (gameOver) {
      if (event == Event::Return)
        screen.ExitLoopClosure()();
      return true;
    }
    if (event.is_character() && currentCol < 5) {
      char c = toupper(event.character()[0]);
      if (isalpha(c)) {
        rows[currentRow].tiles[currentCol].first = string(1, c);
        currentCol++;
        return true;
      }
    }
    if (event == Event::Backspace && currentCol > 0) {
      currentCol--;
      rows[currentRow].tiles[currentCol].first = " ";
      return true;
    }
    if (event == Event::Return && currentCol == 5) {
      string guess;
      for (auto &tile : rows[currentRow].tiles)
        guess += tile.first;
      CheckGuess(rows[currentRow], target);
      currentRow++;
      currentCol = 0;
      if (guess == target) {
        message = "You got it! Press Enter to continue.";
        gameOver = won = true;
      } else if (currentRow == 6) {
        message = "The word was: " + target + ". Press Enter to continue.";
        gameOver = true;
      }
      return true;
    }
    return false;
  });

  screen.Loop(component);
  return won;
}

bool ValidUsername(const string &u) {
  if (u.empty() || u.size() > 20)
    return false;
  for (char c : u)
    if (!isalnum(c) && c != '_')
      return false;
  return true;
}

int main() {
  vector<string> wordBank;
  ifstream file("../../data/wordbank.json");
  string line;
  while (getline(file, line)) {
    size_t pos = 0;
    while ((pos = line.find('"', pos)) != string::npos) {
      size_t end = line.find('"', pos + 1);
      if (end == string::npos)
        break;
      string word = line.substr(pos + 1, end - pos - 1);
      if (word.size() == 5 && word != "words" &&
          all_of(word.begin(), word.end(), ::isalpha))
        wordBank.push_back(word);
      pos = end + 1;
    }
  }

  while (true) {
    cout << endl << "=== WORDLE ===" << endl;
    cout << "1. Login" << endl;
    cout << "2. Sign Up" << endl;
    cout << "3. Play as Guest" << endl;
    cout << "4. Exit" << endl;
    cout << "Choice: ";
    int choice;
    cin >> choice;

    if (choice == 4)
      break;

    vector<Player> players = LoadPlayers();
    Player currentPlayer;

    if (choice == 1 || choice == 2) {
      string username, password;
      cout << "Username: ";
      cin >> username;
      cout << "Password: ";
      cin >> password;

      if (choice == 2) {
        if (!ValidUsername(username)) {
          cout << "Invalid username. Letters, numbers, underscores only."
               << endl;
          continue;
        }
        bool taken =
            any_of(players.begin(), players.end(),
                   [&](const Player &p) { return p.username == username; });
        if (taken) {
          cout << "Username already taken." << endl;
          continue;
        }
        currentPlayer = {username, password, 0, 0, false};
        players.push_back(currentPlayer);
        SavePlayers(players);
        cout << "Account created!" << endl;
      } else {
        auto it = find_if(players.begin(), players.end(), [&](const Player &p) {
          return p.username == username && p.password == password;
        });
        if (it == players.end()) {
          cout << "Invalid username or password." << endl;
          continue;
        }
        currentPlayer = *it;
        cout << "Welcome back, " << currentPlayer.username << "!" << endl;
      }
    } else {
      currentPlayer = {"Guest", "", 0, 0, true};
    }

    // Session loop
    while (true) {
      system("cls");
      bool won = PlayGame(wordBank);

      if (!currentPlayer.isGuest) {
        if (won) {
          currentPlayer.wins++;
          currentPlayer.streak++;
        } else {
          currentPlayer.streak = 0;
        }

        auto it = find_if(players.begin(), players.end(), [&](const Player &p) {
          return p.username == currentPlayer.username;
        });
        if (it != players.end())
          *it = currentPlayer;
        else
          players.push_back(currentPlayer);
        SavePlayers(players);
        players = LoadPlayers();
      }

      ShowLeaderboard(players);

      cout << "1. Play Again" << endl << "2. Logout" << endl << "Choice: ";
      int endChoice;
      cin >> endChoice;
      if (endChoice != 1)
      break;
    }
  }

  return 0;
}