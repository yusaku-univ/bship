/* -------------------------------------------------
   player.c – Hunt & Target Battleship Player
   (c) ChatGPT strongest sample 2025
   ------------------------------------------------- */

   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   #include <my-ipc.h>
   #include <client-side.h>
   #include <redundant.h>
   #include <public.h>          /* MSG_LEN, BD_SIZE など */
   
   /*-----------------------------------------------
     定数・列挙型
     ---------------------------------------------*/
   const char myName[]   = "strongest";
   const char deployment[] =
     "Ba3a4a5a6 Cc1c2c3 Cc5c6c7 De1e2 De4e5 De7e8 "
     "Sg1 Sg3 Sg5 Sg7 ";
   
   enum cell {
     UNKNOWN,
     ROCK,
     NOSHIP,
     BSHIP,
     CSHIP,
     DSHIP,
     SSHIP
   };
   
   /*-----------------------------------------------
     盤面とゲーム状態
     ---------------------------------------------*/
   static enum cell enemy[BD_SIZE][BD_SIZE];
   static int  cur_x, cur_y;              /* 直前に撃った座標            */
   
   static enum { HUNT, TARGET } mode = HUNT;
   
   /* 方向ベクトル */
   static const int dir4[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
   
   /* ターゲットキュー (単純リングバッファ) */
   #define QMAX  128
   typedef struct { int x, y; } Point;
   static Point queue[QMAX];
   static int qhead = 0, qtail = 0;
   #define Q_EMPTY (qhead == qtail)
   static void q_push(int x, int y){
     if(enemy[x][y] == UNKNOWN){      /* 未知マスのみ */
       queue[qtail++] = (Point){x,y};
       if(qtail >= QMAX) qtail = 0;
     }
   }
   static Point q_pop(void){
     Point p = queue[qhead++];
     if(qhead >= QMAX) qhead = 0;
     return p;
   }
   
   /*-----------------------------------------------
     初期化
     ---------------------------------------------*/
   static void init_board(void)
   {
     for(int x=0; x<BD_SIZE; x++)
       for(int y=0; y<BD_SIZE; y++)
         enemy[x][y] = UNKNOWN;
   
     /* たとえば (0,0) が岩なら：*/
     enemy[0][0] = ROCK;
   }
   
   /*-----------------------------------------------
     周囲 8 マスを NOSHIP にする (Submarine 撃沈時)
     ---------------------------------------------*/
   static void mark_around_noship(int x, int y)
   {
     for(int dx=-1; dx<=1; dx++){
       for(int dy=-1; dy<=1; dy++){
         int nx=x+dx, ny=y+dy;
         if(nx<0||nx>=BD_SIZE||ny<0||ny>=BD_SIZE) continue;
         if(enemy[nx][ny] == UNKNOWN)
           enemy[nx][ny] = NOSHIP;
       }
     }
   }
   
   /*-----------------------------------------------
     record_result : line[13] の文字に応じ盤面を更新
     ---------------------------------------------*/
   static void record_result(int x,int y,char line[])
   {
     char r = line[13];
   
     switch(r){
       case 'B': enemy[x][y] = BSHIP; break;
       case 'C': enemy[x][y] = CSHIP; break;
       case 'D': enemy[x][y] = DSHIP; break;
       case 'S': enemy[x][y] = SSHIP; break;
       case 'R': enemy[x][y] = ROCK;  break;
       default : enemy[x][y] = NOSHIP;break; /* miss or water */
     }
   
     /*--- 命中時の処理 --------------------------------*/
     if(r=='B' || r=='C' || r=='D' || r=='S'){
       /* TARGET フェーズ突入 */
       mode = TARGET;
   
       if(r == 'S'){                /* 1マス艦は即座に周囲を除外 */
         mark_around_noship(x, y);
       }else{
         /* 四方向に隣接マスをキューに積む */
         for(int i=0;i<4;i++){
           int nx = x + dir4[i][0];
           int ny = y + dir4[i][1];
           if(nx>=0 && nx<BD_SIZE && ny>=0 && ny<BD_SIZE)
             q_push(nx, ny);
         }
       }
     }
   
     /*--- キューが空なら TARGET 終了 --------------------*/
     if(Q_EMPTY)
       mode = HUNT;
   }
   
   /*-----------------------------------------------
     探索パターン（市松模様）用カーソル
     ---------------------------------------------*/
   static int hunt_x = 0, hunt_y = 0;
   static void advance_hunt_cursor(void)
   {
     /* 市松模様 (奇偶パリティ) を維持しながら線形に回す */
     do {
       hunt_x++;
       if(hunt_x >= BD_SIZE){
         hunt_x = 0;
         hunt_y++;
         if(hunt_y >= BD_SIZE) hunt_y = 0;
       }
     }while( ((hunt_x+hunt_y)&1) ); /* parity 合わないときスキップ */
   }
   
   /*-----------------------------------------------
     respond_with_shot : HUNT / TARGET 戦略で次弾決定
     ---------------------------------------------*/
   static void respond_with_shot(void)
   {
     char shot_string[MSG_LEN];
     int x, y;
   
     while(1){
       if(mode == TARGET && !Q_EMPTY){
         /* キュー先頭から取り出す */
         Point p = q_pop();
         x = p.x; y = p.y;
       }else{
         /* HUNT フェーズ：市松模様探索 */
         x = hunt_x; y = hunt_y;
         advance_hunt_cursor();
       }
   
       /* 未知マスのみ撃つ */
       if(enemy[x][y] == UNKNOWN)
         break;
     }
   
     /* 発射 */
     printf("[%s] shooting at %d%d ... ", myName, x, y);
     sprintf(shot_string, "%d%d", x, y);
     send_to_ref(shot_string);
     cur_x = x;
     cur_y = y;
   }
   
   /*-----------------------------------------------
     可視化 (デバッグ)
     ---------------------------------------------*/
   static void print_board(void)
   {
     for(int y=BD_SIZE-1; y>=0; y--){
       printf("%2d ", y);
       for(int x=0; x<BD_SIZE; x++){
         char c='?';
         switch(enemy[x][y]){
           case UNKNOWN: c='U'; break;
           case ROCK   : c='R'; break;
           case NOSHIP : c='.'; break;
           case BSHIP  : c='B'; break;
           case CSHIP  : c='C'; break;
           case DSHIP  : c='D'; break;
           case SSHIP  : c='S'; break;
         }
         printf("%c ", c);
       }
       printf("\n");
     }
     printf("   ");
     for(int x=0;x<BD_SIZE;x++) printf("%2d",x);
     printf("\n\n");
   }
   
   /*-----------------------------------------------
     ネットワーク I/O ラッパ
     ---------------------------------------------*/
   static void respond_with_name(void)
   {
     char *str = strdup(myName);
     send_to_ref(str); free(str);
   }
   
   static void respond_with_deployment(void)
   {
     char *str = strdup(deployment);
     send_to_ref(str); free(str);
   }
   
   /*-----------------------------------------------
     メインループ
     ---------------------------------------------*/
   static void handle_messages(void)
   {
     char line[MSG_LEN];
   
     srand(getpid());
     init_board();
   
     while(TRUE){
       receive_from_ref(line);
   
       if(message_has_type(line, "name?")){
         respond_with_name();
       }else if(message_has_type(line, "deployment?")){
         respond_with_deployment();
       }else if(message_has_type(line, "shot?")){
         respond_with_shot();
       }else if(message_has_type(line, "shot-result:")){
         record_result(cur_x, cur_y, line);
         printf("[%s] result: %c\n", myName, line[13]);
   #ifdef DEBUG_VIEW
         print_board();          /* 必要なら板表示 */
   #endif
       }else if(message_has_type(line, "end:")){
         break;
       }else{
         printf("[%s] ignoring message: %s", myName, line);
       }
     }
   }
   
   int main(void)
   {
     client_make_connection();
     handle_messages();
     client_close_connection();
     return 0;
   }
   