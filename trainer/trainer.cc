// trainer.cpp : Defines the entry point for the application.
//

#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <windows.h>

#include <string>

#include "enum_match.h"
#include "game.h"
#include "handmaster.h"
#include "parse_line.h"
#include "resource.h"
#include "vpoker.h"

const char const *szAppName = "VideoPokerTrainer";
const wchar_t *const wideAppName = L"VideoPokerTrainer";

const COLORREF yellow = RGB(255, 253, 193);

const int timer_id = 999;
const int delay_1 = 300;  // Pause when all cards are face down
const int delay_2 = 30;   // Pause between cards

card c_display[5];
int c_face_up;
unsigned c_held;
const char *caption;

void reset_display() {
  c_face_up = 0;
  c_held = 0;
  caption = 0;
}

HINSTANCE global_instance;
HWND game_cbox, level_cbox;

HANDLE held_bitmap;
BITMAP held_bitmap_size;

HANDLE wild_bitmap;
BITMAP wild_bitmap_size;

HANDLE back_bitmap;
BITMAP back_bitmap_size;
HBRUSH back_brush;

HBRUSH background;

HFONT hfont;
TEXTMETRIC font_info;

// int cardwidth = 142, cardheight = 198;
const int cardwidth = 113, cardheight = 158;
int control_height;
int window_height;
int window_width;

const int buttonheight = 30;
const int buttonwidth = 50;

namespace display_parms {
// const int width = 120;
// const int height = 200;
const int separation = 10;

};  // namespace display_parms

class emf_bitmap {
  bool initialized;

 public:
  HBITMAP bitmap;
  HDC mem_hdc;

  int height, width;

  emf_bitmap() { initialized = false; }

  void create(HDC hdc, int idr_number, int height);
  void make_red(HDC hdc, emf_bitmap &black);
  bool is_initialized() { return initialized; }
  ~emf_bitmap();
};

class text_resource {
  char *current;
  int chars_left;

 public:
  int line_number;
  text_resource(int resource_number);
  char *get_line();
};

text_resource::text_resource(int resource_number) {
  HRSRC str_resource =
      FindResourceA(NULL, MAKEINTRESOURCEA(resource_number), "txt");
  _ASSERT(str_resource);

  chars_left = SizeofResource(NULL, str_resource);
  current = (char *)LockResource(LoadResource(NULL, str_resource));
  line_number = 1;
}

char *text_resource::get_line() {
  if (chars_left <= 0) return 0;

  for (;;) {
    switch (*current) {
      case '\n':
        line_number += 1;
        // fall into return case

      case '\r':
        chars_left -= 1;
        current += 1;

        if (chars_left <= 0) return 0;
        break;

      default: {
        int len = strcspn(current, "\r\n");
        if (len > chars_left) len = chars_left;

        char *result = new char[len + 1];
        memcpy(result, current, len);
        result[len] = 0;

        chars_left -= len;
        current += len;

        return result;
      }
    }
  }
}
// Uses cards.h out of precompiled header.

strategy selected_strategy;

game_parameters *selected_game;
char *game_name;

struct level_elt {
  int resource_num;
  char *level;

  level_elt(int n, char *s) : resource_num(n), level(s) {};
};

typedef std::vector<level_elt> level_list;

struct game_elt {
  char *game_name;
  level_list levels;
  game_elt(char *g) : game_name(g) {};
};

typedef std::vector<game_elt> game_list;
game_list the_games;

int selected_game_index = 0;
int selected_level = 0;

void delete_strategy_file() {
  // Erase the current strategy
  // It would be cleaner if strategy_line
  // used constructors and destructors.

  for (strategy::iterator j = selected_strategy.begin();
       j != selected_strategy.end(); ++j) {
    for (line_list::iterator rover = (*j).begin(); rover != (*j).end();
         ++rover) {
      if ((*rover).pattern) delete (*rover).pattern;
      if ((*rover).options) delete (*rover).options;
      if ((*rover).image) delete (*rover).image;
    }
  }

  selected_strategy.clear();
}

