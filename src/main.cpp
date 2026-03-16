// main.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using json = nlohmann::json;
using namespace ftxui;
using std::string;
using std::vector;
namespace fs = std::filesystem;

struct Player {
  string username = "Guest";
  string password;
  int wins = 0;
  int streak = 0;
  bool isGuest = true;
};

static constexpr int WORD_LEN = 5;
static constexpr int MAX_ROWS = 6;

/////////////////////////
// Path helper methods //
/////////////////////////

fs::path FindProjectRoot() {
  fs::path current = fs::current_path();

  for (int i = 0; i < 8; ++i) {
    if (fs::exists(current / "CMakeLists.txt")) {
      return current;
    }
    if (fs::exists(current / "data" / "wordbank.json")) {
      return current;
    }
    if (!current.has_parent_path()) break;
    current = current.parent_path();
  }

  return fs::current_path();
}

fs::path PlayersFilePath() {
  return FindProjectRoot() / "data" / "players.json";
}

fs::path WordBankFilePath() {
  return FindProjectRoot() / "data" / "wordbank.json";
}

/////////////////////
// JSON IO helpers //
/////////////////////

vector<Player> LoadPlayers(const fs::path& path = PlayersFilePath()) {
  vector<Player> result;
  if (!fs::exists(path)) return result;

  std::ifstream ifs(path);
  if (!ifs) return result;

  json j;
  try {
    ifs >> j;
  } catch (...) {
    return result;
  }

  if (!j.is_object() || !j.contains("players") || !j["players"].is_array())
    return result;

  for (const auto& it : j["players"]) {
    Player p;
    p.username = it.value("username", string("Guest"));
    p.password = it.value("password", string(""));
    p.wins = it.value("wins", 0);
    p.streak = it.value("streak", 0);
    p.isGuest = false;
    result.push_back(p);
  }

  return result;
}

void SavePlayers(const vector<Player>& players,
                 const fs::path& path = PlayersFilePath()) {
  json j;
  j["players"] = json::array();

  for (const auto& p : players) {
    j["players"].push_back({
        {"username", p.username},
        {"password", p.password},
        {"wins", p.wins},
        {"streak", p.streak},
    });
  }

  std::ofstream ofs(path);
  if (!ofs) return;
  ofs << j.dump(2) << '\n';
}

vector<string> LoadWordBank(const fs::path& path = WordBankFilePath()) {
  vector<string> words;
  if (!fs::exists(path)) return words;

  std::ifstream ifs(path);
  if (!ifs) return words;

  json j;
  try {
    ifs >> j;
  } catch (...) {
    return words;
  }

  auto push_if_valid = [&](const string& s) {
    if ((int)s.size() != WORD_LEN) return;
    if (!std::all_of(s.begin(), s.end(),
                     [](unsigned char c) { return std::isalpha(c); }))
      return;

    string t = s;
    for (auto& c : t)
      c = (char)std::toupper(static_cast<unsigned char>(c));
    words.push_back(t);
  };

  if (j.is_array()) {
    for (const auto& it : j)
      if (it.is_string()) push_if_valid(it.get<string>());
  } else if (j.is_object() && j.contains("words") && j["words"].is_array()) {
    for (const auto& it : j["words"])
      if (it.is_string()) push_if_valid(it.get<string>());
  }

  return words;
}

/////////////////////
// Game primitives //
/////////////////////

enum class State { BLANK, CORRECT, MISPLACE, INCORRECT };

struct Tile {
  char ch = ' ';
  State state = State::BLANK;
};

struct Row {
  std::array<Tile, WORD_LEN> tiles;

  Elements RenderTiles() const {
    Elements elems;
    for (const auto& t : tiles) {
      std::string s(1, (t.ch == ' ' ? ' ' : t.ch));
      Color bg = Color::GrayDark;

      switch (t.state) {
        case State::CORRECT:
          bg = Color::Green;
          break;
        case State::MISPLACE:
          bg = Color::Gold1;
          break;
        case State::INCORRECT:
          bg = Color::Red;
          break;
        default:
          break;
      }

      elems.push_back(text(s) | bold | center | size(WIDTH, EQUAL, 7) |
                      size(HEIGHT, EQUAL, 3) | bgcolor(bg) |
                      color(Color::White) | borderEmpty);
    }
    return elems;
  }
};

