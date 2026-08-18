#include <QuickDraw.h>
#include <Windows.h>
#include <Events.h>
#include <Menus.h>
#include <Fonts.h>
#include <ToolUtils.h>

#include "similarity.h"

#define HISTORY_LIMIT 256

typedef struct {
    char word[MAX_WORD_LENGTH + 1];
    short raw_score;
    short rank_score;
    short indexed;
} GuessRecord;

static WindowPtr gWindow;
static MenuHandle gFileMenu;
static unsigned long gRandomState;
static unsigned short gTargetIndex;
static short gSolved;
static char gGuess[MAX_WORD_LENGTH + 1];
static short gGuessLength;
static char gStatus[96];
static GuessRecord gHistory[HISTORY_LIMIT];
static short gHistoryCount;
static unsigned short gTopIndices[TOP_NEIGHBOR_COUNT];

static const Rect kContentRect = { 0, 0, 280, 452 };
static const Rect kNewRoundButton = { 332, 178, 428, 204 };

static unsigned char lower_ascii(unsigned char c)
{
    if (c >= (unsigned char)'A' && c <= (unsigned char)'Z')
        return (unsigned char)(c + ((unsigned char)'a' - (unsigned char)'A'));
    return c;
}

static unsigned long next_random(void)
{
    /* A tiny LCG is sufficient for selecting a word and works on a 68000. */
    gRandomState = gRandomState * 1664525UL + 1013904223UL;
    return gRandomState;
}

static void copy_text(char *destination, const char *source, unsigned short limit)
{
    unsigned short i = 0;

    if (limit == 0)
        return;
    while (source != 0 && source[i] != 0 && i + 1 < limit) {
        destination[i] = source[i];
        ++i;
    }
    destination[i] = 0;
}

static short text_length(const char *text)
{
    short length = 0;

    while (text != 0 && text[length] != 0)
        ++length;
    return length;
}

static void draw_c_text(short x, short y, const char *text)
{
    short length = text_length(text);

    MoveTo(x, y);
    if (length > 0)
        DrawText((Ptr)text, 0, length);
}

static void draw_centered(short y, const char *text)
{
    short length = text_length(text);
    short width = 0;

    if (length > 0)
        width = TextWidth((Ptr)text, 0, length);
    draw_c_text((short)((452 - width) / 2), y, text);
}

static void draw_score(short x, short y, short score)
{
    char buffer[6];

    if (score >= 100) {
        buffer[0] = '1';
        buffer[1] = '0';
        buffer[2] = '0';
        buffer[3] = '%';
        buffer[4] = 0;
    } else if (score >= 10) {
        buffer[0] = (char)('0' + (score / 10));
        buffer[1] = (char)('0' + (score % 10));
        buffer[2] = '%';
        buffer[3] = 0;
    } else {
        buffer[0] = (char)('0' + score);
        buffer[1] = '%';
        buffer[2] = 0;
    }
    draw_c_text(x, y, buffer);
}

static void draw_value(short x, short y, short raw_score, short rank_score)
{
    char buffer[6];

    if (rank_score > 0) {
        if (rank_score >= 10) {
            buffer[0] = (char)('0' + (rank_score / 10));
            buffer[1] = (char)('0' + (rank_score % 10));
            buffer[2] = '/';
            buffer[3] = '5';
            buffer[4] = '0';
            buffer[5] = 0;
        } else {
            buffer[0] = (char)('0' + rank_score);
            buffer[1] = '/';
            buffer[2] = '5';
            buffer[3] = '0';
            buffer[4] = 0;
        }
        draw_c_text(x, y, buffer);
    } else {
        draw_score(x, y, raw_score);
    }
}

static void set_status(const char *text)
{
    copy_text(gStatus, text, sizeof(gStatus));
}

static void start_round(void)
{
    unsigned short count = candidate_word_count();

    gTargetIndex = (unsigned short)(next_random() % count);
    gGuessLength = 0;
    gGuess[0] = 0;
    gHistoryCount = 0;
    gSolved = 0;
    prepare_top_neighbors(gTargetIndex, gTopIndices);
    set_status("Type a word; press RETURN.");
}

