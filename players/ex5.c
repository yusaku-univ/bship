#include <stdio.h>
#include <stdlib.h>
#include <my-ipc.h>
#include <client-side.h>
#include <redundant.h>
#include <public.h>

const char myName[] = "ex5";
const char deployment[] = "Ba3a4a5a6 Cc1c2c3 Cc5c6c7 De1e2 De4e5 De7e8 Sg1 Sg3 Sg5 Sg7 ";

enum ship {
  UNKNOWN,
  ROCK,
  NOSHIP,
  BSHIP,
  CSHIP,
  DSHIP,
  SSHIP
};

int cur_x,cur_y;
enum ship enemy_board[BD_SIZE][BD_SIZE];    // BD_SIZE is 9 (defined in public.h)

void respond_with_name(void)
{
  char *str = (char *)malloc(sizeof(myName));
  strcpy(str, myName);
  send_to_ref(str);
  free(str);
}

void respond_with_deployment(void)
{
  char *str = (char *)malloc(sizeof(deployment));
  strcpy(str, deployment);
  send_to_ref(str);
  free(str);
}


void init_board(void){
  int ix, iy;

  for(ix = 0; ix < (BD_SIZE); ix++)
  {
    for(iy = 0; iy < (BD_SIZE); iy++)
    {
      //======kokokara======
      enemy_board[ix][iy] = UNKNOWN;

      //======kokomade======
    }
  }

  //rock is out of bound

  enemy_board[0][0] = ROCK;

  //======kokokara======
  enemy_board[0][1] = ROCK;
  enemy_board[0][8] = ROCK;
  enemy_board[1][8] = ROCK;
  enemy_board[0][7] = ROCK;
  enemy_board[8][0] = ROCK;
  enemy_board[7][0] = ROCK;
  enemy_board[8][1] = ROCK;
  enemy_board[8][8] = ROCK;
  enemy_board[8][7] = ROCK;
  enemy_board[7][8] = ROCK;\
  //======kokomade======
}

void respond_with_shot(void)
{
  char shot_string[MSG_LEN];
  int x, y;
  
  while (TRUE)
  {
    x = rand() % BD_SIZE;
    y = rand() % BD_SIZE;
    //=====kokokara====
    if (enemy_board[x][y] == UNKNOWN)
    {
      break;
    }
    //=====kokomade=====
  }
  printf("[%s] shooting at %d%d ... ", myName, x, y);
  sprintf(shot_string, "%d%d", x, y);
  send_to_ref(shot_string);
  cur_x = x;
  cur_y = y;
}

void record_result(int x,int y,char line[])
{
  if(line[13]=='B')
  {
    //====kokokara====
    enemy_board[x][y] = BSHIP;
    //====kokomade====
  }
  else if(line[13]=='C')
  {
    //====kokokara====
    enemy_board[x][y] = CSHIP;
    //====kokomade====
  }
  else if(line[13]=='D')
  {
    //====kokokara====
    enemy_board[x][y] = DSHIP;
    //====kokomade====
  }
  else if(line[13]=='S')
  {
    //====kokokara====
    enemy_board[x][y] = SSHIP;
    //====kokomade====
  }
  else if(line[13]=='R')
  {
    //====kokokara====
    enemy_board[x][y] = ROCK;
    //====kokomade====
  }
  else
  {
    //====kokokara====
    enemy_board[x][y] = NOSHIP;
    //====kokomade====
  }
}

void print_board(void){
  int ix, iy;

  for (iy = BD_SIZE - 1; iy >= 0; iy--)
  {
    printf("%2d ", iy);
    for (ix = 0; ix < BD_SIZE; ix++)
    {
      switch(enemy_board[ix][iy])
      {
        case UNKNOWN:
          printf("U ");
          break;
        case NOSHIP:
          printf("N ");
          break;
        case ROCK:
          printf("R ");
          break;
        case BSHIP:
          printf("B ");
          break;
        case CSHIP:
          printf("C ");
          break;
        case DSHIP:
          printf("D ");
          break;
        case SSHIP:
          printf("S ");
          break;
        default:
          break;
      }
    }
    printf("\n");
  }

  printf("  ");
  for (ix = 0; ix < BD_SIZE; ix++)
  {
    printf("%2d", ix);
  }
  printf("\n\n");
}

void handle_messages(void)
{
  char line[MSG_LEN];

  srand(getpid());
  init_board();
  
  while (TRUE)
  {
    receive_from_ref(line);

    if(message_has_type(line, "name?"))
    {
      respond_with_name(); 
    }
    else if(message_has_type(line, "deployment?"))
    {
       respond_with_deployment(); 
    }
    else if(message_has_type(line, "shot?"))
    {
      respond_with_shot(); 
    }
    else if(message_has_type(line, "shot-result:"))
    {
      record_result(cur_x,cur_y,line);
      printf("[%s] result: %c\n", myName, line[13]);
      print_board();
    }
    else if(message_has_type(line, "end:"))
    {
      break;
    }
    else
    {
      printf("[%s] ignoring message: %s", myName, line);
    }
  }
}

int main()
{
  client_make_connection();
  handle_messages();
  client_close_connection();
  return 0;
}