void read_strategy_file() {
  text_resource f(
      the_games[selected_game_index].levels[selected_level].resource_num);
  try {
    game_name = f.get_line();
    vp_game *g = vp_game::find(game_name);
    _ASSERT(g);

    selected_game = new game_parameters(*g);
    initialize_deck();

    delete game_name;
    delete f.get_line();  // the strategy level

    for (int j = 0; j < selected_game->number_wild_cards + 1; j++) {
      selected_strategy.push_back(line_list());
    }

    int wild_count = 0;
    line_list *tail = &selected_strategy.front();

    for (;;) {
      char *const line = f.get_line();
      if (!line) break;

      if ('0' <= *line && *line <= '4' &&
          (strcmp(line + 1, " Deuces") == 0 || strcmp(line, "1 Deuce") == 0)) {
        wild_count = *line - '0';
        _ASSERT(wild_count <= selected_game->number_wild_cards);
        tail = &(selected_strategy[wild_count]);
      } else {
        // Nonempty line
        strategy_line strline;
        parse_line(line,
                   selected_game->number_wild_cards == 0 ? -1 : wild_count,
                   strline);
        tail->push_back(strline);
      }

      delete line;
    }

    for (strategy::iterator k = selected_strategy.begin();
         k != selected_strategy.end(); ++k) {
      _ASSERT((*k).size() > 0);
    }
  } catch (std::string msg) {
    char buffer[128];
    sprintf(buffer, "Line %d: ", f.line_number);
    std::string full(buffer);
    full.append(msg);

    MessageBoxA(NULL, full.c_str(), "Error", MB_OK);
  }
}

// Structure to mimic the game/level menus

struct lt_c_string {
  bool operator()(const char *t1, const char *t2) const {
    return strcmp(t1, t2) < 0;
  }
};

void read_strategy_files() {
  the_games.reserve(20);  // hack that should be fixed.

  typedef std::map<char *, level_list *, lt_c_string> menu_map_type;
  menu_map_type menu_map;

  for (int j = IDR_STRAT_BEGIN + 1; j < IDR_STRAT_END; j++) {
    text_resource f(j);

    char *game_name = f.get_line();
    _ASSERT(game_name);

    char *strategy_level = f.get_line();
    _ASSERT(strategy_level);

    std::pair<menu_map_type::iterator, bool> x =
        menu_map.insert(menu_map_type::value_type(
            game_name, reinterpret_cast<level_list *>(NULL)));

    if (x.second) {
      the_games.push_back(game_elt(game_name));
      x.first->second = &the_games.back().levels;
    }

    x.first->second->push_back(level_elt(j, strategy_level));
  }
}

void load_level_menu(int game_number) {
  selected_game_index = game_number;
  SendMessage(level_cbox, CB_RESETCONTENT, 0, 0);

  level_list &m = the_games[game_number].levels;
  for (level_list::iterator rover = m.begin(); rover != m.end(); ++rover) {
    SendMessage(level_cbox, CB_ADDSTRING, 0, (LPARAM)(*rover).level);
  }

  SendMessage(level_cbox, CB_SETCURSEL, 0, 0);
  selected_level = 0;
}

