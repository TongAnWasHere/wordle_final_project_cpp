#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <ctime>
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
using namespace std;
namespace fs = std::filesystem;

struct Player {
  string username;
  string password;
  int wins;
  int streak;
  bool isGuest;
  bool isAdmin;
};

struct GameResult {
  bool won;
  bool quitToMenu;
  int attemptsUsed;
};

enum class State {
  BLANK,
  CORRECT,
  MISPLACE,
  INCORRECT
};

struct Tile {
  char ch;
  State state;
};

static const int WORD_LEN = 5;
static const int MAX_ROWS = 6;
static const int WORDS_PER_PAGE = 15;

/////////////////////////
// Path helper methods //
/////////////////////////

fs::path FindProjectRoot() {
  fs::path current = fs::current_path();

  for (int i = 0; i < 8; i++) {
    if (fs::exists(current / "CMakeLists.txt")) {
      return current;
    }

    if (fs::exists(current / "data" / "wordbank.json")) {
      return current;
    }

    if (!current.has_parent_path()) {
      break;
    }

    current = current.parent_path();
  }

  return fs::current_path();
}

fs::path PlayersFilePath() {
  fs::path root = FindProjectRoot();
  return root / "data" / "players.json";
}

fs::path WordBankFilePath() {
  fs::path root = FindProjectRoot();
  return root / "data" / "wordbank.json";
}

/////////////////////
// Utility helpers //
/////////////////////

Player MakeGuestPlayer() {
  Player p;
  p.username = "Guest";
  p.password = "";
  p.wins = 0;
  p.streak = 0;
  p.isGuest = true;
  p.isAdmin = false;
  return p;
}

Player MakeAdminPlayer() {
  Player p;
  p.username = "admin";
  p.password = "admin123";
  p.wins = 0;
  p.streak = 0;
  p.isGuest = false;
  p.isAdmin = true;
  return p;
}

bool IsValidUsername(const string& username) {
  if (username.empty()) {
    return false;
  }

  if (username.size() > 20) {
    return false;
  }

  for (int i = 0; i < (int)username.size(); i++) {
    unsigned char c = (unsigned char)username[i];
    if (!isalnum(c) && c != '_') {
      return false;
    }
  }

  return true;
}

bool IsValidWord(const string& word) {
  if ((int)word.size() != WORD_LEN) {
    return false;
  }

  for (int i = 0; i < (int)word.size(); i++) {
    unsigned char c = (unsigned char)word[i];
    if (!isalpha(c)) {
      return false;
    }
  }

  return true;
}

string ToUpperWord(string word) {
  for (int i = 0; i < (int)word.size(); i++) {
    word[i] = (char)toupper((unsigned char)word[i]);
  }
  return word;
}

/////////////////////
// JSON IO helpers //
/////////////////////

vector<Player> LoadPlayers(const fs::path& path = PlayersFilePath()) {
  vector<Player> players;

  if (!fs::exists(path)) {
    return players;
  }

  ifstream ifs(path);
  if (!ifs) {
    return players;
  }

  json j;
  try {
    ifs >> j;
  } catch (...) {
    return players;
  }

  if (!j.is_object()) {
    return players;
  }

  if (!j.contains("players")) {
    return players;
  }

  if (!j["players"].is_array()) {
    return players;
  }

  for (int i = 0; i < (int)j["players"].size(); i++) {
    Player p;
    p.username = j["players"][i].value("username", "");
    p.password = j["players"][i].value("password", "");
    p.wins = j["players"][i].value("wins", 0);
    p.streak = j["players"][i].value("streak", 0);
    p.isGuest = false;
    p.isAdmin = false;
    players.push_back(p);
  }

  return players;
}

void SavePlayers(const vector<Player>& players,
                 const fs::path& path = PlayersFilePath()) {
  json j;
  j["players"] = json::array();

  for (int i = 0; i < (int)players.size(); i++) {
    if (players[i].isGuest) {
      continue;
    }

    if (players[i].isAdmin) {
      continue;
    }

    json item;
    item["username"] = players[i].username;
    item["password"] = players[i].password;
    item["wins"] = players[i].wins;
    item["streak"] = players[i].streak;

    j["players"].push_back(item);
  }

  ofstream ofs(path);
  if (!ofs) {
    return;
  }

  ofs << j.dump(2) << '\n';
}

