/* -------------------------------------------------
   player.c – Hunt & Target Battleship Player
   ① ループ無しで盤面初期化・S 周囲除外
   ② B/C/D/S 撃沈時に周囲 8 マスを NOSHIP
   ③ HUNT (市松) ＆ TARGET 戦略で攻撃
   ------------------------------------------------- */
   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   #include <my-ipc.h>
   #include <client-side.h>
   #include <redundant.h>
   #include <public.h>       /* MSG_LEN, BD_SIZE など ─ BD_SIZE は 9 */
   
   #define QMAX 128          /* ターゲットキュー上限 */
   
   /* ---------- 定数 / 列挙型 ------------------------------------------ */
   const char myName[] = "strongest";
   
   const char deployment[] =
     "Bd5e5f5g5 "
     "Ci1i2i3 "
     "Cc6c7c8 "
     "Df1f2 "
     "Dh7h8 "
     "Sa1 Sc3 Se7 Sg5 ";
   
   enum cell {
     UNKNOWN,
     ROCK,
     NOSHIP,
     BSHIP,
     CSHIP,
     DSHIP,
     SSHIP
   };
   
   /* ---------- 盤面 & 状態 ------------------------------------------- */
   static enum cell enemy[BD_SIZE][BD_SIZE];
   static int  cur_x, cur_y;                 /* 直前の攻撃座標           */
   
   static enum { HUNT, TARGET } mode = HUNT; /* フェーズ */
   
   typedef struct { int x, y; } Point;
   
   /* --- ターゲットキュー（リングバッファ） --------------------------- */
   static Point queue[QMAX];
   static int qh = 0, qt = 0;
   #define Q_EMPTY (qh == qt)
   static void q_push(int x, int y){
     if(enemy[x][y] == UNKNOWN){
       queue[qt++] = (Point){x,y};
       if(qt >= QMAX) qt = 0;
     }
   }
   static Point q_pop(void){
     Point p = queue[qh++];
     if(qh >= QMAX) qh = 0;
     return p;
   }
   
   /* ---------- ヘルパ: ループ無しで初期化 ---------------------------- */
   static void init_board(void)
   {
     /* 配列全体を UNKNOWN で埋める */
     memset(enemy, UNKNOWN, sizeof(enemy));
   
     /* 岩マスだけ個別指定 (例として 0,0) */
     enemy[0][0] = ROCK;
   }
   
   /* ---------- ヘルパ: S 撃沈時に周囲を NOSHIP ---------------------- */
   static void set_if_unknown(int nx, int ny){
     if(nx>=0 && nx<BD_SIZE && ny>=0 && ny<BD_SIZE &&
        enemy[nx][ny] == UNKNOWN)
       enemy[nx][ny] = NOSHIP;
   }
   
   static void mark_around_noship(int x, int y)
   {
     /* 左上から時計回りに 8 マスを列挙 */
     set_if_unknown(x-1, y+1);
     set_if_unknown(x  , y+1);
     set_if_unknown(x+1, y+1);
     set_if_unknown(x-1, y  );
     set_if_unknown(x+1, y  );
     set_if_unknown(x-1, y-1);
     set_if_unknown(x  , y-1);
     set_if_unknown(x+1, y-1);
   }
   
   /* ---------- 連結艦を DFS で収集 → 沈没判定 ----------------------- */
   static int collect_component(int x,int y, enum cell tp,
                                Point buf[], int *n)
   {
     if(x<0||x>=BD_SIZE||y<0||y>=BD_SIZE) return 0;
     if(enemy[x][y] != tp)                return 0;
   
     enemy[x][y] = (enum cell)(tp | 0x80);        /* 訪問済みフラグ */
     buf[(*n)++] = (Point){x,y};
   
     collect_component(x+1,y,tp,buf,n);
     collect_component(x-1,y,tp,buf,n);
     collect_component(x,y+1,tp,buf,n);
     collect_component(x,y-1,tp,buf,n);
     return 1;
   }
   
   static void restore_marks(Point comp[], int n, enum cell tp){
     for(int i=0;i<n;i++) enemy[comp[i].x][comp[i].y] = tp;
   }
   
   static void check_and_mark_sunk(int x,int y)
   {
     enum cell tp = enemy[x][y];
     int need = (tp==BSHIP)?4 : (tp==CSHIP)?3 : (tp==DSHIP)?2 : 0;
     if(need == 0) return;                         /* S は対象外 */
   
     Point comp[9]; int cnt = 0;
     collect_component(x,y,tp,comp,&cnt);
     restore_marks(comp,cnt,tp);
   
     if(cnt == need){                              /* 沈没確定 */
       for(int i=0;i<cnt;i++)
         mark_around_noship(comp[i].x, comp[i].y);
     }
   }
   
   /* ---------- 方向配列 & HUNT カーソル ------------------------------ */
   static const int dir4[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
   static int hunt_x=0, hunt_y=0;
   static void advance_hunt_cursor(void){
     do{
       hunt_x++;
       if(hunt_x>=BD_SIZE){ hunt_x=0; hunt_y++; if(hunt_y>=BD_SIZE) hunt_y=0; }
     }while( (hunt_x+hunt_y)&1 );   /* 市松 parity */
   }
   
   /* ---------- record_result ---------------------------------------- */
   static void record_result(int x,int y,char line[])
   {
     char r = line[13];
   
     switch(r){
       case 'B': enemy[x][y] = BSHIP; break;
       case 'C': enemy[x][y] = CSHIP; break;
       case 'D': enemy[x][y] = DSHIP; break;
       case 'S': enemy[x][y] = SSHIP; break;
       case 'R': enemy[x][y] = ROCK ; break;
       default : enemy[x][y] = NOSHIP;break;
     }
   
     /* 命中時の処理 */
     if(r=='B'||r=='C'||r=='D'||r=='S'){
       mode = TARGET;
   
       if(r=='S'){                                  /* 潜水艦は1マス */
         mark_around_noship(x,y);
       }else{                                       /* 他艦：隣接をキューへ */
         for(int i=0;i<4;i++)
           q_push(x+dir4[i][0], y+dir4[i][1]);
       }
     }
   
     /* B/C/D 沈没チェック */
     if(enemy[x][y]==BSHIP||enemy[x][y]==CSHIP||enemy[x][y]==DSHIP)
       check_and_mark_sunk(x,y);
   
     if(Q_EMPTY) mode = HUNT;
   }
   
   /* ---------- respond_with_shot ----------------------------------- */
   static void respond_with_shot(void)
   {
     char shot[MSG_LEN];
     int x,y;
   
     while(1){
       if(mode==TARGET && !Q_EMPTY){
         Point p=q_pop(); x=p.x; y=p.y;
       }else{
         x=hunt_x; y=hunt_y; advance_hunt_cursor();
       }
       if(enemy[x][y]==UNKNOWN) break;              /* 未攻撃マスのみ */
     }
   
     sprintf(shot,"%d%d",x,y);
     printf("[%s] shooting at %s ... ", myName, shot);
     send_to_ref(shot);
     cur_x=x; cur_y=y;
   }
   
   /* ---------- ネットワーク I/O ラッパ ------------------------------ */
   static void send_name(void){  char*s=strdup(myName); send_to_ref(s); free(s);}
   static void send_deploy(void){char*s=strdup(deployment); send_to_ref(s);free(s);}
   
   /* ---------- メインハンドラ -------------------------------------- */
   static void handle_messages(void)
   {
     char line[MSG_LEN];
     srand(getpid());
     init_board();
   
     while(TRUE){
       receive_from_ref(line);
   
       if(message_has_type(line,"name?"))          send_name();
       else if(message_has_type(line,"deployment?")) send_deploy();
       else if(message_has_type(line,"shot?"))       respond_with_shot();
       else if(message_has_type(line,"shot-result:")){
         record_result(cur_x,cur_y,line);
         printf("[%s] result: %c\n", myName, line[13]);
       }
       else if(message_has_type(line,"end:")) break;
     }
   }
   
   /* ---------- main ------------------------------------------------- */
   int main(void)
   {
     client_make_connection();
     handle_messages();
     client_close_connection();
     return 0;
   }
   