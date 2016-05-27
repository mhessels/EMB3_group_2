 #include <stdio.h>
 #include<math.h>
#include "xaxidma.h"
 #include "platform.h"
 #include "xgpio.h"
 #include "xil_printf.h"
 #include "xstatus.h"
 
 #define false 0
 #define true 1
 
 #define paddle_width 10
 #define paddle_height 70
 
 #define BUTTON_DEVICE_ID XPAR_AXI_GPIO_0_DEVICE_ID
 #define SWITCH_DEVICE_ID XPAR_AXI_GPIO_1_DEVICE_ID
 #define LED_DEVICE_ID    XPAR_AXI_GPIO_2_DEVICE_ID

//DMA defines
#define DeviceId XPAR_AXIDMA_0_DEVICE_ID
#define DDR_BASE_ADDR	XPAR_PS7_DDR_0_S_AXI_BASEADDR
#define MEM_BASE_ADDR       (DDR_BASE_ADDR + 0x1000000)
#define TX_BUFFER_BASE      (MEM_BASE_ADDR + 0x00100000)

 
 #define printf xil_printf							/* smaller, optimised printf */
 
 #define PADDLE_SPEED_SCALE 0.00001
 
 enum STATE{INITIALIZE,MENU,READ_GPIO,MOVE,CALCULATE,UPDATE_SCREEN};
 enum RES{HIGH,LOW};
 
 int max_x,max_y;
 
 XGpio gpio_button;
 XGpio gpio_switch;
 XGpio gpio_led;
 
 //USER DEFINED FUNCTIONS
 int gpio_initialize();
 void statemachine(int state); // the main function that loops between the different states
 void generate_map(int x,int y, int map[x][y]);
 void draw_paddle(double pos,char side,int x,int y, int map[x][y]);
 void draw_ball(int ball_x, int ball_y, int x,int y, int map[x][y]);
 int change_dir(int ball_dir);
 void move_ball(int ball_x ,int ball_y,int ball_dir);
 void update_score(int score_1, int score_2, int x, int y, int map[x][y]);
 
 int main()
 {
   init_platform();
   
   int state = INITIALIZE;
   statemachine(state);
   
   cleanup_platform();
   return 0;
 }
 
 void statemachine(int state){
   int score_1 = 0, score_2 = 0;
   u32 sw_1 = 0, sw_2 = 0, sw_4 = 0, btn_1 = 0, btn_2 = 0, btn_4 = 0, init = 0;
   double paddle_1_pos = 0.0, paddle_2_pos = 0.0;
   int map[800][600]; // The maximum resolution controls how big the array should be.
   int ball_x = 0, ball_y = 0; // Ball position on the map.
   int running = 0; //Is the game running
   int ball_dir = 4; //Ball direction. 8 point connectivity
   
   XAxiDma AxiDma;
   XAxiDma_Config *CfgPtr;
   u8 *TxBufferPtr;
   CfgPtr = XAxiDma_LookupConfig(DeviceId);
   TxBufferPtr = (u8 *)TX_BUFFER_BASE ;
   int Status;


   while(1){
     switch(state){
       case INITIALIZE:
         Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
         if(Status != XST_SUCCESS){
        	 return;
         }

    	 init = gpio_initialize();
         if(init == 0){
           state = READ_GPIO;
         }
         break;
       case MENU:
         if(sw_2){
           max_x = 800;
           max_y = 600;
         }else{
           max_x = 640;
           max_y = 480;
         }
         if(btn_2){
           generate_map(max_x,max_y,map);
           running = 1;
           state = MOVE;
         }else{
           running = 0;
         }
         break;
       case READ_GPIO: ;
       u32 button_state = XGpio_DiscreteRead(&gpio_button,1);
       u32 switch_state = XGpio_DiscreteRead(&gpio_switch,1);
       sw_1 = switch_state & (0x1);
       sw_2 = switch_state & (0x2);
       sw_4 = switch_state & (0x8);
       btn_1 = button_state & (0x1);
       btn_2 = button_state & (0x2);
       btn_4 = button_state & (0x8);
       if(running){
         state = MOVE;
       }else{
         state = MENU;
       }
       break; 
       case MOVE: ;
       int move_p1_up = 0;
       int move_p2_up = 0;
       //Check if player 1 has to move
       if(sw_1 & btn_1){
         if(paddle_1_pos < 1){
           move_p1_up = 1;
         }
       }else if(btn_1){
         if(paddle_1_pos > 0){
           move_p1_up = -1;
         }
       }
       //Check if player 2 has to move
       if(sw_4 & btn_4){
         if(paddle_2_pos < 1){
           move_p2_up = 1;
         }
       }else if(btn_4){
         if(paddle_2_pos > 0){
           move_p2_up = -1;
         }
       }
       paddle_1_pos += PADDLE_SPEED_SCALE*move_p1_up;
       draw_paddle(paddle_1_pos,'l',max_x,max_y,map);
       paddle_2_pos += PADDLE_SPEED_SCALE*move_p2_up;
       draw_paddle(paddle_2_pos,'r',max_x,max_y,map);
       state = CALCULATE;
       break;
       case CALCULATE:
         if(ball_x < 40){
           if(fabs(paddle_1_pos*max_y - ball_y) > 20){
             ball_dir = change_dir(ball_dir);
           }else{
             score_1++;
             if(score_1 == 10){
               state = MENU;
             }
             ball_x = max_x /2;
             ball_y = max_y /2;
           }
         }
         if(ball_x > max_x - 40){
           if(fabs(paddle_2_pos*max_y - ball_y) > 20){
             ball_dir = change_dir(ball_dir);
           }else{
             score_2++;
             if(score_2 == 10){
               state = MENU;
             }
             ball_x = max_x /2;
             ball_y = max_y /2;
           }
         }
         move_ball(ball_x,ball_y,ball_dir);
         state = UPDATE_SCREEN;
         break;
       case UPDATE_SCREEN:
         draw_ball(ball_x,ball_y,max_x,max_y,map);
         update_score(score_1,score_2,max_x,max_y,map);

         //Send via DMA
         int i,j;
         for(i = 0;i<max_y;i++){
        	 for(j = 0;j<max_x;j++){
              TxBufferPtr[i*max_y+j] = map[j][i];
        	 }
         }
         Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) TxBufferPtr,
                     max_x*max_y, XAXIDMA_DMA_TO_DEVICE);
         while ((XAxiDma_Busy(&AxiDma,XAXIDMA_DEVICE_TO_DMA)) ||
             (XAxiDma_Busy(&AxiDma,XAXIDMA_DMA_TO_DEVICE))) {
                 /* Wait */
         }
         state = READ_GPIO;
         break;
     }
   }
 }
 
 int gpio_initialize(){
   
   int status = 0; //indicator for if the initialization was successful
   
   status |= XGpio_Initialize(&gpio_button, BUTTON_DEVICE_ID);
   status |= XGpio_Initialize(&gpio_switch, SWITCH_DEVICE_ID);
   status |= XGpio_Initialize(&gpio_led, LED_DEVICE_ID);
   
   XGpio_SetDataDirection(&gpio_button,1,0xFF);
   XGpio_SetDataDirection(&gpio_switch,1,0xFF);
   XGpio_SetDataDirection(&gpio_led,1,0x00);
   XGpio_DiscreteWrite(&gpio_led,1,0x9);
   return status;
 }
 
 void generate_map(int x, int y, int map[x][y]){
   int i = 0,j = 0;
   for(i = 0;i<max_x;i++){
     for(j = 0;j<max_y;j++){
       if((i < 10) || (i > (max_x - 10)) || ( j < 10) || (j > (max_y - 10))){
         map[i][j] = 255;
       }
     }
   }
   draw_paddle(0.5,'l',max_x,max_y, map);
   draw_paddle(0.5,'r',max_x,max_y, map);
   int ball_x = max_x / 2;
   int ball_y = max_y / 2;
   draw_ball(ball_x,ball_y,max_x,max_y,map);
 };
 
 void draw_paddle(double pos, char side,int x, int y, int map[x][y]){
   
   if(pos*max_y > max_y - 40)
     return;
   
   int i, j = pos*max_y;
   if(side == 'l'){
     i = 40; 
   }else{
     i = max_x - 40; 
   }
   
   for(; i < paddle_width;i++){
     for(j = pos*max_y;j < paddle_height;j++){
       map[i][j] = 255;
     }
   }
 }
 
 void draw_ball(int ball_x, int ball_y, int x,int y, int map[x][y]){
   int i = ball_x-5, j = ball_y - 5;
   for(;i<ball_x + 5;i++){
     for(j = ball_y - 5;j<ball_y+5;j++){
       map[i][j] = 255;
     }
     
   }
 }
 
 int change_dir(int ball_dir){
   int return_val = 0;
   switch(ball_dir){
     case 1:
       return_val = 3;
       break;
     case 3:
       return_val = 1;
       break;
     case 4:
       return_val = 6;
       break;
     case 6:
       return_val = 4;
       break;
     case 7:
       return_val = 9;
       break;
     case 9:
       return_val = 7;
       break;
   }
   return return_val;
 }

 void move_ball(int ball_x,int ball_y, int ball_dir){
   switch(ball_dir){
     case 1:
       ball_x--;
       ball_y--;
       break;
     case 3:
       ball_x++;
       ball_y--;
       break;
     case 4:
       ball_x--;
       break;
     case 6:
       ball_x++;
       break;
     case 7:
       ball_x--;
       ball_y++;
       break;
     case 9:
       ball_x++;
       ball_y++;
       break;
   }
   
 }

 void update_score(int score_1, int score_2,int x, int y, int map[x][y]){
   int i = 30, j = 2;
   for(;i<10*score_1;i++){
    for(j=2;j<8;j++){
     map[i][j] = 0; 
    }
   }
   
   for(i = max_x - 130;i< (max_x-130) + 10*score_2;i++){
    for(j=2;j<8;j++){
     map[i][j] = 0; 
    }
   }
 }