vector<string> LoadWordBank(const fs::path& path = WordBankFilePath()) {
  vector<string> words;

  if (!fs::exists(path)) {
    return words;
  }

  ifstream ifs(path);
  if (!ifs) {
    return words;
  }

  json j;
  try {
    ifs >> j;
  } catch (...) {
    return words;
  }

  if (!j.is_object()) {
    return words;
  }

  if (!j.contains("words")) {
    return words;
  }

  if (!j["words"].is_array()) {
    return words;
  }

  for (int i = 0; i < (int)j["words"].size(); i++) {
    if (!j["words"][i].is_string()) {
      continue;
    }

    string word = j["words"][i].get<string>();
    word = ToUpperWord(word);

    if (IsValidWord(word)) {
      words.push_back(word);
    }
  }

  sort(words.begin(), words.end());
  words.erase(unique(words.begin(), words.end()), words.end());

  return words;
}

void SaveWordBank(const vector<string>& words,
                  const fs::path& path = WordBankFilePath()) {
  vector<string> sortedWords = words;
  sort(sortedWords.begin(), sortedWords.end());
  sortedWords.erase(unique(sortedWords.begin(), sortedWords.end()),
                    sortedWords.end());

  json j;
  j["words"] = json::array();

  for (int i = 0; i < (int)sortedWords.size(); i++) {
    j["words"].push_back(sortedWords[i]);
  }

  ofstream ofs(path);
  if (!ofs) {
    return;
  }

  ofs << j.dump(2) << '\n';
}

/////////////////////
// Player helpers  //
/////////////////////

int FindPlayerIndexByUsername(const vector<Player>& players,
                              const string& username) {
  for (int i = 0; i < (int)players.size(); i++) {
    if (players[i].username == username) {
      return i;
    }
  }
  return -1;
}

bool UsernameTaken(const vector<Player>& players, const string& username) {
  if (username == "admin") {
    return true;
  }

  for (int i = 0; i < (int)players.size(); i++) {
    if (players[i].username == username) {
      return true;
    }
  }

  return false;
}

bool CheckNormalLogin(const vector<Player>& players,
                      const string& username,
                      const string& password,
                      Player& resultPlayer) {
  for (int i = 0; i < (int)players.size(); i++) {
    if (players[i].username == username && players[i].password == password) {
      resultPlayer = players[i];
      resultPlayer.isGuest = false;
      resultPlayer.isAdmin = false;
      return true;
    }
  }

  return false;
}

void SaveCurrentPlayer(Player& currentPlayer, vector<Player>& players) {
  if (currentPlayer.isGuest) {
    return;
  }

  if (currentPlayer.isAdmin) {
    return;
  }

  int index = FindPlayerIndexByUsername(players, currentPlayer.username);

  if (index == -1) {
    players.push_back(currentPlayer);
  } else {
    players[index] = currentPlayer;
  }

  SavePlayers(players);
}

/////////////////////
// Game primitives //
/////////////////////

struct Row {
  array<Tile, WORD_LEN> tiles;

  Elements RenderTiles() const {
    Elements elems;

    for (int i = 0; i < WORD_LEN; i++) {
      string s(1, tiles[i].ch);
      if (tiles[i].ch == ' ') {
        s = " ";
      }

      Color bg = Color::GrayDark;

      if (tiles[i].state == State::CORRECT) {
        bg = Color::Green;
      } else if (tiles[i].state == State::MISPLACE) {
        bg = Color::Gold1;
      } else if (tiles[i].state == State::INCORRECT) {
        bg = Color::Red;
      }

      elems.push_back(text(s) | bold | center | size(WIDTH, EQUAL, 7) |
                      size(HEIGHT, EQUAL, 3) | bgcolor(bg) |
                      color(Color::White) | borderEmpty);
    }

    return elems;
  }
};

mt19937& GlobalRng() {
  static mt19937 rng(
      (unsigned)chrono::steady_clock::now().time_since_epoch().count());
  return rng;
}

string PickRandomWord(const vector<string>& words) {
  if (words.empty()) {
    return "";
  }

  uniform_int_distribution<int> dist(0, (int)words.size() - 1);
  int index = dist(GlobalRng());
  return words[index];
}