static void append_history(const char *word, SimilarityResult result)
{
    short i;

    if (gHistoryCount == HISTORY_LIMIT) {
        for (i = 1; i < HISTORY_LIMIT; ++i)
            gHistory[i - 1] = gHistory[i];
        gHistoryCount = HISTORY_LIMIT - 1;
    }
    copy_text(gHistory[gHistoryCount].word, word, MAX_WORD_LENGTH + 1);
    gHistory[gHistoryCount].raw_score = result.raw_score;
    gHistory[gHistoryCount].rank_score = result.rank_score;
    gHistory[gHistoryCount].indexed = result.indexed;
    ++gHistoryCount;
}

static void submit_guess(void)
{
    SimilarityResult result;

    if (gGuessLength == 0) {
        set_status("Please type a word first.");
        return;
    }

    result = similarity_for_target(gGuess, gTargetIndex, gTopIndices);
    append_history(gGuess, result);
    if (candidate_index_for_word(gGuess) == (short)gTargetIndex) {
        gSolved = 1;
        set_status("FOUND! 50/50 - click NEW ROUND.");
    } else if (result.rank_score >= 40) {
        set_status("TOP 50 - exceptionally close.");
    } else if (result.rank_score >= 20) {
        set_status("TOP 50 - strong semantic neighbor.");
    } else if (result.rank_score > 0) {
        set_status("TOP 50 - keep exploring this neighborhood.");
    } else if (!result.indexed) {
        set_status("Not indexed - fallback score shown.");
    } else {
        set_status("Outside top 50 - raw score shown.");
    }
    gGuessLength = 0;
    gGuess[0] = 0;
}

static void draw_button(const Rect *button, const char *label)
{
    FrameRoundRect(button, 8, 8);
    draw_c_text((short)(button->left + 18), (short)(button->top + 17), label);
}

static void collect_top_guesses(short top[4])
{
    short i;
    short j;
    short position;
    short candidate_value;
    short current_value;

    for (i = 0; i < 4; ++i)
        top[i] = -1;

    for (i = 0; i < gHistoryCount; ++i) {
        candidate_value = gHistory[i].rank_score > 0
            ? (short)(100 + gHistory[i].rank_score)
            : gHistory[i].raw_score;
        position = 0;
        while (position < 4 && top[position] >= 0 &&
               (current_value = gHistory[top[position]].rank_score > 0
                    ? (short)(100 + gHistory[top[position]].rank_score)
                    : gHistory[top[position]].raw_score) >= candidate_value)
            ++position;
        if (position < 4) {
            for (j = 3; j > position; --j)
                top[j] = top[j - 1];
            top[position] = i;
        }
    }
}

static void draw_ui(void)
{
    Rect line;
    Rect input;
    short top[4];
    short i;
    short row;

    SetPort((GrafPtr)gWindow);
    TextFont(systemFont);
    TextSize(12);
    TextFace(bold);
    EraseRect(&kContentRect);

    draw_centered(24, "SEMANTLE PLUS");
    TextFace(normal);
    draw_centered(42, "A word is hiding in the neighborhood.");
    draw_centered(57, "Find it by chasing the score.");

    line.top = 68;
    line.left = 22;
    line.bottom = 69;
    line.right = 430;
    FillRect(&line, &qd.black);

    draw_c_text(24, 94, "YOUR WORD");
    input.top = 122;
    input.left = 24;
    input.bottom = 148;
    input.right = 428;
    FrameRoundRect(&input, 6, 6);
    draw_c_text(34, 140, gGuess);
    if (!gSolved)
        draw_c_text((short)(34 + TextWidth((Ptr)gGuess, 0, gGuessLength)), 140, "_");

    collect_top_guesses(top);
    TextFace(bold);
    draw_c_text(24, 160, "TOP 4 GUESSES");
    TextFace(normal);
    draw_c_text(300, 160, "SCORE");
    line.top = 168;
    line.left = 22;
    line.bottom = 169;
    line.right = 430;
    FillRect(&line, &qd.black);

    row = 183;
    for (i = 0; i < 4; ++i) {
        if (top[i] < 0)
            break;
        draw_c_text(28, row, gHistory[top[i]].word);
        draw_value(300, row, gHistory[top[i]].raw_score,
                   gHistory[top[i]].rank_score);
        row += 13;
    }
    if (gHistoryCount == 0)
        draw_c_text(28, 183, "(none yet)");

    draw_button(&kNewRoundButton, "NEW ROUND");
    TextFace(bold);
    draw_c_text(24, 238, "LATEST");
    TextFace(normal);
    if (gHistoryCount > 0) {
        draw_c_text(100, 238, gHistory[gHistoryCount - 1].word);
        draw_value(300, 238, gHistory[gHistoryCount - 1].raw_score,
                   gHistory[gHistoryCount - 1].rank_score);
    } else {
        TextFace(normal);
        draw_c_text(100, 238, "(none yet)");
    }
    TextFace(bold);
    draw_c_text(24, 255, "STATUS");
    TextFace(normal);
    draw_c_text(24, 270, gStatus);
}