void emf_bitmap::create(HDC hdc, int idr_number, int height) {
  _ASSERT(!initialized);
  initialized = true;

  HRSRC emf_resource = FindResourceA(NULL, MAKEINTRESOURCEA(idr_number), "emf");

  _ASSERT(emf_resource);

  const HENHMETAFILE mf = SetEnhMetaFileBits(
      SizeofResource(NULL, emf_resource),
      (BYTE *)LockResource(LoadResource(NULL, emf_resource)));

  _ASSERT(mf);

  this->height = height;

  {
    ENHMETAHEADER hdr;
    // hdr.iType = EMR_HEADER;
    // hdr.nSize = sizeof hdr;
    GetEnhMetaFileHeader(mf, sizeof hdr, &hdr);

    int emh_height = hdr.rclBounds.bottom - hdr.rclBounds.top;
    int emh_width = hdr.rclBounds.right - hdr.rclBounds.left;

    width = (emh_width * height + emh_height / 2) / emh_height;

#ifdef TRACE
    fprintf(trace_file, "rect t=%d b=%d l=%d r=%d\n", hdr.rclBounds.top,
            hdr.rclBounds.bottom, hdr.rclBounds.left, hdr.rclBounds.right);
    fprintf(trace_file, "records = %d\n", hdr.nRecords);
#endif
  }

  bitmap = CreateCompatibleBitmap(hdc, width, height);
  _ASSERT(bitmap);

  mem_hdc = CreateCompatibleDC(hdc);
  SelectObject(mem_hdc, (HGDIOBJ)bitmap);

  {
    RECT rect;
    rect.left = 0;
    rect.right = width;
    rect.top = 0;
    rect.bottom = height;

    FillRect(mem_hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    PlayEnhMetaFile(mem_hdc, mf, &rect);
  }

  _ASSERT(DeleteEnhMetaFile(mf));
}

void emf_bitmap::make_red(HDC hdc, emf_bitmap &black) {
  _ASSERT(!initialized && black.initialized);
  initialized = true;

  width = black.width;
  height = black.height;

  bitmap = CreateCompatibleBitmap(hdc, width, height);
  _ASSERT(bitmap);

  mem_hdc = CreateCompatibleDC(hdc);
  SelectObject(mem_hdc, (HGDIOBJ)bitmap);

  {
    HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));

    RECT rect;
    rect.left = 0;
    rect.right = width;
    rect.top = 0;
    rect.bottom = height;

    FillRect(mem_hdc, &rect, red);
    DeleteObject(red);
  }

  BitBlt(mem_hdc, 0, 0, width, height, black.mem_hdc, 0, 0, SRCPAINT);
}

emf_bitmap::~emf_bitmap() {
  if (initialized) {
    DeleteDC(mem_hdc);
    DeleteObject((HGDIOBJ)bitmap);
  }
}

emf_bitmap black_map[num_denoms], red_map[num_denoms], court_icon[num_denoms],
    small_suit[num_suits], large_suit[num_suits], wild_icon, joker_icon;

int max_denom_width;
int max_icon_width = 0;

static int card_left(int j) {
  return display_parms::separation +
         j * (display_parms::separation + cardwidth);
}

void card_pos(int j, RECT &r) {
  r.left = card_left(j);

  r.right = r.left + cardwidth;

  r.top = control_height + display_parms::separation +
          held_bitmap_size.bmHeight + display_parms::separation;

  r.bottom = r.top + cardheight;
}

void held_pos(int j, RECT &r) {
  r.left = card_left(j) + (cardwidth - held_bitmap_size.bmWidth) / 2;
  r.right = r.left + held_bitmap_size.bmWidth;
  r.top = control_height + display_parms::separation;
  r.bottom = r.top + held_bitmap_size.bmHeight;
}

const int card_outline = 1;
HGDIOBJ outline_pen;

