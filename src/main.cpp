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

static const int project_root_search_depth = 8;
static const int json_indent_spaces = 2;
static const int default_stat_value = 0;
static const int max_username_length = 10;
static const int title_spacer_height = 1;
static const int screen_section_spacer_height = 1;
static const int max_rows = 6;
static const int leaderboard_entries_to_show = 5;
static const int words_per_page = 15;
static const int button_width = 40;
static const int rules_box_width = 40;
static const int rules_box_gap = 20;
static const int game_box_width_3_letter = 32;
static const int game_box_width_5_letter = 50;
static const int game_box_width_6_letter = 60;
static const int dialog_min_width = 30;
static const int dialog_outer_padding_height = 0;
static const int dialog_inner_padding_height = 1;
static const int dialog_button_gap_width = 3;
static const int rules_spacer_height = 1;
static const int tile_width = 7;
static const int tile_height = 3;

static const string guest_username = "Guest";
static const string admin_username = "admin";
static const string admin_password = "admin123";

static const string button_back = "Back";
static const string button_cancel = "Cancel";
static const string button_continue = "Continue";
static const string button_previous = "Previous";
static const string button_next = "Next";
static const string button_play_again = "Play Again";
static const string button_return_to_menu = "Return to Menu";

Element VerticalSpacer(int height) {
  return text("") | size(HEIGHT, EQUAL, height);
}

Element HorizontalSpacer(int width) {
  return text("") | size(WIDTH, EQUAL, width);
}

int GetGameBoxWidth(int wordLength) {
  if (wordLength == 3) {
    return game_box_width_3_letter;
  }

  if (wordLength == 6) {
    return game_box_width_6_letter;
  }

  return game_box_width_5_letter;
}

Element ColoredSubtitleLine(const string& left,
                            const string& middle,
                            const string& right) {
  return hbox({
             text(left) | color(Color::Green),
             text(middle) | color(Color::Yellow),
             text(right) | color(Color::Red),
         }) | center;
}

Element RenderWordleTitle() {
  return vbox({
             text(" █████   ███   █████                                 █████    ████                                           ") | center,
             text("▒▒███   ▒███  ▒▒███                                 ▒▒███    ▒▒███                     ███            ███    ") | center,
             text(" ▒███   ▒███   ▒███      ██████     ████████      ███████     ▒███      ██████        ▒███           ▒███    ") | center,
             text(" ▒███   ▒███   ▒███     ███▒▒███   ▒▒███▒▒███    ███▒▒███     ▒███     ███▒▒███    ███████████    ███████████") | center,
             text(" ▒▒███  █████  ███     ▒███ ▒███    ▒███ ▒▒▒    ▒███ ▒███     ▒███    ▒███████    ▒▒▒▒▒███▒▒▒    ▒▒▒▒▒███▒▒▒ ") | center,
             text("  ▒▒▒█████▒█████▒      ▒███ ▒███    ▒███        ▒███ ▒███     ▒███    ▒███▒▒▒         ▒███           ▒███    ") | center,
             text("    ▒▒███ ▒▒███        ▒▒██████     █████       ▒▒████████    █████   ▒▒██████        ▒▒▒            ▒▒▒     ") | center,
             text("     ▒▒▒   ▒▒▒          ▒▒▒▒▒▒     ▒▒▒▒▒         ▒▒▒▒▒▒▒▒    ▒▒▒▒▒     ▒▒▒▒▒▒                                ") | center,
             VerticalSpacer(title_spacer_height),
             VerticalSpacer(title_spacer_height),

             ColoredSubtitleLine(
                 "██     ██  ▄▄▄  ▄▄▄▄  ▄▄▄▄     ",
                 "▄████  ▄▄ ▄▄ ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄ ▄▄ ▄▄  ▄▄  ▄▄▄▄    ",
                 "▄████   ▄▄▄  ▄▄   ▄▄ ▄▄▄▄▄"),

             ColoredSubtitleLine(
                 " ██ ▄█▄ ██ ██▀██ ██▄█▄ ██▀██   ",
                 "██  ▄▄▄ ██ ██ ██▄▄  ███▄▄ ███▄▄ ██ ███▄██ ██ ▄▄   ",
                 "██  ▄▄▄ ██▀██ ██▀▄▀██ ██▄▄  "),

             ColoredSubtitleLine(
                 "  ▀██▀██▀  ▀███▀ ██ ██ ████▀   ",
                 " ▀███▀  ▀███▀ ██▄▄▄ ▄▄██▀ ▄▄██▀ ██ ██ ▀██ ▀███▀   ",
                 " ▀███▀  ██▀██ ██   ██ ██▄▄▄ "),

             VerticalSpacer(title_spacer_height),
         });
}