static void handle_key(EventRecord *event, short *done)
{
    unsigned char character = (unsigned char)(event->message & charCodeMask);

    if ((event->modifiers & cmdKey) != 0 && (character == 'q' || character == 'Q')) {
        *done = 1;
        return;
    }

    if (character == (unsigned char)'\r' || character == (unsigned char)'\n') {
        submit_guess();
    } else if (character == (unsigned char)'\b' || character == 127) {
        if (gGuessLength > 0) {
            --gGuessLength;
            gGuess[gGuessLength] = 0;
        }
    } else if (character >= (unsigned char)' ' && character <= (unsigned char)'~') {
        if (gGuessLength < MAX_WORD_LENGTH) {
            gGuess[gGuessLength++] = (char)lower_ascii(character);
            gGuess[gGuessLength] = 0;
        }
    }
}

static void handle_menu(long menu_result, short *done)
{
    short menu_id = (short)((menu_result >> 16) & 0xFFFF);
    short item = (short)(menu_result & 0xFFFF);

    if (menu_id == 129 && item == 1)
        *done = 1;
    HiliteMenu(0);
}

static void handle_mouse(EventRecord *event, short *done)
{
    WindowPtr window;
    short part;
    Point local;
    long menu_result;

    part = FindWindow(event->where, &window);
    if (part == inMenuBar) {
        menu_result = MenuSelect(event->where);
        handle_menu(menu_result, done);
        return;
    }
    if (part == inDrag) {
        DragWindow(window, event->where, &qd.screenBits.bounds);
        return;
    }
    if (part != inContent || window != gWindow)
        return;

    SetPort((GrafPtr)gWindow);
    local = event->where;
    GlobalToLocal(&local);
    if (PtInRect(local, &kNewRoundButton))
        start_round();
}

static void setup_menus(void)
{
    MenuHandle apple_menu;

    apple_menu = NewMenu(128, "\p\024");
    AppendMenu(apple_menu, "\pAbout Semantle Plus");
    InsertMenu(apple_menu, 0);

    gFileMenu = NewMenu(129, "\pFile");
    AppendMenu(gFileMenu, "\pQuit/Q");
    InsertMenu(gFileMenu, 0);
    DrawMenuBar();
}

int main(void)
{
    EventRecord event;
    short done = 0;
    Boolean got_event;
    short part;
    WindowPtr window;
    Rect window_rect = { 30, 38, 318, 482 };

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(0L);
    InitCursor();

    word_bank_init();
    gRandomState = (unsigned long)TickCount() ^ 0x5E6D4E54UL;
    gWindow = NewWindow(0L, &window_rect, "\pSemantle Plus", false,
                        documentProc, (WindowPtr)-1L, true, 0L);
    if (gWindow == 0L)
        return 1;

    setup_menus();
    SetPort((GrafPtr)gWindow);
    ShowWindow(gWindow);
    SelectWindow(gWindow);
    start_round();
    draw_ui();

    while (!done) {
        got_event = GetNextEvent(everyEvent, &event);
        if (!got_event) {
            SystemTask();
            continue;
        }

        switch (event.what) {
            case mouseDown:
                handle_mouse(&event, &done);
                break;
            case keyDown:
                handle_key(&event, &done);
                break;
            case updateEvt:
                window = (WindowPtr)event.message;
                if (window == gWindow) {
                    BeginUpdate(gWindow);
                    draw_ui();
                    EndUpdate(gWindow);
                }
                break;
            case activateEvt:
                part = (short)(event.modifiers & activeFlag);
                if (part != 0)
                    SelectWindow(gWindow);
                break;
            default:
                break;
        }
        if (!done)
            draw_ui();
    }

    if (gFileMenu != 0L)
        DeleteMenu(129);
    DisposeWindow(gWindow);
    return 0;
}