void draw_card(HDC hdc, RECT &cc, card value, bool face_up) {
  {  // Draw the card outline

    const HGDIOBJ restore1 = SelectObject(
        hdc, face_up ? GetStockObject(WHITE_BRUSH) : (HGDIOBJ)back_brush);
    const HGDIOBJ restore2 = SelectObject(hdc, outline_pen);

    int hs = (cc.right - cc.left) / 10;
    if (hs == 0) hs = 1;
    int cs = 2 * hs;

    BeginPath(hdc);
    MoveToEx(hdc, cc.left + hs, cc.top, NULL);

    ArcTo(hdc, cc.left, cc.top, cc.left + cs, cc.top + cs, cc.left + hs, cc.top,
          cc.left, cc.top + hs);

    LineTo(hdc, cc.left, cc.bottom - hs);

    ArcTo(hdc, cc.left, cc.bottom - cs, cc.left + cs, cc.bottom, cc.left,
          cc.bottom - hs, cc.left + hs, cc.bottom);

    LineTo(hdc, cc.right - hs, cc.bottom - 1);

    ArcTo(hdc, cc.right - cs, cc.bottom - cs, cc.right, cc.bottom,
          cc.right - hs, cc.bottom, cc.right, cc.bottom - hs);

    LineTo(hdc, cc.right - 1, cc.top + hs);

    ArcTo(hdc, cc.right - cs, cc.top, cc.right, cc.top + cs, cc.right,
          cc.top + hs, cc.right - hs, cc.top);

    CloseFigure(hdc);
    EndPath(hdc);
    StrokeAndFillPath(hdc);

    SelectObject(hdc, restore1);
    SelectObject(hdc, restore2);
  }

  if (!face_up) return;

  if (value == joker) {
    if (joker_icon.is_initialized()) {
      // Center the joker in the card frame.
      BitBlt(hdc, (cc.left + cc.right - joker_icon.width) / 2,
             (cc.top + cc.bottom - joker_icon.height) / 2, joker_icon.width,
             joker_icon.height, joker_icon.mem_hdc, 0, 0, SRCCOPY);
    }
  } else
  // Draw the denom
  {
    int s = suit(value);
    emf_bitmap *bm =
        (is_black(suit(value)) ? black_map : red_map) + pips(value);

    int left_base = cc.left + 10 + max_denom_width / 2;
    int top_base = cc.top + 12;

    BitBlt(hdc, left_base - bm->width / 2, top_base, bm->width, bm->height,
           bm->mem_hdc, 0, 0, SRCCOPY);

    emf_bitmap *ss = small_suit + s;

    BitBlt(hdc, left_base - ss->width / 2, top_base + bm->height + 10,
           ss->width, ss->height, ss->mem_hdc, 0, 0, SRCCOPY);

    emf_bitmap *ls = large_suit + s;

    // Center the suit at the bottom of the card

    BitBlt(hdc, cc.left + (cardwidth - ls->width) / 2,
           cc.bottom - 12 - ls->height, ls->width, ls->height, ls->mem_hdc, 0,
           0, SRCCOPY);

    emf_bitmap *ci =
        selected_game->is_wild(value) ? &wild_icon : court_icon + pips(value);

    if (ci->is_initialized()) {
      int right_base = cc.right - 14 - max_icon_width / 2;
      // Right justify the card
      BitBlt(hdc, right_base - ci->width / 2, cc.top + 14, ci->width,
             ci->height, ci->mem_hdc, 0, 0, SRCCOPY);
    }
  }
}

int caption_y() {
  return control_height + display_parms::separation +
         held_bitmap_size.bmHeight + display_parms::separation + cardheight +
         display_parms::separation - font_info.tmInternalLeading;
};

void do_paint(HWND hwnd) {
  PAINTSTRUCT ps;
  const HDC hdc = BeginPaint(hwnd, &ps);

  HDC bitmap_hdc = CreateCompatibleDC(hdc);

  const HGDIOBJ restore_bitmap = SelectObject(bitmap_hdc, (HGDIOBJ)NULL);
  const HGDIOBJ restore_font = SelectObject(hdc, hfont);
  const unsigned restore_align = GetTextAlign(hdc);
  const COLORREF restore_color = GetTextColor(hdc);
  const int restore_bk = GetBkMode(hdc);

  SetTextColor(hdc, yellow);
  SetBkMode(hdc, TRANSPARENT);

  for (int j = 0; j < 5; j++) {
    RECT cc;
    card_pos(j, cc);
    draw_card(hdc, cc, c_display[j], j < c_face_up);

    held_pos(j, cc);
    if (c_held & (1 << j)) {
      SelectObject(bitmap_hdc, (HGDIOBJ)held_bitmap);
      BitBlt(hdc, cc.left, cc.top, held_bitmap_size.bmWidth,
             held_bitmap_size.bmHeight, bitmap_hdc, 0, 0, SRCCOPY);
    }
  }

  if (c_face_up >= 5 && caption != 0) {
    SetTextAlign(hdc, TA_CENTER | TA_TOP | TA_NOUPDATECP);

    const int x = (display_parms::separation +
                   5 * (cardwidth + display_parms::separation)) /
                  2;

    TextOutA(hdc, x, caption_y(), caption, strlen(caption));
  }

  SetTextColor(hdc, restore_color);
  SetBkMode(hdc, restore_bk);
  SetTextAlign(hdc, restore_align);
  SelectObject(hdc, restore_font);
  SelectObject(bitmap_hdc, restore_bitmap);

  DeleteDC(bitmap_hdc);

  EndPaint(hwnd, &ps);
}