// Path helper methods

fs::path FindProjectRoot() {
  fs::path current = fs::current_path();

  for (int i = 0; i < project_root_search_depth; i++) {
    if (fs::exists(current / "CMakeLists.txt")) {
      return current;
    }

    if (fs::exists(current / "data" / "wordbank_3.json") ||
        fs::exists(current / "data" / "wordbank_5.json") ||
        fs::exists(current / "data" / "wordbank_6.json") ||
        fs::exists(current / "data" / "wordbank.json")) {
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

fs::path LegacyWordBankFilePath() {
  fs::path root = FindProjectRoot();
  return root / "data" / "wordbank.json";
}

fs::path WordBankFilePath(int wordLength) {
  fs::path root = FindProjectRoot();
  return root / "data" / ("wordbank_" + to_string(wordLength) + ".json");
}

string WordPlaceholder(int wordLength) {
  return to_string(wordLength) + "-letter word";
}

string WordRequirementMessage(int wordLength) {
  return "Word must be exactly " + to_string(wordLength) + " letters.";
}

string DailyWordBankEmptyMessage() {
  return "The " + to_string(5) +
         "-letter word bank is empty.";
}

// Utility helpers

Player MakeGuestPlayer() {
  Player p;
  p.username = guest_username;
  p.password = "";
  p.wins = default_stat_value;
  p.streak = default_stat_value;
  p.isGuest = true;
  p.isAdmin = false;
  return p;
}

Player MakeAdminPlayer() {
  Player p;
  p.username = admin_username;
  p.password = admin_password;
  p.wins = default_stat_value;
  p.streak = default_stat_value;
  p.isGuest = false;
  p.isAdmin = true;
  return p;
}

bool IsValidUsername(const string& username) {
  if (username.empty()) {
    return false;
  }

  if (username.size() > max_username_length) {
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

bool IsValidWord(const string& word, int wordLength) {
  if ((int)word.size() != wordLength) {
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

// JSON IO helpers

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

  ofs << j.dump(json_indent_spaces) << '\n';
}

vector<string> LoadWordBankFromPath(const fs::path& path, int wordLength) {
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

    if (IsValidWord(word, wordLength)) {
      words.push_back(word);
    }
  }

  sort(words.begin(), words.end());
  words.erase(unique(words.begin(), words.end()), words.end());

  return words;
}

vector<string> LoadWordBank(int wordLength) {
  fs::path path = WordBankFilePath(wordLength);

  if (wordLength == 5 && !fs::exists(path)) {
    return LoadWordBankFromPath(LegacyWordBankFilePath(), wordLength);
  }

  return LoadWordBankFromPath(path, wordLength);
}

void SaveWordBank(const vector<string>& words, int wordLength) {
  vector<string> sortedWords = words;
  sort(sortedWords.begin(), sortedWords.end());
  sortedWords.erase(unique(sortedWords.begin(), sortedWords.end()),
                    sortedWords.end());

  json j;
  j["words"] = json::array();

  for (int i = 0; i < (int)sortedWords.size(); i++) {
    j["words"].push_back(sortedWords[i]);
  }

  ofstream ofs(WordBankFilePath(wordLength));
  if (!ofs) {
    return;
  }

  ofs << j.dump(json_indent_spaces) << '\n';
}

// Player helpers 

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
  if (username == admin_username) {
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

// Game primitives

struct Row {
  vector<Tile> tiles;

  Row() = default;

  explicit Row(int wordLength) : tiles(wordLength) {}

  Elements RenderTiles() const {
    Elements elems;

    for (int i = 0; i < (int)tiles.size(); i++) {
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

      elems.push_back(text(s) | bold | center | size(WIDTH, EQUAL, tile_width) |
                      size(HEIGHT, EQUAL, tile_height) | bgcolor(bg) |
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

void EvaluateGuess(Row& row, string target, int wordLength) {
  string guess = "";

  for (int i = 0; i < wordLength; i++) {
    guess += row.tiles[i].ch;
  }

  for (int i = 0; i < wordLength; i++) {
    if (guess[i] == target[i]) {
      row.tiles[i].state = State::CORRECT;
      target[i] = '_';
    }
  }

  for (int i = 0; i < wordLength; i++) {
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

// Dialog helpers 

void ConfirmationDialog(const string& message) {
  auto screen = ScreenInteractive::Fullscreen();
  auto okay = Button(button_continue, screen.ExitLoopClosure());

  auto renderer = Renderer(okay, [&] {
    Element dialogBox = vbox({
                            VerticalSpacer(dialog_outer_padding_height),
                            text(message) | center,
                            separator(),
                            okay->Render() | center,
                            VerticalSpacer(dialog_outer_padding_height),
                        }) |
                        border | size(WIDTH, GREATER_THAN, dialog_min_width);

    return dialogBox | center;
  });

  screen.Loop(renderer);
}

bool LoginDialog(string& username, string& password) {
  auto screen = ScreenInteractive::Fullscreen();

  username = "";
  password = "";
  bool submitted = false;

  auto username_input = Input(&username, "username");
  InputOption password_option;
  password_option.content = &password;
  password_option.placeholder = "password";
  password_option.password = true;
  password_option.multiline = false;
  auto password_input = Input(password_option);

  auto login = Button("Log in", [&] {
    submitted = true;
    screen.ExitLoopClosure()();
  });

  auto cancel = Button(button_cancel, [&] {
    submitted = false;
    screen.ExitLoopClosure()();
  });

  auto form = Container::Vertical({
      username_input,
      password_input,
      Container::Horizontal({login, cancel}),
  });

  auto renderer = Renderer(form, [&] {
    Element formBox = vbox({
                          VerticalSpacer(dialog_outer_padding_height),
                          text("Log In") | bold | center,
                          separator(),
                          VerticalSpacer(dialog_inner_padding_height),
                          hbox(text("Username: ") | bold,
                               username_input->Render()) |
                              center,
                          VerticalSpacer(dialog_inner_padding_height),
                          hbox(text("Password: ") | bold,
                               password_input->Render()) |
                              center,
                          VerticalSpacer(dialog_inner_padding_height),
                          separator(),
                          hbox(login->Render(),
                               HorizontalSpacer(dialog_button_gap_width),
                               cancel->Render()) |
                              center,
                          VerticalSpacer(dialog_outer_padding_height),
                      }) |
                      border | size(WIDTH, GREATER_THAN, dialog_min_width);

    return formBox | center;
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
  InputOption password_option;
  password_option.content = &password;
  password_option.placeholder = "password";
  password_option.password = true;
  password_option.multiline = false;
  auto password_input = Input(password_option);

  auto create = Button("Create", [&] {
    submitted = true;
    screen.ExitLoopClosure()();
  });

  auto cancel = Button(button_cancel, [&] {
    submitted = false;
    screen.ExitLoopClosure()();
  });

  auto form = Container::Vertical({
      username_input,
      password_input,
      Container::Horizontal({create, cancel}),
  });

  auto renderer = Renderer(form, [&] {
    Element formBox = vbox({
                          VerticalSpacer(dialog_outer_padding_height),
                          text("Sign Up") | bold | center,
                          separator(),
                          VerticalSpacer(dialog_inner_padding_height),
                          hbox(text("Username: ") | bold,
                               username_input->Render()) |
                              center,
                          VerticalSpacer(dialog_inner_padding_height),
                          hbox(text("Password: ") | bold,
                               password_input->Render()) |
                              center,
                          VerticalSpacer(dialog_inner_padding_height),
                          separator(),
                          hbox(create->Render(),
                               HorizontalSpacer(dialog_button_gap_width),
                               cancel->Render()) |
                              center,
                          VerticalSpacer(dialog_outer_padding_height),
                      }) |
                      border | size(WIDTH, GREATER_THAN, dialog_min_width);

    return formBox | center;
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

  auto cancel = Button(button_cancel, [&] {
    submitted = false;
    screen.ExitLoopClosure()();
  });

  auto form = Container::Vertical({
      input,
      Container::Horizontal({confirm, cancel}),
  });

  auto renderer = Renderer(form, [&] {
    Element dialogBox = vbox({
                            VerticalSpacer(dialog_outer_padding_height),
                            text(title) | bold | center,
                            separator(),
                            VerticalSpacer(dialog_inner_padding_height),
                            hbox(text(label), input->Render()) | center,
                            VerticalSpacer(dialog_inner_padding_height),
                            separator(),
                            hbox(confirm->Render(),
                                 HorizontalSpacer(dialog_button_gap_width),
                                 cancel->Render()) |
                                center,
                            VerticalSpacer(dialog_outer_padding_height),
                        }) |
                        border | size(WIDTH, GREATER_THAN, dialog_min_width);

    return dialogBox | center;
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

bool ShowPauseDialog() {
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
    Element dialogBox = vbox({
                            VerticalSpacer(dialog_outer_padding_height),
                            text("Game Paused.") | bold | center,
                            separator(),
                            hbox(resume->Render(),
                                 HorizontalSpacer(dialog_button_gap_width),
                                 quit_btn->Render()) |
                                center,
                            VerticalSpacer(dialog_outer_padding_height),
                        }) |
                        border | size(WIDTH, GREATER_THAN, dialog_min_width);

    return dialogBox | center;
  });

  screen.Loop(renderer);
  return quit;
}

int ShowAfterGameOptions(bool isGuest,
                         bool isAdmin,
                         const string& username,
                         bool won,
                         int attemptsUsed) {
  auto screen = ScreenInteractive::Fullscreen();
  int choice = 1;

  auto play_again = Button(button_play_again, [&] {
    choice = 0;
    screen.ExitLoopClosure()();
  });

  auto return_menu = Button(button_return_to_menu, [&] {
    choice = 1;
    screen.ExitLoopClosure()();
  });

  string sessionLabel = "";
  if (isGuest) {
    sessionLabel = "Game finished for Guest";
  } else if (isAdmin) {
    sessionLabel = "Game finished for Admin";
  } else {
    sessionLabel = "Game finished for: " + username;
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
    Element dialogBox = vbox({
                            VerticalSpacer(dialog_outer_padding_height),
                            text(sessionLabel) | bold | center,
                            separator(),
                            VerticalSpacer(dialog_inner_padding_height),
                            text(resultLabel) | center,
                            VerticalSpacer(dialog_inner_padding_height),
                            separator(),
                            hbox(play_again->Render(),
                                 HorizontalSpacer(dialog_button_gap_width),
                                 return_menu->Render()) |
                                center,
                            VerticalSpacer(dialog_outer_padding_height),
                        }) |
                        border | size(WIDTH, GREATER_THAN, dialog_min_width);

    return dialogBox | center;
  });

  screen.Loop(renderer);
  return choice;
}

int ShowPlayModeDialog() {
  auto screen = ScreenInteractive::Fullscreen();
  int selectedLength = 0;

  auto three_btn = Button("3-Letter Mode", [&] {
    selectedLength = 3;
    screen.ExitLoopClosure()();
  });

  auto five_btn = Button("5-Letter Mode", [&] {
    selectedLength = 5;
    screen.ExitLoopClosure()();
  });

  auto six_btn = Button("6-Letter Mode", [&] {
    selectedLength = 6;
    screen.ExitLoopClosure()();
  });

  auto back_btn = Button(button_back, [&] {
    selectedLength = 0;
    screen.ExitLoopClosure()();
  });

  auto menu = Container::Vertical({
      three_btn,
      five_btn,
      six_btn,
      back_btn,
  });

  auto renderer = Renderer(menu, [&] {
    return vbox({
               text("Select Play Mode") | bold | center,
               separator(),
               three_btn->Render() | size(WIDTH, EQUAL, button_width) | center,
               five_btn->Render() | size(WIDTH, EQUAL, button_width) | center,
               six_btn->Render() | size(WIDTH, EQUAL, button_width) | center,
               back_btn->Render() | size(WIDTH, EQUAL, button_width) | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);
  return selectedLength;
}

int ShowWordBankModeDialog() {
  auto screen = ScreenInteractive::Fullscreen();
  int selectedLength = 0;

  auto three_btn = Button("3-Letter Bank", [&] {
    selectedLength = 3;
    screen.ExitLoopClosure()();
  });

  auto five_btn = Button("5-Letter Bank", [&] {
    selectedLength = 5;
    screen.ExitLoopClosure()();
  });

  auto six_btn = Button("6-Letter Bank", [&] {
    selectedLength = 6;
    screen.ExitLoopClosure()();
  });

  auto back_btn = Button(button_back, [&] {
    selectedLength = 0;
    screen.ExitLoopClosure()();
  });

  auto menu = Container::Vertical({
      three_btn,
      five_btn,
      six_btn,
      back_btn,
  });

  auto renderer = Renderer(menu, [&] {
    return vbox({
               text("Select Word Bank") | bold | center,
               separator(),
               three_btn->Render() | size(WIDTH, EQUAL, button_width) | center,
               five_btn->Render() | size(WIDTH, EQUAL, button_width) | center,
               six_btn->Render() | size(WIDTH, EQUAL, button_width) | center,
               back_btn->Render() | size(WIDTH, EQUAL, button_width) | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);
  return selectedLength;
}

// Leaderboard

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
  if (n > leaderboard_entries_to_show) {
    n = leaderboard_entries_to_show;
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

  auto back = Button(button_back, screen.ExitLoopClosure());

  auto renderer = Renderer(back, [&] {
    return vbox({
               text("██     ▄▄▄▄▄  ▄▄▄  ▄▄▄▄  ▄▄▄▄▄ ▄▄▄▄  ▄▄▄▄   ▄▄▄   ▄▄▄  ▄▄▄▄  ▄▄▄▄  ") | center,
               text("██     ██▄▄  ██▀██ ██▀██ ██▄▄  ██▄█▄ ██▄██ ██▀██ ██▀██ ██▄█▄ ██▀██ ") | center,
               text("██████ ██▄▄▄ ██▀██ ████▀ ██▄▄▄ ██ ██ ██▄█▀ ▀███▀ ██▀██ ██ ██ ████▀ ") | center,
               VerticalSpacer(screen_section_spacer_height),
               separator(),
               vbox(lines) | center,
               separator(),
               back->Render() | center,
           }) |
           center | border;
  });

  screen.Loop(renderer);
}

// Word bank admin

void ShowWordBankDialog(vector<string>& wordBank, int wordLength) {
  int page = 0;

  while (true) {
    auto screen = ScreenInteractive::Fullscreen();
    int action = 4;  

    int totalPages = 1;
    if (!wordBank.empty()) {
      totalPages = ((int)wordBank.size() + words_per_page - 1) / words_per_page;
    }

    if (page < 0) {
      page = 0;
    }
    if (page >= totalPages) {
      page = totalPages - 1;
    }

    int start = page * words_per_page;
    int end = start + words_per_page;
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

    auto prev = Button(button_previous, [&] {
      action = 0;
      screen.ExitLoopClosure()();
    });

    auto next = Button(button_next, [&] {
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

    auto back = Button(button_back, [&] {
      action = 4;
      screen.ExitLoopClosure()();
    });

    auto container =
        Container::Vertical({Container::Horizontal({prev, next}),
                             Container::Horizontal({add, remove, back})});

    auto renderer = Renderer(container, [&] {
      return vbox({
                 text("██     ██  ▄▄▄  ▄▄▄▄  ▄▄▄▄    █████▄  ▄▄▄  ▄▄  ▄▄ ▄▄ ▄▄ ") | center,
                 text("██ ▄█▄ ██ ██▀██ ██▄█▄ ██▀██   ██▄▄██ ██▀██ ███▄██ ██▄█▀ ") | center,
                 text(" ▀██▀██▀  ▀███▀ ██ ██ ████▀   ██▄▄█▀ ██▀██ ██ ▀██ ██ ██ ") | center,
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
      bool gotInput = SingleInputDialog("Add Word", "Word: ",
                                        WordPlaceholder(wordLength), "Add",
                                        inputWord);

      if (!gotInput) {
        continue;
      }

      inputWord = ToUpperWord(inputWord);

      if (!IsValidWord(inputWord, wordLength)) {
        ConfirmationDialog(WordRequirementMessage(wordLength));
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
        ConfirmationDialog("That word already exists.");
        continue;
      }

      wordBank.push_back(inputWord);
      sort(wordBank.begin(), wordBank.end());
      SaveWordBank(wordBank, wordLength);
      ConfirmationDialog("Word added.");
    } else if (action == 3) {
      string inputWord = "";
      bool gotInput = SingleInputDialog("Remove Word", "Word: ",
                                        WordPlaceholder(wordLength),
                                        "Remove", inputWord);

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
        ConfirmationDialog("Word not found.");
        continue;
      }

      wordBank.erase(wordBank.begin() + index);
      SaveWordBank(wordBank, wordLength);
      ConfirmationDialog("Word removed.");
    } else {
      return;
    }
  }
}

// Delete players 

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
    int action = 3; 

    int totalPages = 1;
    if (!normalPlayers.empty()) {
      totalPages =
          ((int)normalPlayers.size() + words_per_page - 1) / words_per_page;
    }

    if (page < 0) {
      page = 0;
    }
    if (page >= totalPages) {
      page = totalPages - 1;
    }

    int start = page * words_per_page;
    int end = start + words_per_page;
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

    auto prev = Button(button_previous, [&] {
      action = 0;
      screen.ExitLoopClosure()();
    });

    auto next = Button(button_next, [&] {
      action = 1;
      screen.ExitLoopClosure()();
    });

    auto del = Button("Delete Player", [&] {
      action = 2;
      screen.ExitLoopClosure()();
    });

    auto back = Button(button_back, [&] {
      action = 3;
      screen.ExitLoopClosure()();
    });

    auto container =
        Container::Vertical({Container::Horizontal({prev, next}),
                             Container::Horizontal({del, back})});

    auto renderer = Renderer(container, [&] {
      return vbox({
                 text("████▄  ▄▄▄▄▄ ▄▄    ▄▄▄▄▄ ▄▄▄▄▄▄ ▄▄▄▄▄   █████▄ ▄▄     ▄▄▄  ▄▄ ▄▄ ▄▄▄▄▄ ▄▄▄▄ ") | center,
                 text("██  ██ ██▄▄  ██    ██▄▄    ██   ██▄▄    ██▄▄█▀ ██    ██▀██ ▀███▀ ██▄▄  ██▄█▄") | center,
                 text("████▀  ██▄▄▄ ██▄▄▄ ██▄▄▄   ██   ██▄▄▄   ██     ██▄▄▄ ██▀██   █   ██▄▄▄ ██ ██ ") | center,
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

      if (username == admin_username) {
        ConfirmationDialog("Admin cannot be deleted.");
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
        ConfirmationDialog("Player not found.");
        continue;
      }

      players.erase(players.begin() + index);
      SavePlayers(players);
      ConfirmationDialog("Player deleted.");
    } else {
      return;
    }
  }
}

// FTXUI - Game UI

GameResult RunGameFTXUI(const vector<string>& wordBank,
                        Player& player,
                        bool dailyMode,
                        int wordLength) {
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

  array<Row, max_rows> rows;

  for (int r = 0; r < max_rows; r++) {
    rows[r] = Row(wordLength);
    for (int c = 0; c < wordLength; c++) {
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
    int gameBoxWidth = GetGameBoxWidth(wordLength);
    Elements board;
    for (int r = 0; r < max_rows; r++) {
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

    info.push_back(paragraphAlignCenter(playerLabel) | center);
    info.push_back(paragraphAlignCenter(modeLabel) | center);

    if (player.isAdmin) {
      info.push_back(paragraphAlignCenter("Target word: " + target) | bold |
                     center);
    }

    Element rulesBox = vbox({
                             text("RULES") | bold | center,
                             separator(),
                             text("Guess the " + to_string(wordLength) +
                                  "-letter word!") |
                                 center,
                             text("You have 6 tries!") | center,
                             VerticalSpacer(rules_spacer_height),
                             text("Green  = right letter, right spot") | center,
                             text("Yellow = right letter, wrong spot") | center,
                             text("Red    = letter not in word") | center,
                             VerticalSpacer(rules_spacer_height),
                             text("ESC = pause") | center,
                         }) |
                         border;

    Element boardBox = vbox({
                             vbox(info) | center,
                             separator(),
                             vbox(board) | center,
                             separator(),
                             paragraphAlignCenter(message) | center,
                         }) |
                         border | size(WIDTH, EQUAL, gameBoxWidth);

    Element rulesColumn = vbox({
                             rulesBox | size(WIDTH, EQUAL, rules_box_width),
                             filler(),
                         });

    return hbox({
               HorizontalSpacer(rules_box_width),
               boardBox | flex,
               HorizontalSpacer(rules_box_gap),
               rulesColumn,
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
      bool quit = ShowPauseDialog();
      if (quit) {
        result.quitToMenu = true;
        screen.ExitLoopClosure()();
      }
      return true;
    }

    if (event.is_character() && currentCol < wordLength) {
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

    if (event == Event::Return && currentCol == wordLength) {
      string guess = "";
      for (int i = 0; i < wordLength; i++) {
        guess += rows[currentRow].tiles[i].ch;
      }

      EvaluateGuess(rows[currentRow], target, wordLength);

      if (guess == target) {
        gameOver = true;
        result.won = true;
        result.attemptsUsed = currentRow + 1;
        message = "Press Enter to continue.";
      } else {
        currentRow++;
        currentCol = 0;

        if (currentRow >= max_rows) {
          gameOver = true;
          result.won = false;
          result.attemptsUsed = max_rows;
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

// Session helpers 

void PlaySessionLoop(Player& currentPlayer,
                     vector<Player>& players,
                     const vector<string>& wordBank,
                     bool dailyMode,
                     int wordLength) {
  while (true) {
    GameResult result =
        RunGameFTXUI(wordBank, currentPlayer, dailyMode, wordLength);

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

// Main loop

int main() {
  vector<Player> players = LoadPlayers();
  Player currentPlayer = MakeGuestPlayer();

  while (true) {
    int menu_choice = -1;
    auto screen = ScreenInteractive::Fullscreen();

    if (currentPlayer.isGuest) {
      auto login_btn = Button("🔑 Log in", [&] {
        menu_choice = 0;
        screen.ExitLoopClosure()();
      });

      auto signup_btn = Button("📝 Sign up", [&] {
        menu_choice = 1;
        screen.ExitLoopClosure()();
      });

      auto guest_btn = Button("🎮 Play as Guest", [&] {
        menu_choice = 2;
        screen.ExitLoopClosure()();
      });

      auto leaderboard_btn = Button("🏆 Leaderboard", [&] {
        menu_choice = 3;
        screen.ExitLoopClosure()();
      });

      auto exit_btn = Button("🚪 Exit", [&] {
        menu_choice = 4;
        screen.ExitLoopClosure()();
      });

      auto menu = Container::Vertical({
          guest_btn,
          login_btn,
          signup_btn,
          leaderboard_btn,
          exit_btn,
      });

      auto renderer = Renderer(menu, [&] {
        return vbox({
                   RenderWordleTitle(),
                   text("Not logged in") | center,
                   VerticalSpacer(screen_section_spacer_height),
                   vbox({
                       guest_btn->Render() | size(WIDTH, EQUAL, button_width),
                       text("or") | bold | center,
                       login_btn->Render() | size(WIDTH, EQUAL, button_width),
                       signup_btn->Render() | size(WIDTH, EQUAL, button_width),
                       leaderboard_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       exit_btn->Render() | size(WIDTH, EQUAL, button_width),
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

        if (username == admin_username && password == admin_password) {
          currentPlayer = MakeAdminPlayer();
          ConfirmationDialog("Logged in as admin.");
          continue;
        }

        Player loggedInPlayer;
        bool found = CheckNormalLogin(players, username, password, loggedInPlayer);

        if (found) {
          currentPlayer = loggedInPlayer;
          ConfirmationDialog("Logged in as: " + currentPlayer.username);
        } else {
          ConfirmationDialog("Invalid username or password.");
        }

      } else if (menu_choice == 1) {
        string username = "";
        string password = "";

        bool success = SignupDialog(username, password);
        if (!success) {
          continue;
        }

        if (!IsValidUsername(username)) {
          ConfirmationDialog(
              "Invalid username. Letters, numbers, underscores only.");
          continue;
        }

        if (UsernameTaken(players, username)) {
          ConfirmationDialog("Username already taken.");
          continue;
        }

        Player newPlayer;
        newPlayer.username = username;
        newPlayer.password = password;
        newPlayer.wins = default_stat_value;
        newPlayer.streak = default_stat_value;
        newPlayer.isGuest = false;
        newPlayer.isAdmin = false;

        players.push_back(newPlayer);
        SavePlayers(players);
        currentPlayer = newPlayer;

        ConfirmationDialog("Account created. Logged in as: " +
                          currentPlayer.username);

      } else if (menu_choice == 2) {
        currentPlayer = MakeGuestPlayer();
        int wordLength = ShowPlayModeDialog();
        if (wordLength == 0) {
          continue;
        }

        vector<string> wordBank = LoadWordBank(wordLength);
        if (wordBank.empty()) {
          ConfirmationDialog("That word bank is empty.");
          continue;
        }

        PlaySessionLoop(currentPlayer, players, wordBank, false, wordLength);

      } else if (menu_choice == 3) {
        players = LoadPlayers();
        ShowLeaderboardDialog(players);

      } else if (menu_choice == 4) {
        return 0;
      }

    } else if (currentPlayer.isAdmin) {
      auto play_btn = Button("🎮 Play", [&] {
        menu_choice = 10;
        screen.ExitLoopClosure()();
      });

      auto daily_btn = Button("📆 Play Daily Mode", [&] {
        menu_choice = 11;
        screen.ExitLoopClosure()();
      });

      auto word_bank_btn = Button("🗃️  Word Bank", [&] {
        menu_choice = 12;
        screen.ExitLoopClosure()();
      });

      auto delete_players_btn = Button("➖ Delete Player", [&] {
        menu_choice = 13;
        screen.ExitLoopClosure()();
      });

      auto leaderboard_btn = Button("🏆 Leaderboard", [&] {
        menu_choice = 14;
        screen.ExitLoopClosure()();
      });

      auto logout_btn = Button("🔐 Log Out", [&] {
        menu_choice = 15;
        screen.ExitLoopClosure()();
      });

      auto exit_btn = Button("🚪 Exit", [&] {
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
                   RenderWordleTitle(),
                   separator(),
                   text("Logged in as: admin") | bold | center,
                   VerticalSpacer(screen_section_spacer_height),
                   vbox({
                       play_btn->Render() | size(WIDTH, EQUAL, button_width),
                       daily_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       word_bank_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       delete_players_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       leaderboard_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       logout_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       exit_btn->Render() | size(WIDTH, EQUAL, button_width),
                   }) |
                       center,
               }) |
               center;
      });

      screen.Loop(renderer);

      if (menu_choice == 10) {
        int wordLength = ShowPlayModeDialog();
        if (wordLength == 0) {
          continue;
        }

        vector<string> wordBank = LoadWordBank(wordLength);
        if (wordBank.empty()) {
          ConfirmationDialog("That word bank is empty.");
          continue;
        }

        PlaySessionLoop(currentPlayer, players, wordBank, false, wordLength);
      } else if (menu_choice == 11) {
        vector<string> wordBank = LoadWordBank(5);
        if (wordBank.empty()) {
          ConfirmationDialog(DailyWordBankEmptyMessage());
          continue;
        }

        PlaySessionLoop(currentPlayer, players, wordBank, true, 5);
      } else if (menu_choice == 12) {
        int wordLength = ShowWordBankModeDialog();
        if (wordLength == 0) {
          continue;
        }

        vector<string> wordBank = LoadWordBank(wordLength);
        ShowWordBankDialog(wordBank, wordLength);
      } else if (menu_choice == 13) {
        players = LoadPlayers();
        ShowDeletePlayersDialog(players);
        players = LoadPlayers();
      } else if (menu_choice == 14) {
        players = LoadPlayers();
        ShowLeaderboardDialog(players);
      } else if (menu_choice == 15) {
        currentPlayer = MakeGuestPlayer();
        ConfirmationDialog("Logged out.");
      } else if (menu_choice == 16) {
        return 0;
      }

    } else {
      auto play_btn = Button("🎮 Play", [&] {
        menu_choice = 20;
        screen.ExitLoopClosure()();
      });

      auto daily_btn = Button("📆 Play Daily Mode", [&] {
        menu_choice = 21;
        screen.ExitLoopClosure()();
      });

      auto leaderboard_btn = Button("🏆 Leaderboard", [&] {
        menu_choice = 22;
        screen.ExitLoopClosure()();
      });

      auto logout_btn = Button("🔐 Log Out", [&] {
        menu_choice = 23;
        screen.ExitLoopClosure()();
      });

      auto exit_btn = Button("🚪 Exit", [&] {
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
                   RenderWordleTitle(),
                   separator(),
                   text("Logged in as: " + currentPlayer.username) | bold |
                       center,
                   VerticalSpacer(screen_section_spacer_height),
                   vbox({
                       play_btn->Render() | size(WIDTH, EQUAL, button_width),
                       daily_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       leaderboard_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       logout_btn->Render() |
                           size(WIDTH, EQUAL, button_width),
                       exit_btn->Render() | size(WIDTH, EQUAL, button_width),
                   }) |
                       center,
               }) |
               center;
      });

      screen.Loop(renderer);

      if (menu_choice == 20) {
        int wordLength = ShowPlayModeDialog();
        if (wordLength == 0) {
          continue;
        }

        vector<string> wordBank = LoadWordBank(wordLength);
        if (wordBank.empty()) {
          ConfirmationDialog("That word bank is empty.");
          continue;
        }

        PlaySessionLoop(currentPlayer, players, wordBank, false, wordLength);
      } else if (menu_choice == 21) {
        vector<string> wordBank = LoadWordBank(5);
        if (wordBank.empty()) {
          ConfirmationDialog(DailyWordBankEmptyMessage());
          continue;
        }

        PlaySessionLoop(currentPlayer, players, wordBank, true, 5);
      } else if (menu_choice == 22) {
        players = LoadPlayers();
        ShowLeaderboardDialog(players);
      } else if (menu_choice == 23) {
        currentPlayer = MakeGuestPlayer();
        ConfirmationDialog("Logged out.");
      } else if (menu_choice == 24) {
        return 0;
      }
    }
  }

  return 0;
}