string PickDailyWord(const vector<string>& words) {
  if (words.empty()) {
    return "";
  }

  auto now = chrono::system_clock::now();
  time_t t = chrono::system_clock::to_time_t(now);

  tm local_tm{};
#ifdef _WIN32
  localtime_s(&local_tm, &t);
#else
  local_tm = *std::localtime(&t);
#endif

  int seed = 0;
  seed = seed + (local_tm.tm_year + 1900) * 10000;
  seed = seed + (local_tm.tm_mon + 1) * 100;
  seed = seed + local_tm.tm_mday;

  int index = seed % (int)words.size();
  return words[index];
}

void EvaluateGuess(Row& row, string target) {
  string guess = "";

  for (int i = 0; i < WORD_LEN; i++) {
    guess += row.tiles[i].ch;
  }

  for (int i = 0; i < WORD_LEN; i++) {
    if (guess[i] == target[i]) {
      row.tiles[i].state = State::CORRECT;
      target[i] = '_';
    }
  }

  for (int i = 0; i < WORD_LEN; i++) {
    if (row.tiles[i].state == State::CORRECT) {
      continue;
    }

    size_t pos = target.find(guess[i]);
    if (pos != string::npos) {
      row.tiles[i].state = State::MISPLACE;
      target[pos] = '_';
    } else {
      row.tiles[i].state = State::INCORRECT;
    }
  }
}

/////////////////////
// Dialog helpers  //
/////////////////////

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