void left_click(HWND hwnd, int x, int y) {
  if (c_face_up < 5) {
    // Ignore left click unless all the cards are showing
    return;
  }

  for (int j = 0; j < 5; j++) {
    RECT rect;
    card_pos(j, rect);

    if (rect.left <= x && x < rect.right && rect.top <= y && y < rect.bottom) {
      c_held ^= (1 << j);

      held_pos(j, rect);
      InvalidateRect(hwnd, &rect, TRUE);
    }
  }
}

void new_hand() {
  if (selected_strategy.size() == 0) read_strategy_file();
  deal_hand(c_display, caption);
  c_face_up = 0;
  c_held = 0;
}

void right_click(HWND hwnd) {
  if (1 < c_face_up && c_face_up < 5) {
    // Ignore right click while the cards are being dealt
    return;
  }

  if (c_face_up == 0 || is_right_move(c_display, c_held)) {
    // Get a new hand
    new_hand();
    InvalidateRect(hwnd, NULL, TRUE);
    SetTimer(hwnd, timer_id, delay_1, NULL);
  } else {
    got_it_wrong();
    // Make a rude noise

    PlaySound(MAKEINTRESOURCE(IDR_WRONG), global_instance,
              SND_RESOURCE | SND_ASYNC);
  }
}

void display_answer(HWND hwnd) {
  if (c_face_up < 5) {
    // Ignore right click unless all the cards are showing
    return;
  }

  find_right_move(c_display, c_held, caption);

  for (int j = 0; j < 5; j++) {
    RECT rect;
    held_pos(j, rect);
    InvalidateRect(hwnd, &rect, TRUE);
  }

  // Display the caption
  {
    RECT rect;
    rect.top = caption_y();
    rect.bottom = window_height;
    rect.left = 0;
    rect.right = window_width;
    InvalidateRect(hwnd, &rect, TRUE);
  }
}

static void do_timer(HWND hwnd) {
  KillTimer(hwnd, timer_id);

  if (c_face_up >= 5) {
    if (caption != 0) {
      PlaySound(MAKEINTRESOURCE(IDR_PAY), global_instance,
                SND_RESOURCE | SND_ASYNC);
    }

    return;
  }

  c_face_up += 1;
  InvalidateRect(hwnd, NULL, FALSE);  // could improve this

  SetTimer(hwnd, timer_id, delay_2, NULL);
}

const int ID_ANSWER = 100;
const int ID_DEAL = 101;
const int ID_GAMENAME = 102;
const int ID_STRATEGY_LEVEL = 103;

void change_game(HWND hwnd) {
  if (selected_strategy.size() != 0) {
    reset_display();
    InvalidateRect(hwnd, NULL, TRUE);
    erase_hands();
    delete_strategy_file();
  }
}

