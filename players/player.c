/* -------------------------------------------------
    player.c – Hunt & Target Battleship Player
    整理版（可読性 / Norm意識）
------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <my-ipc.h>
#include <client-side.h>
#include <redundant.h>
#include <public.h>

#define QMAX 128
#define BD   BD_SIZE

const char myName[] = "03250211";
const char deployment[] =
  "Bd5e5f5g5 Ci3i4i5 Cc3d3e3 Dg2g3 Dg7h7 De7e8 "
  "Sa2 Sb5 Sc8 Sg0 ";

enum cell
{
    UNKNOWN,
    ROCK,
    NOSHIP,
    BSHIP,
    CSHIP,
    DSHIP,
    SSHIP
};

typedef struct { int x, y; } Point;
typedef struct { int x, y, has; } HitInfo;

/* ------------------------------------------------- 盤面・状態 */
static enum cell enemy[BD][BD];
static unsigned char in_queue[BD][BD];
static HitInfo last_hit[7];
static int cur_x, cur_y;
static enum { HUNT, TARGET } mode = HUNT;

/* ------------------------------------------------- ターゲットキュー */
static Point queue[QMAX];
static int qh = 0, qt = 0;
#define Q_EMPTY (qh == qt)

static void push_to_queue(int x, int y)
{
    if (x < 0 || x >= BD || y < 0 || y >= BD)
        return;
    if (enemy[x][y] != UNKNOWN)
        return;
    if (in_queue[x][y])
        return;
    queue[qt++] = (Point){x, y};
    if (qt >= QMAX)
        qt = 0;
    in_queue[x][y] = 1;
}

static Point pop_from_queue(void)
{
    Point p = queue[qh++];
    if (qh >= QMAX)
        qh = 0;
    in_queue[p.x][p.y] = 0;
    return p;
}

/* ------------------------------------------------- 初期化処理 */
static void init_board(void)
{
    memset(enemy, UNKNOWN, sizeof(enemy));
    int rocks[][2] = {
        {0,0},{0,1},{1,0},
        {0,7},{0,8},{1,8},
        {7,0},{8,0},{8,1},
        {8,7},{7,8},{8,8}
    };
    for (size_t i = 0; i < sizeof(rocks) / sizeof(rocks[0]); i++)
        enemy[rocks[i][0]][rocks[i][1]] = ROCK;
    memset(in_queue, 0, sizeof(in_queue));
    memset(last_hit, 0, sizeof(last_hit));
}

/* ------------------------------------------------- ヘルパ関数 */
static void set_noship(int x, int y)
{
    if (x >= 0 && x < BD && y >= 0 && y < BD && enemy[x][y] == UNKNOWN)
        enemy[x][y] = NOSHIP;
}

static void mark_around_as_noship(int x, int y)
{
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            if (dx != 0 || dy != 0)
                set_noship(x + dx, y + dy);
        }
    }
}

static int count_clear_cells(int x, int y, int dx, int dy, enum cell tp)
{
    int steps = 0;
    while (1)
    {
        x += dx;
        y += dy;
        if (x < 0 || x >= BD || y < 0 || y >= BD)
            break;
        if (enemy[x][y] != UNKNOWN && enemy[x][y] != tp)
            break;
        steps++;
    }
    return steps;
}

static int can_place_ship(enum cell tp, int x, int y, int dx, int dy)
{
    int required = (tp == BSHIP) ? 4 : (tp == CSHIP) ? 3 : 2;
    int left = count_clear_cells(x, y, dx, dy, tp);
    int right = count_clear_cells(x, y, -dx, -dy, tp);
    return (left + right >= required - 1);
}

/* ------------------------------------------------- HUNTモード */
static int hunt_x = 0, hunt_y = 0, hunt_parity = 0;

static int advance_hunt_cursor(void)
{
    int start_x = hunt_x, start_y = hunt_y;
    int first = 1;
    while (first || hunt_x != start_x || hunt_y != start_y)
    {
        first = 0;
        hunt_x++;
        if (hunt_x >= BD)
        {
            hunt_x = 0;
            hunt_y++;
            if (hunt_y >= BD)
                hunt_y = 0;
        }
        if (enemy[hunt_x][hunt_y] != UNKNOWN)
            continue;
        if (hunt_parity < 2)
        {
            if (((hunt_x + hunt_y) & 1) == hunt_parity)
                return 1;
        }
        else
            return 1;
    }
    if (hunt_parity < 2)
    {
        hunt_parity++;
        return advance_hunt_cursor();
    }
    return 0;
}