bool LoginDialog(string& username, string& password) {
  auto screen = ScreenInteractive::Fullscreen();

  username = "";
  password = "";
  bool submitted = false;

  auto username_input = Input(&username, "username");
  auto password_input = Input(&password, "password");

  auto login = Button("Log in", [&] {
    submitted = true;
    screen.ExitLoopClosure()();
  });

  auto cancel = Button("Cancel", [&] {
    submitted = false;
    screen.ExitLoopClosure()();
  });

  auto form = Container::Vertical({
      username_input,
      password_input,
      Container::Horizontal({login, cancel}),
  });

  auto renderer = Renderer(form, [&] {
    return vbox({
               text("Login") | bold | center,
               separator(),
               hbox(text("Username: "), username_input->Render()) | center,
               hbox(text("Password: "), password_input->Render()) | center,
               hbox(login->Render(), separator(), cancel->Render()) | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);

  if (!submitted) {
    return false;
  }

  if (username.empty() || password.empty()) {
    return false;
  }

  return true;
}

bool SignupDialog(string& username, string& password) {
  auto screen = ScreenInteractive::Fullscreen();

  username = "";
  password = "";
  bool submitted = false;

  auto username_input = Input(&username, "username");
  auto password_input = Input(&password, "password");

  auto create = Button("Create", [&] {
    submitted = true;
    screen.ExitLoopClosure()();
  });

  auto cancel = Button("Cancel", [&] {
    submitted = false;
    screen.ExitLoopClosure()();
  });

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

  if (!submitted) {
    return false;
  }

  if (username.empty() || password.empty()) {
    return false;
  }

  return true;
}

bool SingleInputDialog(const string& title,
                       const string& label,
                       const string& placeholder,
                       const string& confirmText,
                       string& output) {
  auto screen = ScreenInteractive::Fullscreen();

  output = "";
  bool submitted = false;

  auto input = Input(&output, placeholder);

  auto confirm = Button(confirmText, [&] {
    submitted = true;
    screen.ExitLoopClosure()();
  });

  auto cancel = Button("Cancel", [&] {
    submitted = false;
    screen.ExitLoopClosure()();
  });

  auto form = Container::Vertical({
      input,
      Container::Horizontal({confirm, cancel}),
  });

  auto renderer = Renderer(form, [&] {
    return vbox({
               text(title) | bold | center,
               separator(),
               hbox(text(label), input->Render()) | center,
               hbox(confirm->Render(), separator(), cancel->Render()) | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);

  if (!submitted) {
    return false;
  }

  if (output.empty()) {
    return false;
  }

  return true;
}

// false = Resume, true = Quit to Menu
bool ShowQuitConfirmationDialog() {
  auto screen = ScreenInteractive::Fullscreen();
  bool quit = false;

  auto resume = Button("Resume", [&] {
    quit = false;
    screen.ExitLoopClosure()();
  });

  auto quit_btn = Button("Quit to Menu", [&] {
    quit = true;
    screen.ExitLoopClosure()();
  });

  auto container = Container::Horizontal({resume, quit_btn});

  auto renderer = Renderer(container, [&] {
    return vbox({
               text("Quit current game?") | bold | center,
               separator(),
               hbox(resume->Render(), separator(), quit_btn->Render()) | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);
  return quit;
}

// 0 = Play Again, 1 = Return to Menu
int ShowAfterGameOptions(bool isGuest,
                         bool isAdmin,
                         const string& username,
                         bool won,
                         int attemptsUsed) {
  auto screen = ScreenInteractive::Fullscreen();
  int choice = 1;

  auto play_again = Button("Play Again", [&] {
    choice = 0;
    screen.ExitLoopClosure()();
  });

  auto return_menu = Button("Return to Menu", [&] {
    choice = 1;
    screen.ExitLoopClosure()();
  });

  string sessionLabel = "";
  if (isGuest) {
    sessionLabel = "Guest session finished";
  } else if (isAdmin) {
    sessionLabel = "Admin session finished";
  } else {
    sessionLabel = "Session finished for: " + username;
  }

  string resultLabel = "";
  if (won) {
    resultLabel = "You won in " + to_string(attemptsUsed);
    if (attemptsUsed == 1) {
      resultLabel += " attempt.";
    } else {
      resultLabel += " attempts.";
    }
  } else {
    resultLabel = "Game over.";
  }

  auto container = Container::Horizontal({play_again, return_menu});

  auto renderer = Renderer(container, [&] {
    return vbox({
               text(sessionLabel) | bold | center,
               separator(),
               text(resultLabel) | center,
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
// Leaderboard     //
/////////////////////

void ShowLeaderboardDialog(const vector<Player>& players) {
  auto screen = ScreenInteractive::Fullscreen();

  vector<Player> sorted = players;

  sort(sorted.begin(), sorted.end(), [](const Player& a, const Player& b) {
    if (a.streak != b.streak) {
      return a.streak > b.streak;
    }
    return a.wins > b.wins;
  });

  Elements lines;
  int n = (int)sorted.size();
  if (n > 5) {
    n = 5;
  }

  if (n == 0) {
    lines.push_back(text("No players yet.") | center);
  } else {
    for (int i = 0; i < n; i++) {
      string line = to_string(i + 1) + ". " + sorted[i].username;
      line += "  |  Streak: " + to_string(sorted[i].streak);
      line += "  |  Wins: " + to_string(sorted[i].wins);
      lines.push_back(text(line) | center);
    }
  }

  auto back = Button("Back", screen.ExitLoopClosure());

  auto renderer = Renderer(back, [&] {
    return vbox({
               text("Leaderboard") | bold | center,
               separator(),
               vbox(lines) | center,
               separator(),
               back->Render() | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);
}

/////////////////////
// Word bank admin //
/////////////////////

void ShowWordBankDialog(vector<string>& wordBank) {
  int page = 0;

  while (true) {
    auto screen = ScreenInteractive::Fullscreen();
    int action = 4;  // default back

    int totalPages = 1;
    if (!wordBank.empty()) {
      totalPages = ((int)wordBank.size() + WORDS_PER_PAGE - 1) / WORDS_PER_PAGE;
    }

    if (page < 0) {
      page = 0;
    }
    if (page >= totalPages) {
      page = totalPages - 1;
    }

    int start = page * WORDS_PER_PAGE;
    int end = start + WORDS_PER_PAGE;
    if (end > (int)wordBank.size()) {
      end = (int)wordBank.size();
    }

    Elements lines;

    if (wordBank.empty()) {
      lines.push_back(text("Word bank is empty.") | center);
    } else {
      for (int i = start; i < end; i++) {
        string line = to_string(i + 1) + ". " + wordBank[i];
        lines.push_back(text(line) | center);
      }
    }

    auto prev = Button("Previous", [&] {
      action = 0;
      screen.ExitLoopClosure()();
    });

    auto next = Button("Next", [&] {
      action = 1;
      screen.ExitLoopClosure()();
    });

    auto add = Button("Add Word", [&] {
      action = 2;
      screen.ExitLoopClosure()();
    });

    auto remove = Button("Remove Word", [&] {
      action = 3;
      screen.ExitLoopClosure()();
    });

    auto back = Button("Back", [&] {
      action = 4;
      screen.ExitLoopClosure()();
    });

    auto container =
        Container::Vertical({Container::Horizontal({prev, next}),
                             Container::Horizontal({add, remove, back})});

    auto renderer = Renderer(container, [&] {
      return vbox({
                 text("Word Bank") | bold | center,
                 separator(),
                 text("Page " + to_string(page + 1) + " / " +
                      to_string(totalPages)) |
                     center,
                 separator(),
                 vbox(lines) | center,
                 separator(),
                 hbox(prev->Render(), separator(), next->Render()) | center,
                 hbox(add->Render(), separator(), remove->Render(),
                      separator(), back->Render()) |
                     center,
             }) |
             center | border;
    });

    screen.Loop(renderer);

    if (action == 0) {
      if (page > 0) {
        page--;
      }
    } else if (action == 1) {
      if (page + 1 < totalPages) {
        page++;
      }
    } else if (action == 2) {
      string inputWord = "";
      bool gotInput =
          SingleInputDialog("Add Word", "Word: ", "5-letter word", "Add",
                            inputWord);

      if (!gotInput) {
        continue;
      }

      inputWord = ToUpperWord(inputWord);

      if (!IsValidWord(inputWord)) {
        ShowMessageDialog("Word must be exactly 5 letters.");
        continue;
      }

      bool exists = false;
      for (int i = 0; i < (int)wordBank.size(); i++) {
        if (wordBank[i] == inputWord) {
          exists = true;
          break;
        }
      }

      if (exists) {
        ShowMessageDialog("That word already exists.");
        continue;
      }

      wordBank.push_back(inputWord);
      sort(wordBank.begin(), wordBank.end());
      SaveWordBank(wordBank);
      ShowMessageDialog("Word added.");
    } else if (action == 3) {
      string inputWord = "";
      bool gotInput = SingleInputDialog("Remove Word", "Word: ",
                                        "5-letter word", "Remove", inputWord);

      if (!gotInput) {
        continue;
      }

      inputWord = ToUpperWord(inputWord);

      int index = -1;
      for (int i = 0; i < (int)wordBank.size(); i++) {
        if (wordBank[i] == inputWord) {
          index = i;
          break;
        }
      }

      if (index == -1) {
        ShowMessageDialog("Word not found.");
        continue;
      }

      wordBank.erase(wordBank.begin() + index);
      SaveWordBank(wordBank);
      ShowMessageDialog("Word removed.");
    } else {
      return;
    }
  }
}

/////////////////////
// Delete players  //
/////////////////////

void ShowDeletePlayersDialog(vector<Player>& players) {
  int page = 0;

  while (true) {
    vector<Player> normalPlayers;

    for (int i = 0; i < (int)players.size(); i++) {
      if (!players[i].isGuest && !players[i].isAdmin) {
        normalPlayers.push_back(players[i]);
      }
    }

    sort(normalPlayers.begin(), normalPlayers.end(),
         [](const Player& a, const Player& b) {
           return a.username < b.username;
         });

    auto screen = ScreenInteractive::Fullscreen();
    int action = 3;  // back

    int totalPages = 1;
    if (!normalPlayers.empty()) {
      totalPages =
          ((int)normalPlayers.size() + WORDS_PER_PAGE - 1) / WORDS_PER_PAGE;
    }

    if (page < 0) {
      page = 0;
    }
    if (page >= totalPages) {
      page = totalPages - 1;
    }

    int start = page * WORDS_PER_PAGE;
    int end = start + WORDS_PER_PAGE;
    if (end > (int)normalPlayers.size()) {
      end = (int)normalPlayers.size();
    }

    Elements lines;

    if (normalPlayers.empty()) {
      lines.push_back(text("No players to delete.") | center);
    } else {
      for (int i = start; i < end; i++) {
        string line = to_string(i + 1) + ". " + normalPlayers[i].username;
        line += "  |  Wins: " + to_string(normalPlayers[i].wins);
        line += "  |  Streak: " + to_string(normalPlayers[i].streak);
        lines.push_back(text(line) | center);
      }
    }

    auto prev = Button("Previous", [&] {
      action = 0;
      screen.ExitLoopClosure()();
    });

    auto next = Button("Next", [&] {
      action = 1;
      screen.ExitLoopClosure()();
    });

    auto del = Button("Delete Player", [&] {
      action = 2;
      screen.ExitLoopClosure()();
    });

    auto back = Button("Back", [&] {
      action = 3;
      screen.ExitLoopClosure()();
    });

    auto container =
        Container::Vertical({Container::Horizontal({prev, next}),
                             Container::Horizontal({del, back})});

    auto renderer = Renderer(container, [&] {
      return vbox({
                 text("Delete Players") | bold | center,
                 separator(),
                 text("Page " + to_string(page + 1) + " / " +
                      to_string(totalPages)) |
                     center,
                 separator(),
                 vbox(lines) | center,
                 separator(),
                 hbox(prev->Render(), separator(), next->Render()) | center,
                 hbox(del->Render(), separator(), back->Render()) | center,
             }) |
             center | border;
    });

    screen.Loop(renderer);

    if (action == 0) {
      if (page > 0) {
        page--;
      }
    } else if (action == 1) {
      if (page + 1 < totalPages) {
        page++;
      }
    } else if (action == 2) {
      string username = "";
      bool gotInput = SingleInputDialog("Delete Player", "Username: ",
                                        "username", "Delete", username);

      if (!gotInput) {
        continue;
      }

      if (username == "admin") {
        ShowMessageDialog("Admin cannot be deleted.");
        continue;
      }

      int index = -1;
      for (int i = 0; i < (int)players.size(); i++) {
        if (!players[i].isGuest && !players[i].isAdmin &&
            players[i].username == username) {
          index = i;
          break;
        }
      }

      if (index == -1) {
        ShowMessageDialog("Player not found.");
        continue;
      }

      players.erase(players.begin() + index);
      SavePlayers(players);
      ShowMessageDialog("Player deleted.");
    } else {
      return;
    }
  }
}

/////////////////////
// FTXUI - Game UI //
/////////////////////

GameResult RunGameFTXUI(const vector<string>& wordBank,
                        Player& player,
                        bool dailyMode) {
  GameResult result;
  result.won = false;
  result.quitToMenu = false;
  result.attemptsUsed = 0;

  if (wordBank.empty()) {
    result.quitToMenu = true;
    return result;
  }

  string target = "";
  if (dailyMode) {
    target = PickDailyWord(wordBank);
  } else {
    target = PickRandomWord(wordBank);
  }

  array<Row, MAX_ROWS> rows;

  for (int r = 0; r < MAX_ROWS; r++) {
    for (int c = 0; c < WORD_LEN; c++) {
      rows[r].tiles[c].ch = ' ';
      rows[r].tiles[c].state = State::BLANK;
    }
  }

  int currentRow = 0;
  int currentCol = 0;
  bool gameOver = false;
  string message = "";

  auto screen = ScreenInteractive::Fullscreen();

  auto renderer = Renderer([&] {
    Elements board;
    for (int r = 0; r < MAX_ROWS; r++) {
      board.push_back(hbox(rows[r].RenderTiles()) | center);
    }

    Elements info;

    string playerLabel = "";
    if (player.isGuest) {
      playerLabel = "Playing as Guest";
    } else if (player.isAdmin) {
      playerLabel = "Admin: " + player.username;
    } else {
      playerLabel = "Player: " + player.username;
    }

    string modeLabel = "";
    if (dailyMode) {
      modeLabel = "Mode: Daily";
    } else {
      modeLabel = "Mode: Normal";
    }

    info.push_back(text(playerLabel) | center);
    info.push_back(text(modeLabel) | center);

    if (player.isAdmin) {
      info.push_back(text("Target word: " + target) | bold | center);
    }

    return vbox({
               vbox(info) | center,
               separator(),
               vbox(board) | center,
               separator(),
               text("Press Esc to open menu.") | center,
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

    if (event == Event::Escape) {
      bool quit = ShowQuitConfirmationDialog();
      if (quit) {
        result.quitToMenu = true;
        screen.ExitLoopClosure()();
      }
      return true;
    }

    if (event.is_character() && currentCol < WORD_LEN) {
      string s = event.character();

      if (!s.empty()) {
        unsigned char uc = (unsigned char)s[0];
        if (isalpha(uc)) {
          rows[currentRow].tiles[currentCol].ch =
              (char)toupper((unsigned char)uc);
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
      string guess = "";
      for (int i = 0; i < WORD_LEN; i++) {
        guess += rows[currentRow].tiles[i].ch;
      }

      EvaluateGuess(rows[currentRow], target);

      if (guess == target) {
        gameOver = true;
        result.won = true;
        result.attemptsUsed = currentRow + 1;

        message = "You won in " + to_string(result.attemptsUsed);
        if (result.attemptsUsed == 1) {
          message += " attempt.";
        } else {
          message += " attempts.";
        }
        message += " Press Enter to continue.";
      } else {
        currentRow++;
        currentCol = 0;

        if (currentRow >= MAX_ROWS) {
          gameOver = true;
          result.won = false;
          result.attemptsUsed = MAX_ROWS;
          message = "The word was: " + target + ". Press Enter to continue.";
        }
      }

      return true;
    }

    return false;
  });

  screen.Loop(component);

  if (!result.quitToMenu) {
    if (!player.isGuest && !player.isAdmin) {
      if (result.won) {
        player.wins++;
        player.streak++;
      } else {
        player.streak = 0;
      }
    }
  }

  return result;
}

/////////////////////
// Session helpers //
/////////////////////

void PlaySessionLoop(Player& currentPlayer,
                     vector<Player>& players,
                     const vector<string>& wordBank,
                     bool dailyMode) {
  while (true) {
    GameResult result = RunGameFTXUI(wordBank, currentPlayer, dailyMode);

    if (result.quitToMenu) {
      return;
    }

    SaveCurrentPlayer(currentPlayer, players);

    int choice = ShowAfterGameOptions(currentPlayer.isGuest,
                                      currentPlayer.isAdmin,
                                      currentPlayer.username,
                                      result.won,
                                      result.attemptsUsed);

    if (choice == 0) {
      continue;
    } else {
      return;
    }
  }
}

/////////////////////
// Main loop       //
/////////////////////

int main() {
  vector<string> wordBank = LoadWordBank();
  vector<Player> players = LoadPlayers();
  Player currentPlayer = MakeGuestPlayer();

  while (true) {
    int menu_choice = -1;
    auto screen = ScreenInteractive::Fullscreen();

    if (currentPlayer.isGuest) {
      auto login_btn = Button("Login", [&] {
        menu_choice = 0;
        screen.ExitLoopClosure()();
      });

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

      auto exit_btn = Button("Exit", [&] {
        menu_choice = 4;
        screen.ExitLoopClosure()();
      });

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
                   text("") | size(HEIGHT, EQUAL, 1),
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

      if (menu_choice == 0) {
        string username = "";
        string password = "";

        bool success = LoginDialog(username, password);
        if (!success) {
          continue;
        }

        if (username == "admin" && password == "admin123") {
          currentPlayer = MakeAdminPlayer();
          ShowMessageDialog("Logged in as admin.");
          continue;
        }

        Player loggedInPlayer;
        bool found = CheckNormalLogin(players, username, password, loggedInPlayer);

        if (found) {
          currentPlayer = loggedInPlayer;
          ShowMessageDialog("Logged in as: " + currentPlayer.username);
        } else {
          ShowMessageDialog("Invalid username or password.");
        }

      } else if (menu_choice == 1) {
        string username = "";
        string password = "";

        bool success = SignupDialog(username, password);
        if (!success) {
          continue;
        }

        if (!IsValidUsername(username)) {
          ShowMessageDialog(
              "Invalid username. Letters, numbers, underscores only.");
          continue;
        }

        if (UsernameTaken(players, username)) {
          ShowMessageDialog("Username already taken.");
          continue;
        }

        Player newPlayer;
        newPlayer.username = username;
        newPlayer.password = password;
        newPlayer.wins = 0;
        newPlayer.streak = 0;
        newPlayer.isGuest = false;
        newPlayer.isAdmin = false;

        players.push_back(newPlayer);
        SavePlayers(players);
        currentPlayer = newPlayer;

        ShowMessageDialog("Account created. Logged in as: " +
                          currentPlayer.username);

      } else if (menu_choice == 2) {
        currentPlayer = MakeGuestPlayer();
        PlaySessionLoop(currentPlayer, players, wordBank, false);

      } else if (menu_choice == 3) {
        players = LoadPlayers();
        ShowLeaderboardDialog(players);

      } else if (menu_choice == 4) {
        return 0;
      }

    } else if (currentPlayer.isAdmin) {
      auto play_btn = Button("Play", [&] {
        menu_choice = 10;
        screen.ExitLoopClosure()();
      });

      auto daily_btn = Button("Play Daily Mode", [&] {
        menu_choice = 11;
        screen.ExitLoopClosure()();
      });

      auto word_bank_btn = Button("Word Bank", [&] {
        menu_choice = 12;
        screen.ExitLoopClosure()();
      });

      auto delete_players_btn = Button("Delete Players", [&] {
        menu_choice = 13;
        screen.ExitLoopClosure()();
      });

      auto leaderboard_btn = Button("Leaderboard", [&] {
        menu_choice = 14;
        screen.ExitLoopClosure()();
      });

      auto logout_btn = Button("Log Out", [&] {
        menu_choice = 15;
        screen.ExitLoopClosure()();
      });

      auto exit_btn = Button("Exit", [&] {
        menu_choice = 16;
        screen.ExitLoopClosure()();
      });

      auto menu = Container::Vertical({
          play_btn,
          daily_btn,
          word_bank_btn,
          delete_players_btn,
          leaderboard_btn,
          logout_btn,
          exit_btn,
      });

      auto renderer = Renderer(menu, [&] {
        return vbox({
                   text("=== WORDLE ===") | bold | center,
                   separator(),
                   text("Logged in as: admin") | bold | center,
                   text("") | size(HEIGHT, EQUAL, 1),
                   vbox({
                       play_btn->Render(),
                       daily_btn->Render(),
                       word_bank_btn->Render(),
                       delete_players_btn->Render(),
                       leaderboard_btn->Render(),
                       logout_btn->Render(),
                       exit_btn->Render(),
                   }) |
                       center,
               }) |
               center;
      });

      screen.Loop(renderer);

      if (menu_choice == 10) {
        PlaySessionLoop(currentPlayer, players, wordBank, false);
      } else if (menu_choice == 11) {
        PlaySessionLoop(currentPlayer, players, wordBank, true);
      } else if (menu_choice == 12) {
        wordBank = LoadWordBank();
        ShowWordBankDialog(wordBank);
        wordBank = LoadWordBank();
      } else if (menu_choice == 13) {
        players = LoadPlayers();
        ShowDeletePlayersDialog(players);
        players = LoadPlayers();
      } else if (menu_choice == 14) {
        players = LoadPlayers();
        ShowLeaderboardDialog(players);
      } else if (menu_choice == 15) {
        currentPlayer = MakeGuestPlayer();
        ShowMessageDialog("Logged out.");
      } else if (menu_choice == 16) {
        return 0;
      }

    } else {
      auto play_btn = Button("Play", [&] {
        menu_choice = 20;
        screen.ExitLoopClosure()();
      });

      auto daily_btn = Button("Play Daily Mode", [&] {
        menu_choice = 21;
        screen.ExitLoopClosure()();
      });

      auto leaderboard_btn = Button("Leaderboard", [&] {
        menu_choice = 22;
        screen.ExitLoopClosure()();
      });

      auto logout_btn = Button("Log Out", [&] {
        menu_choice = 23;
        screen.ExitLoopClosure()();
      });

      auto exit_btn = Button("Exit", [&] {
        menu_choice = 24;
        screen.ExitLoopClosure()();
      });

      auto menu = Container::Vertical({
          play_btn,
          daily_btn,
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
                   text("") | size(HEIGHT, EQUAL, 1),
                   vbox({
                       play_btn->Render(),
                       daily_btn->Render(),
                       leaderboard_btn->Render(),
                       logout_btn->Render(),
                       exit_btn->Render(),
                   }) |
                       center,
               }) |
               center;
      });

      screen.Loop(renderer);

      if (menu_choice == 20) {
        PlaySessionLoop(currentPlayer, players, wordBank, false);
      } else if (menu_choice == 21) {
        PlaySessionLoop(currentPlayer, players, wordBank, true);
      } else if (menu_choice == 22) {
        players = LoadPlayers();
        ShowLeaderboardDialog(players);
      } else if (menu_choice == 23) {
        currentPlayer = MakeGuestPlayer();
        ShowMessageDialog("Logged out.");
      } else if (menu_choice == 24) {
        return 0;
      }
    }
  }

  return 0;
}