static int do_command(HWND hwnd, int ctrl_id, int notify) {
  switch (ctrl_id) {
    case ID_ANSWER:
      display_answer(hwnd);
      SetFocus(hwnd);
      return true;

    case ID_DEAL:
      right_click(hwnd);
      SetFocus(hwnd);
      return true;

    case ID_GAMENAME:
      if (notify == CBN_SELENDOK) {
        int new_game = SendMessage(game_cbox, CB_GETCURSEL, 0, 0);

        if (new_game != CB_ERR && new_game != selected_game_index) {
          load_level_menu(new_game);
          change_game(hwnd);
        }

        SetFocus(hwnd);
        return true;
      }
      break;

    case ID_STRATEGY_LEVEL:
      if (notify == CBN_SELENDOK) {
        int new_level = SendMessage(level_cbox, CB_GETCURSEL, 0, 0);

        if (new_level != CB_ERR && new_level != selected_level) {
          selected_level = new_level;
          change_game(hwnd);
        }

        SetFocus(hwnd);
        return true;
      }
      break;
  }
  return false;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      return 0;

    case WM_LBUTTONDOWN:
      left_click(hwnd, LOWORD(lParam), HIWORD(lParam));
      return 0;

    case WM_TIMER:
      do_timer(hwnd);
      return 0;

    case WM_RBUTTONDOWN:
      right_click(hwnd);
      return 0;

    case WM_COMMAND:
      if (do_command(hwnd, LOWORD(wParam), HIWORD(wParam))) return 0;
      break;

    case WM_PAINT:
      do_paint(hwnd);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void make_button(const char *label, int id, int &x, HWND hwnd,
                 HINSTANCE hInst) {
  SIZE button_size;
  HDC hdc = GetDC(hwnd);
  TEXTMETRIC tm;
  GetTextMetrics(hdc, &tm);
  GetTextExtentPoint32A(hdc, label, strlen(label), &button_size);
  ReleaseDC(hwnd, hdc);

  button_size.cx += 3 * tm.tmAveCharWidth;
  button_size.cy = control_height;

  CreateWindowA("button", label, WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                x = x - button_size.cx, 4,

                button_size.cx, button_size.cy,

                hwnd, (HMENU)id, hInst, NULL);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine,
                   int nShowCmd) {
  HWND hwnd;
  MSG msg;
  WNDCLASSEX wndclass;

  global_instance = hInst;

  background = CreateSolidBrush(RGB(0, 0, 220));

  held_bitmap = LoadImage(hInst, MAKEINTRESOURCE(IDB_HELD), IMAGE_BITMAP, 0, 0,
                          LR_DEFAULTCOLOR);
  GetObject(held_bitmap, sizeof(held_bitmap_size), &held_bitmap_size);

  back_bitmap = LoadImage(hInst, MAKEINTRESOURCE(IDB_BACK), IMAGE_BITMAP, 0, 0,
                          LR_DEFAULTCOLOR);
  GetObject(back_bitmap, sizeof(back_bitmap_size), &back_bitmap_size);
  back_brush = CreatePatternBrush((HBITMAP)back_bitmap);

  const DWORD Style =
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME;

  // Window class for the main application parent window
  wndclass.cbSize = sizeof(wndclass);
  wndclass.style = 0, wndclass.lpfnWndProc = WndProc;
  wndclass.cbClsExtra = 0;
  wndclass.cbWndExtra = 0;
  wndclass.hInstance = hInst;
  wndclass.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON));
  wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
  wndclass.hbrBackground = background;
  wndclass.lpszMenuName = 0;
  wndclass.lpszClassName = wideAppName;
  wndclass.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON));

  RegisterClassEx(&wndclass);

  hwnd = CreateWindowA(szAppName,              // window class name
                       "Video Poker Trainer",  // window caption
                       Style,
                       CW_USEDEFAULT,  // initial x position
                       0,              // initial y position
                       CW_USEDEFAULT,  // x size
                       0,              // y size
                       NULL,           // parent window handle
                       NULL,           // use window class menu
                       hInst,          // program instance handle
                       NULL);          // creation parameters

  {
    LOGFONT lfont;
    ZeroMemory(&lfont, sizeof lfont);

    lfont.lfHeight = cardheight / 7;
    lfont.lfWeight = FW_NORMAL;
    lfont.lfCharSet = ANSI_CHARSET;
    lfont.lfQuality = PROOF_QUALITY;
    lfont.lfPitchAndFamily = FF_SWISS;
    wcscpy(lfont.lfFaceName, L"Arial");

    hfont = CreateFontIndirect(&lfont);
  }

  // Size the window to hold the text
  {
    HDC hdc = GetDC(hwnd);
    const HGDIOBJ restore_font = SelectObject(hdc, hfont);

    GetTextMetrics(hdc, &font_info);

    SelectObject(hdc, restore_font);

    ReleaseDC(hwnd, hdc);
  }

  // Create the bitmaps to display the cards
  {
    HDC hdc = GetDC(hwnd);
    int denom_height = (int)(0.5 + (float)cardheight * .2);
    int small_suit_height = (int)(0.5 + (float)cardheight * .175);
    int large_suit_height = (int)(0.5 + (float)cardheight * .375);
    int icon_height = denom_height + small_suit_height;

    int j;

    max_denom_width = 0;

    for (j = 0; j < num_denoms; j++) {
      black_map[j].create(hdc, IDR_A_EMF + j, denom_height);
      red_map[j].make_red(hdc, black_map[j]);

      if (black_map[j].width > max_denom_width) {
        max_denom_width = black_map[j].width;
      }
    }

    for (j = 0; j < num_suits; j++) {
      small_suit[j].create(hdc, IDR_SPADE_EMF + j, small_suit_height);
      large_suit[j].create(hdc, IDR_SPADE_EMF + j, large_suit_height);

      if (small_suit[j].width > max_denom_width) {
        max_denom_width = small_suit[j].width;
      }
    }

    court_icon[jack].create(hdc, IDR_JACK_EMF, icon_height);
    court_icon[queen].create(hdc, IDR_QUEEN_EMF, icon_height);
    court_icon[king].create(hdc, IDR_KING_EMF, icon_height);
    court_icon[ace].create(hdc, IDR_ACE_EMF, icon_height);
    wild_icon.create(hdc, IDR_WILD_EMF, icon_height);
    joker_icon.create(hdc, IDR_JOKER_EMF,
                      static_cast<int>(cardheight * .8 + .5));

    max_icon_width = court_icon[jack].width;
    if (court_icon[queen].width > max_icon_width)
      max_icon_width = court_icon[queen].width;
    if (court_icon[king].width > max_icon_width)
      max_icon_width = court_icon[king].width;
    if (court_icon[ace].width > max_icon_width)
      max_icon_width = court_icon[ace].width;

    ReleaseDC(hwnd, hdc);
  }

  read_strategy_files();

  game_cbox = CreateWindowA(
      "combobox", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
      0, 0, 220, 200, hwnd, (HMENU)ID_GAMENAME, hInst, NULL);

  level_cbox = CreateWindowA(
      "combobox", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 230, 0, 180,
      cardheight, hwnd, (HMENU)ID_STRATEGY_LEVEL, hInst, NULL);

  for (game_list::iterator rover = the_games.begin(); rover != the_games.end();
       ++rover) {
    SendMessage(game_cbox, CB_ADDSTRING, 0, (LPARAM)(*rover).game_name);
  }

  // Someday save defaults in the registry
  SendMessage(game_cbox, CB_SETCURSEL, 0, 0);
  load_level_menu(0);

  control_height = SendMessage(game_cbox, CB_GETITEMHEIGHT, -1, 0);

  {
    RECT current;
    GetClientRect(game_cbox, &current);
    control_height = current.bottom - current.top;
  }

  {
    RECT current;
    GetWindowRect(hwnd, &current);

    RECT wsize;

    wsize.top = 0;
    wsize.left = 0;
    wsize.bottom = window_height =
        control_height + display_parms::separation + held_bitmap_size.bmHeight +
        display_parms::separation + cardheight + display_parms::separation +
        font_info.tmAscent + display_parms::separation;

    wsize.right = window_width =
        display_parms::separation + 5 * (cardwidth + display_parms::separation);

    AdjustWindowRect(&wsize, Style, FALSE);

    MoveWindow(hwnd, current.left, current.top, wsize.right - wsize.left,
               wsize.bottom - wsize.top, FALSE);
  }

  {
    int b_x = 5 * (cardwidth + display_parms::separation);
    make_button("DEAL", ID_DEAL, b_x, hwnd, hInst);
    b_x -= display_parms::separation;
    make_button("ANSWER", ID_ANSWER, b_x, hwnd, hInst);
  }

  reset_display();
  ShowWindow(hwnd, nShowCmd);
  // SetTimer (hwnd, timer_id, delay_1, NULL);

  outline_pen = CreatePen(PS_SOLID, card_outline, RGB(0, 0, 0));

  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DeleteObject(hfont);
  DeleteObject(background);
  DeleteObject(held_bitmap);

  DeleteObject(back_bitmap);
  DeleteObject(back_brush);
  DeleteObject(outline_pen);

  return 0;
}