/* ------------------------------------------------- 攻撃結果反映 */
static const int dx4[4] = {1, -1, 0, 0};
static const int dy4[4] = {0, 0, 1, -1};

static void collect_component(int x, int y, enum cell tp, Point *buf, int *n)
{
    if (x < 0 || x >= BD || y < 0 || y >= BD)
        return;
    if (enemy[x][y] != tp)
        return;
    enemy[x][y] = (enum cell)(tp | 0x80);
    buf[(*n)++] = (Point){x, y};
    collect_component(x + 1, y, tp, buf, n);
    collect_component(x - 1, y, tp, buf, n);
    collect_component(x, y + 1, tp, buf, n);
    collect_component(x, y - 1, tp, buf, n);
}

static void restore_component(Point *buf, int n, enum cell tp)
{
    for (int i = 0; i < n; i++)
        enemy[buf[i].x][buf[i].y] = tp;
}

static void check_and_mark_sunk(int x, int y)
{
    enum cell tp = enemy[x][y];
    int len = (tp == BSHIP) ? 4 : (tp == CSHIP) ? 3 : (tp == DSHIP) ? 2 : 0;
    if (!len)
        return;
    Point comp[9];
    int cnt = 0;
    collect_component(x, y, tp, comp, &cnt);
    restore_component(comp, cnt, tp);
    if (cnt == len)
    {
        for (int i = 0; i < cnt; i++)
            mark_around_as_noship(comp[i].x, comp[i].y);
    }
}

static void record_result(int x, int y, char *line)
{
    char r = line[13];
    enum cell tp = (r == 'B') ? BSHIP : (r == 'C') ? CSHIP :
                   (r == 'D') ? DSHIP : (r == 'S') ? SSHIP :
                   (r == 'R') ? ROCK : NOSHIP;
    enemy[x][y] = tp;

    if (tp == BSHIP || tp == CSHIP || tp == DSHIP || tp == SSHIP)
    {
        mode = TARGET;

        if (tp == SSHIP)
            mark_around_as_noship(x, y);
        else
        {
            HitInfo *lh = &last_hit[tp];
            int direction = -1;
            if (lh->has)
            {
                for (int i = 0; i < 4; i++)
                    if (lh->x + dx4[i] == x && lh->y + dy4[i] == y)
                        direction = i;
            }
            if (direction != -1)
            {
                if (can_place_ship(tp, x, y, dx4[direction], dy4[direction]))
                    push_to_queue(x + dx4[direction], y + dy4[direction]);
                if (can_place_ship(tp, x, y, -dx4[direction], -dy4[direction]))
                    push_to_queue(x - dx4[direction], y - dy4[direction]);
            }
            else
            {
                for (int i = 0; i < 4; i++)
                    if (can_place_ship(tp, x, y, dx4[i], dy4[i]))
                        push_to_queue(x + dx4[i], y + dy4[i]);
            }
            *lh = (HitInfo){x, y, 1};
        }
    }

    if (tp == BSHIP || tp == CSHIP || tp == DSHIP)
        check_and_mark_sunk(x, y);

    if (Q_EMPTY)
        mode = HUNT;
}

/* ------------------------------------------------- respond_with_shot */
static void respond_with_shot(void)
{
    char shot[MSG_LEN];
    int x, y;

    while (1)
    {
        if (mode == TARGET && !Q_EMPTY)
        {
            Point p = pop_from_queue();
            if (enemy[p.x][p.y] != UNKNOWN)
                continue;
            x = p.x;
            y = p.y;
        }
        else
        {
            if (!advance_hunt_cursor())
            {
                sprintf(shot, "00");
                send_to_ref(shot);
                return;
            }
            x = hunt_x;
            y = hunt_y;
        }
        break;
    }
    sprintf(shot, "%d%d", x, y);
    send_to_ref(shot);
    cur_x = x;
    cur_y = y;
}

/* ------------------------------------------------- handle_messages */
static void send_text(const char *text)
{
    char *s = strdup(text);
    send_to_ref(s);
    free(s);
}

static void handle_messages(void)
{
    char line[MSG_LEN];
    srand(getpid());
    init_board();

    while (TRUE)
    {
        receive_from_ref(line);
        if (message_has_type(line, "name?"))
            send_text(myName);
        else if (message_has_type(line, "deployment?"))
            send_text(deployment);
        else if (message_has_type(line, "shot?"))
            respond_with_shot();
        else if (message_has_type(line, "shot-result:"))
            record_result(cur_x, cur_y, line);
        else if (message_has_type(line, "end:"))
            break;
    }
}

int main(void)
{
    client_make_connection();
    handle_messages();
    client_close_connection();
    return 0;
}