static std::mt19937& GlobalRng() {
  static std::mt19937 rng(
      (unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
  return rng;
}

string PickRandomWord(const vector<string>& words) {
  if (words.empty()) return string(WORD_LEN, 'A');
  std::uniform_int_distribution<size_t> dist(0, words.size() - 1);
  return words[dist(GlobalRng())];
}

void EvaluateGuess(Row& row, string target) {
  string guess;
  guess.reserve(WORD_LEN);
  for (auto& t : row.tiles) guess.push_back(t.ch);

  for (int i = 0; i < WORD_LEN; ++i) {
    if (guess[i] == target[i]) {
      row.tiles[i].state = State::CORRECT;
      target[i] = '_';
    }
  }

  for (int i = 0; i < WORD_LEN; ++i) {
    if (row.tiles[i].state == State::CORRECT) continue;

    auto pos = target.find(guess[i]);
    if (pos != string::npos) {
      row.tiles[i].state = State::MISPLACE;
      target[pos] = '_';
    } else {
      row.tiles[i].state = State::INCORRECT;
    }
  }
}

/////////////////////
// FTXUI - Game UI //
/////////////////////

bool RunGameFTXUI(const vector<string>& wordBank, Player& player) {
  if (wordBank.empty()) return false;

  string target = PickRandomWord(wordBank);
  std::array<Row, MAX_ROWS> rows;
  int currentRow = 0;
  int currentCol = 0;
  bool gameOver = false;
  bool won = false;
  string message = "";

  auto screen = ScreenInteractive::Fullscreen();

  auto renderer = Renderer([&] {
    Elements board;
    for (int r = 0; r < MAX_ROWS; ++r) {
      board.push_back(hbox(rows[r].RenderTiles()) | center);
    }

    return vbox({
               text(player.isGuest ? "Playing as Guest"
                                   : "Player: " + player.username) |
                   center,
               separator(),
               vbox(board) | center,
               separator(),
               text(message) | center,
           }) |
           center;
  });

  auto component = CatchEvent(renderer, [&](Event event) -> bool {
    if (gameOver) {
      if (event == Event::Return) {
        screen.ExitLoopClosure()();
        return true;
      }
      return true;
    }

    if (event.is_character() && currentCol < WORD_LEN) {
      string s = event.character();
      if (!s.empty()) {
        unsigned char uc = static_cast<unsigned char>(s[0]);
        if (std::isalpha(uc)) {
          rows[currentRow].tiles[currentCol].ch =
              (char)std::toupper(static_cast<unsigned char>(uc));
          currentCol++;
          return true;
        }
      }
    }

    if (event == Event::Backspace && currentCol > 0) {
      currentCol--;
      rows[currentRow].tiles[currentCol].ch = ' ';
      return true;
    }

    if (event == Event::Return && currentCol == WORD_LEN) {
      string guess;
      guess.reserve(WORD_LEN);
      for (auto& t : rows[currentRow].tiles) guess.push_back(t.ch);

      EvaluateGuess(rows[currentRow], target);

      if (guess == target) {
        gameOver = true;
        won = true;
        message = "You got it! Press Enter to continue.";
      } else {
        currentRow++;
        currentCol = 0;

        if (currentRow >= MAX_ROWS) {
          gameOver = true;
          won = false;
          message =
              "The word was: " + target + ". Press Enter to continue.";
        }
      }
      return true;
    }

    return false;
  });

  screen.Loop(component);

  if (!player.isGuest) {
    if (won) {
      player.wins++;
      player.streak++;
    } else {
      player.streak = 0;
    }
  }

  return won;
}

////////////////////////////////
// After-game options dialog  //
////////////////////////////////

// Returns 0 = Play Again, 1 = Return to Menu
int ShowAfterGameOptions(bool isGuest, const string& username) {
  int choice = -1;
  auto screen = ScreenInteractive::Fullscreen();

  auto play_again =
      Button("Play Again", [&] { choice = 0; screen.ExitLoopClosure()(); });
  auto return_menu =
      Button("Return to Menu", [&] { choice = 1; screen.ExitLoopClosure()(); });

  auto container = Container::Horizontal({play_again, return_menu});

  auto renderer = Renderer(container, [&] {
    return vbox({
               text(isGuest ? "Guest session finished"
                            : "Session finished for: " + username) |
                   bold | center,
               separator(),
               hbox(play_again->Render(), separator(), return_menu->Render()) |
                   center,
           }) |
           center | border;
  });

  screen.Loop(renderer);
  return choice;
}

/////////////////////
// FTXUI - Dialogs //
/////////////////////

std::optional<std::pair<string, string>> LoginDialog() {
  auto screen = ScreenInteractive::Fullscreen();

  string username, password;
  auto username_input = Input(&username, "username");
  auto password_input = Input(&password, "password");

  auto okay = Button("Okay", screen.ExitLoopClosure());
  auto cancel = Button("Cancel", screen.ExitLoopClosure());

  auto form = Container::Vertical({
      username_input,
      password_input,
      Container::Horizontal({okay, cancel}),
  });

  auto renderer = Renderer(form, [&] {
    return vbox({
               text("Login") | bold | center,
               separator(),
               hbox(text("Username: "), username_input->Render()) | center,
               hbox(text("Password: "), password_input->Render()) | center,
               hbox(okay->Render(), separator(), cancel->Render()) | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);

  if (username.empty() || password.empty()) return std::nullopt;
  return std::pair<string, string>{username, password};
}

std::optional<std::pair<string, string>> SignupDialog() {
  auto screen = ScreenInteractive::Fullscreen();

  string username, password;
  auto username_input = Input(&username, "username");
  auto password_input = Input(&password, "password");

  auto create = Button("Create", screen.ExitLoopClosure());
  auto cancel = Button("Cancel", screen.ExitLoopClosure());

  auto form = Container::Vertical({
      username_input,
      password_input,
      Container::Horizontal({create, cancel}),
  });

  auto renderer = Renderer(form, [&] {
    return vbox({
               text("Sign Up") | bold | center,
               separator(),
               hbox(text("Username: "), username_input->Render()) | center,
               hbox(text("Password: "), password_input->Render()) | center,
               hbox(create->Render(), separator(), cancel->Render()) | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);

  if (username.empty() || password.empty()) return std::nullopt;
  return std::pair<string, string>{username, password};
}

void ShowMessageDialog(const string& message) {
  auto screen = ScreenInteractive::Fullscreen();
  auto okay = Button("Okay", screen.ExitLoopClosure());

  auto renderer = Renderer(okay, [&] {
    return vbox({
               text(message) | center,
               separator(),
               okay->Render() | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);
}

void ShowLeaderboardDialog(const vector<Player>& players) {
  auto screen = ScreenInteractive::Fullscreen();

  vector<Player> sorted = players;
  std::sort(sorted.begin(), sorted.end(),
            [](const Player& a, const Player& b) {
              if (a.streak != b.streak) return a.streak > b.streak;
              return a.wins > b.wins;
            });

  Elements lines;
  int n = std::min((int)sorted.size(), 5);

  if (n == 0) {
    lines.push_back(text("No players yet.") | center);
  } else {
    for (int i = 0; i < n; ++i) {
      lines.push_back(
          text(std::to_string(i + 1) + ". " + sorted[i].username +
               "  |  Streak: " + std::to_string(sorted[i].streak) +
               "  |  Wins: " + std::to_string(sorted[i].wins)) |
          center);
    }
  }

  auto okay = Button("Okay", screen.ExitLoopClosure());

  auto renderer = Renderer(okay, [&] {
    return vbox({
               text("Leaderboard") | bold | center,
               separator(),
               vbox(lines) | center,
               separator(),
               okay->Render() | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);
}

/////////////////////
// Session helpers //
/////////////////////

void SaveCurrentPlayer(Player& currentPlayer, vector<Player>& players) {
  if (currentPlayer.isGuest) return;

  auto it = std::find_if(players.begin(), players.end(),
                         [&](const Player& p) {
                           return p.username == currentPlayer.username;
                         });

  if (it != players.end())
    *it = currentPlayer;
  else
    players.push_back(currentPlayer);

  SavePlayers(players);
}

// Stays logged in if account user returns to menu.
// Guest stays guest.
void PlaySessionLoop(Player& currentPlayer,
                     vector<Player>& players,
                     const vector<string>& wordBank) {
  while (true) {
    RunGameFTXUI(wordBank, currentPlayer);
    SaveCurrentPlayer(currentPlayer, players);

    int choice =
        ShowAfterGameOptions(currentPlayer.isGuest, currentPlayer.username);

    if (choice == 0) {
      continue;
    } else {
      return;
    }
  }
}

/////////////////////
// main loop       //
/////////////////////

int main() {
  auto wordBank = LoadWordBank();
  auto players = LoadPlayers();
  Player currentPlayer{"Guest", "", 0, 0, true};

  while (true) {
    int menu_choice = -1;
    auto screen = ScreenInteractive::Fullscreen();

    if (currentPlayer.isGuest) {
      auto login_btn =
          Button("Login", [&] { menu_choice = 0; screen.ExitLoopClosure()(); });
      auto signup_btn = Button("Sign Up", [&] {
        menu_choice = 1;
        screen.ExitLoopClosure()();
      });
      auto guest_btn = Button("Play as Guest", [&] {
        menu_choice = 2;
        screen.ExitLoopClosure()();
      });
      auto leaderboard_btn = Button("Leaderboard", [&] {
        menu_choice = 3;
        screen.ExitLoopClosure()();
      });
      auto exit_btn =
          Button("Exit", [&] { menu_choice = 4; screen.ExitLoopClosure()(); });

      auto menu = Container::Vertical({
          login_btn,
          signup_btn,
          guest_btn,
          leaderboard_btn,
          exit_btn,
      });

      auto renderer = Renderer(menu, [&] {
        return vbox({
                   text("=== WORDLE ===") | bold | center,
                   separator(),
                   text("Not logged in") | center,
                   (text("") | size(HEIGHT, EQUAL, 1)),
                   vbox({
                       login_btn->Render(),
                       signup_btn->Render(),
                       guest_btn->Render(),
                       leaderboard_btn->Render(),
                       exit_btn->Render(),
                   }) |
                       center,
               }) |
               center;
      });

      screen.Loop(renderer);

      switch (menu_choice) {
        case 0: {  // Login
          auto res = LoginDialog();
          if (res) {
            auto [u, p] = *res;
            auto it = std::find_if(players.begin(), players.end(),
                                   [&](const Player& pp) {
                                     return pp.username == u &&
                                            pp.password == p;
                                   });

            if (it != players.end()) {
              currentPlayer = *it;
              ShowMessageDialog("Logged in as: " + currentPlayer.username);
            } else {
              ShowMessageDialog("Invalid username or password.");
            }
          }
          break;
        }

        case 1: {  // Sign up
          auto res = SignupDialog();
          if (res) {
            auto [u, p] = *res;

            bool okname =
                !u.empty() && u.size() <= 20 &&
                std::all_of(u.begin(), u.end(), [](unsigned char c) {
                  return std::isalnum(c) || c == '_';
                });

            bool taken = std::any_of(players.begin(), players.end(),
                                     [&](const Player& pp) {
                                       return pp.username == u;
                                     });

            if (!okname) {
              ShowMessageDialog(
                  "Invalid username. Letters, numbers, underscores only.");
            } else if (taken) {
              ShowMessageDialog("Username already taken.");
            } else {
              Player np;
              np.username = u;
              np.password = p;
              np.isGuest = false;

              players.push_back(np);
              SavePlayers(players);
              currentPlayer = np;

              ShowMessageDialog("Account created. Logged in as: " +
                                currentPlayer.username);
            }
          }
          break;
        }

        case 2: {  // Play as Guest
          currentPlayer = Player{"Guest", "", 0, 0, true};
          PlaySessionLoop(currentPlayer, players, wordBank);
          break;
        }

        case 3:
          players = LoadPlayers();
          ShowLeaderboardDialog(players);
          break;

        case 4:
          return 0;

        default:
          break;
      }

    } else {
      auto play_btn =
          Button("Play", [&] { menu_choice = 10; screen.ExitLoopClosure()(); });
      auto leaderboard_btn = Button("Leaderboard", [&] {
        menu_choice = 11;
        screen.ExitLoopClosure()();
      });
      auto logout_btn = Button("Log Out", [&] {
        menu_choice = 12;
        screen.ExitLoopClosure()();
      });
      auto exit_btn =
          Button("Exit", [&] { menu_choice = 13; screen.ExitLoopClosure()(); });

      auto menu = Container::Vertical({
          play_btn,
          leaderboard_btn,
          logout_btn,
          exit_btn,
      });

      auto renderer = Renderer(menu, [&] {
        return vbox({
                   text("=== WORDLE ===") | bold | center,
                   separator(),
                   text("Logged in as: " + currentPlayer.username) | bold |
                       center,
                   (text("") | size(HEIGHT, EQUAL, 1)),
                   vbox({
                       play_btn->Render(),
                       leaderboard_btn->Render(),
                       logout_btn->Render(),
                       exit_btn->Render(),
                   }) |
                       center,
               }) |
               center;
      });

      screen.Loop(renderer);

      switch (menu_choice) {
        case 10:
          PlaySessionLoop(currentPlayer, players, wordBank);
          break;

        case 11:
          players = LoadPlayers();
          ShowLeaderboardDialog(players);
          break;

        case 12:
          currentPlayer = Player{"Guest", "", 0, 0, true};
          ShowMessageDialog("Logged out.");
          break;

        case 13:
          return 0;

        default:
          break;
      }
    }
  }

  return 0;
}