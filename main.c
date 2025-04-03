#include <math.h>
#include <stdbool.h>  // For bool, true, false
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  // For abs, rand
#include "assets.h"

/*GLOBALS*/
volatile int pixel_buffer_start;
short int Buffer1[240][512];  // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

#define BALL_RADIUS 10
#define BALL_COLOR 0x001F
#define BALL_SPEED 4

#define PADDLE_RADIUS 12
#define PADDLE_COLOR 0xFFFFF

// fixed point scale
#define SCALE 1000

bool p1Hit = false;
bool p2Hit = false;
bool previouslyHit = false;

// for fsm game logic
bool start = true;
int winner = 0;

// initial positions for the paddles
int paddle1X = 250;
int paddle2X = 70;
int paddle12Y = 120;

// initial ball position
int ballX = 160;
int ballY = 120;

// array with the hexadecimal values to turn on the correct hex display segments
int hexsegNums[10] = {
    0x3F,  // 0
    0x06,  // 1
    0x5B,  // 2
    0x4F,  // 3
    0x66,  // 4
    0x6D,  // 5
    0x7D,  // 6
    0x07,  // 7
    0x7F,  // 8
    0x6F   // 9
};

// track if the spacerbar pressed
bool spacePressed = false;
// to track if m is pressed
bool mPressed = false;
// to track if pause button is pressed, pPressed is designed to TOGGLE, not like
// other keys
bool pPressed = false;

/*Ball Struct*/
struct Ball {
  // center of ball
  int xc;
  int yc;
  // velocity of ball
  double dxBall;
  double dyBall;
};

/*Initialize all three generations of ball*/
// this is the one that is about to be drawn (back buffer about to swap to front
// buffer) swap 2
struct Ball ball;
// this is the one that has been drawn, (back buffer that has already swapped to
// front buffer) swap 1
struct Ball ballOld;
// this is the previous ball that had been drawn
struct Ball ballOldOld;

/*Paddle Struct*/
struct Paddle {
  // center of paddle
  int xc;
  int yc;
  // paddle number
  int paddleNumber;
  // keyPressed
  bool upPressed;
  bool downPressed;
  bool leftPressed;
  bool rightPressed;
};

/*Initialize all three generations of paddle1 and paddle2*/
// arrow keys, paddle 1
struct Paddle paddle1;
struct Paddle paddle1Old;
struct Paddle paddle1OldOld;

// wasd keys, paddle 2
struct Paddle paddle2;
struct Paddle paddle2Old;
struct Paddle paddle2OldOld;

/*Function prototypes*/
void bounceVector(struct Ball *ball, struct Paddle *p);
bool collidedWPaddle(struct Ball *ball, struct Paddle *p);

/*Display Score*/
#define HEX3_0_PTR \
  ((volatile int *)0xFF200020)  // this is for hex 3 to 0 (right 4)
#define HEX5_4_PTR \
  ((volatile int *)0xFF200030)  // this is for hex 5 and 4, (left 2)

int p1_score = 0;
int p2_score = 0;


/*Keyboard*/
unsigned char keyInput() {
  volatile int *PS2Base = (int *)0xFF200100;
  int PS2Data = *PS2Base;

  // 0x8000 masks the rvalid bit, bit 15, checks if there is unread byte in fif0
  if ((PS2Data & 0x8000) != 0) {
    unsigned char pressedKey = (char)(PS2Data & 0xFF);  // lower 8 bits
    return pressedKey;
  }
  return 0x00;
}

void processKeyInput() {
  unsigned char byte1, byte2, byte3;

  byte1 = keyInput();

  if (byte1 != 0x00) {
    // arrow keys require two bytes of data, make code and data code
    // 0xE0 is make code for arrow keys

    // if arrow keys
    if (byte1 == 0xE0) {
      byte2 = keyInput();

      // 0xF0 is the release code
      // arrow key has been released
      if (byte2 == 0xF0) {
        byte3 = keyInput();
        switch (byte3) {
          case 0x75:
            paddle1.upPressed = false;
            break;
          case 0x72:
            paddle1.downPressed = false;
            break;
          case 0x6B:
            paddle1.leftPressed = false;
            break;
          case 0x74:
            paddle1.rightPressed = false;
            break;
        }
      }
      // arrow key has been pressed
      else {
        switch (byte2) {
          case 0x75:
            paddle1.upPressed = true;
            break;
          case 0x72:
            paddle1.downPressed = true;
            break;
          case 0x6B:
            paddle1.leftPressed = true;
            break;
          case 0x74:
            paddle1.rightPressed = true;
            break;
        }
      }
    }

    // if WASD keys or game navigation keys (no 0xE0 prefix)
    // Key has been released
    else if (byte1 == 0xF0) {
      byte2 = keyInput();
      switch (byte2) {
        case 0x1D:  // W
          paddle2.upPressed = false;
          break;
        case 0x1B:  // S
          paddle2.downPressed = false;
          break;
        case 0x1C:  // A
          paddle2.leftPressed = false;
          break;
        case 0x23:  // D
          paddle2.rightPressed = false;
          break;

        // game navigation keys
        case 0x29:  // Spacebar released
          spacePressed = false;
          break;
        case 0x3A:  // M
          mPressed = false;
          break;
        case 0x4D:
          pPressed = !pPressed;  // toggle pause state
          break;
      }
    }
    // key has been pressed
    else {
      switch (byte1) {
        case 0x1D:
          paddle2.upPressed = true;
          break;
        case 0x1B:
          paddle2.downPressed = true;
          break;
        case 0x1C:
          paddle2.leftPressed = true;
          break;
        case 0x23:
          paddle2.rightPressed = true;
          break;

        // game navigation keys
        case 0x29:  // Spacebar pressed
          spacePressed = true;
          break;
        case 0x3A:
          mPressed = true;
          break;
        case 0x4D:
          pPressed = pPressed;  // dont toggle pause state
          break;
      }
    }
  }
}

/*Paddle*/
void updatePaddlePosition(struct Paddle *paddle) {
  if (paddle->upPressed && paddle->leftPressed) {
    paddle->yc -= 2;  // speed of 2
    paddle->xc -= 2;
  } else if (paddle->upPressed && paddle->rightPressed) {
    paddle->yc -= 2;
    paddle->xc += 2;
  } else if (paddle->downPressed && paddle->leftPressed) {
    paddle->yc += 2;
    paddle->xc -= 2;
  } else if (paddle->downPressed && paddle->rightPressed) {
    paddle->yc += 2;
    paddle->xc += 2;
  } else if (paddle->upPressed) {
    paddle->yc -= 2;
  } else if (paddle->downPressed) {
    paddle->yc += 2;
  } else if (paddle->leftPressed) {
    paddle->xc -= 2;
  } else if (paddle->rightPressed) {
    paddle->xc += 2;
  }
}

void screenLimits(struct Paddle *paddle) {
  if (paddle->yc - PADDLE_RADIUS < 1) {
    paddle->upPressed = false;
  }
  // bottom wall
  if (paddle->yc + PADDLE_RADIUS > 239) {
    paddle->downPressed = false;
  }
  // left wall
  if (paddle->xc - PADDLE_RADIUS < 1) {
    paddle->leftPressed = false;
  }
  // right wall
  if (paddle->xc + PADDLE_RADIUS > 319) {
    paddle->rightPressed = false;
  }

  // middle wall
  // for paddle 1, right side
  if (paddle->paddleNumber == 1 && paddle->xc - PADDLE_RADIUS < 161) {
    paddle->leftPressed = false;
  }
  // for paddle 2, left side
  if (paddle->paddleNumber == 2 && paddle->xc + PADDLE_RADIUS > 159) {
    paddle->rightPressed = false;
  }
}

/*Score and Sound*/
void display_score(int p1, int p2) {
  // store p1 score on the left most hex which is 5
  *HEX5_4_PTR = hexsegNums[p1] << 8;  // shift 8 to the left because the first 6
                                      // are for hex4 and 7 is a gap

  // store p2 score on the right most hex which is 0
  *HEX3_0_PTR = hexsegNums[p2];
}

// sound function. Increasing input value makes sound lower pitch
void ballBounceSound(int toggleRate) {
  volatile int *audioPtr = (int *)0xFF203040;

  int status = 1;
  int counter = 0;
  int burst_length = 8000 / 10;  // 0.1 second at 8000 samples/sec
  int sample_counter = 0;
  // int toggleRate = 40; // increase this to make the sound lower pitch
  while (counter < burst_length) {
    // Wait for space in both left and right FIFOs
    if ((*(audioPtr + 1) & 0xFF0000) && (*(audioPtr + 1) & 0xFF000000)) {
      if (sample_counter >= toggleRate) {
        status *= -1;
        sample_counter = 0;
      }
      if (status == 1) {
        *(audioPtr + 2) = 0xFFFFFF;  // Left channel
        *(audioPtr + 3) = 0xFFFFFF;  // Right channel
      } else {
        *(audioPtr + 2) = 0x0;  // Left channel
        *(audioPtr + 3) = 0x0;  // Right channel
      }
      counter++;
      sample_counter++;
    }
  }
}

void winningSound() {
  ballBounceSound(20);
  ballBounceSound(30);
  ballBounceSound(10);
  ballBounceSound(10);
}

/*Handle Draw*/
void plot_pixel(int x, int y, short int line_color) {
  volatile short int *one_pixel_address;

  one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);

  *one_pixel_address = line_color;
}

// this one is the initial one to make all that random nonsense at the beginning
// dissappear
void clear_screen() {
  int x, y;
  for (x = 0; x < 320; x++) {
    for (y = 0; y < 240; y++) {
      plot_pixel(x, y, 0);
    }
  }
}

// Function to draw a circle using the Midpoint Circle Algorithm (Algo inspired
// by BASIC256 at https://rosettacode.org/wiki/Bitmap/Midpoint_circle_algorithm)
void draw_circle(int xc, int yc, int r, short int color) {
  int x = 0, y = r;
  // decision variable (will help decide if we move horizontally or diagonally)
  int d = 1 - r;

  while (x <= y) {
    // We exploit the symmetry of a circle and draw 8 points at once.
    // This allows to compute only 1 octant of a circle instead of every point
    plot_pixel(xc + x, yc + y, color);
    plot_pixel(xc - x, yc + y, color);
    plot_pixel(xc + x, yc - y, color);
    plot_pixel(xc - x, yc - y, color);
    plot_pixel(xc + y, yc + x, color);
    plot_pixel(xc - y, yc + x, color);
    plot_pixel(xc + y, yc - x, color);
    plot_pixel(xc - y, yc - x, color);

    // based on decision variable, change the y coordinate
    if (d < 0) {
      d += 2 * x + 3;
    } else {
      // next pixel move diagonally
      d += 2 * (x - y) + 5;
      y--;
    }
    x++;
  }
}

void draw_start() {
  for (int x = 0; x < 320; x++) {
    for (int y = 0; y < 240; y++) {
      plot_pixel(x, y, start_screen[x + (y * 320)]);
    }
  }
}

void drawRectangle(int x, int y, int width, int height) {
  // horizontal lines
  for (int i = 0; i < width; i++) {
    // top
    plot_pixel(x + i, y - height, 0xFFFF);
    // bottom
    plot_pixel(x + i, y, 0xFFFF);
  }
  // vertical lines
  for (int i = 0; i < height; i++) {
    // left
    plot_pixel(x, y - i, 0xFFFF);
    // right
    plot_pixel(x + width, y - i, 0xFFFF);
  }
}

void draw_end(int w) {
  for (int x = 0; x < 320; x++) {
    for (int y = 0; y < 240; y++) {
      // show player 1 winning end screen
      if (w == 1) {
        plot_pixel(x, y, player1EndScreen[x + (y * 320)]);
      }
      // show player 2 winning end screen
      else {
        plot_pixel(x, y, player2EndScreen[x + (y * 320)]);
      }
    }
  }
}

void erase_circle(int xc, int yc, int radius) {
  draw_circle(xc, yc, radius, 0x0);
}

void wait_for_vsync() {
  volatile int *pixel_ctrl_ptr = (int *)0xff203020;  // base address
  int status;
  *pixel_ctrl_ptr = 1;  // start the synchronization process
  // write 1 into front buffer address register
  status = *(pixel_ctrl_ptr + 3);  // read the status register
  while ((status & 0x01) != 0)     // polling loop waiting for S bit to go to 0
  {
    status = *(pixel_ctrl_ptr + 3);
  }  // polling loop/function exits when status bit goes to 0
}

void drawLine() {
  for (int y = 0; y < 240; y++) {
    plot_pixel(159, y, 0xFFFF);
  }
}

void eraseLine() {
  for (int y = 0; y < 240; y++) {
    plot_pixel(159, y, 0x0);  // Black color
  }
}

/*Ball Physics*/
double currentBallSpeed(struct Ball *ball) {
  return sqrt(ball->dxBall * ball->dxBall + ball->dyBall * ball->dyBall);
}

// use the reflection of a vector to calculate bounce vector off paddle
// bounce vector = ballV - 2(ballV dot Normal)
void bounceVector(struct Ball *ball, struct Paddle *p) {
  // find vector normal to the hit surface relative to the ball
  double nx = ball->xc - p->xc;
  double ny = ball->yc - p->yc;

  // magnitude of n
  double magN = sqrt(nx * nx + ny * ny);

  // unit vector of n
  nx /= magN;
  ny /= magN;

  /*handle bounce*/
  double dotBallN = ball->dxBall * nx + ball->dyBall * ny;
  // ball velocity vector should be reversed to point away from paddle centre so
  // that dot product works reflected vector = v-2(vdotn)n
  ball->dxBall -= 2 * dotBallN * nx;
  ball->dyBall -= 2 * dotBallN * ny;

  // applies fixed speed to ball if it was hit by moving paddle
  bool paddleMoving =
      (p->downPressed || p->leftPressed || p->rightPressed || p->upPressed);

  if (paddleMoving) {
    double currentSpeed = currentBallSpeed(ball);

    double newSpeed = BALL_SPEED * SCALE;

    if (currentSpeed != 0) {
      ball->dxBall = (ball->dxBall / currentSpeed) * newSpeed;
      ball->dyBall = (ball->dyBall / currentSpeed) * newSpeed;
    } else {
      ball->dxBall = nx * newSpeed;
      ball->dyBall = ny * newSpeed;
    }
  }

  // adjustment to give the ball an extra push outside of the paddle, +1 for
  // safety
  ball->xc += nx * (BALL_RADIUS + PADDLE_RADIUS - magN + 1);
  ball->yc += ny * (BALL_RADIUS + PADDLE_RADIUS - magN + 1);
}

// circle collision detection
bool collidedWPaddle(struct Ball *ball, struct Paddle *p) {
  double deltax = ball->xc - p->xc;
  double deltay = ball->yc - p->yc;
  double currdelta = deltax * deltax + deltay * deltay;
  double conditionDelta = BALL_RADIUS + PADDLE_RADIUS;
  return (currdelta <= conditionDelta * conditionDelta);
}

// resets objects positions
void resetObjects(struct Ball *ball, struct Paddle *p1, struct Paddle *p2) {
  ball->xc = ballX;
  ball->yc = ballY;
  ball->dxBall = 0;
  ball->dyBall = 0;
  p1->xc = paddle1X;
  p1->yc = paddle12Y;
  p1->paddleNumber = 1;
  p2->xc = paddle2X;
  p2->yc = paddle12Y;
  p2->paddleNumber = 2;
}

int main(void) {
  volatile int *pixel_ctrl_ptr = (int *)0xFF203020;
  resetObjects(&ball, &paddle1, &paddle2);

  ballOld = ball;
  ballOldOld = ball;
  paddle1Old = paddle1;
  paddle1OldOld = paddle1;
  paddle2Old = paddle2;
  paddle2OldOld = paddle2;

  /* set front pixel buffer to Buffer 1 */
  // first store the address in the  back buffer
  *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
  /* now, swap the front/back buffers, to set the front buffer location */
  wait_for_vsync();
  /* initialize a pointer to the pixel buffer, used by drawing functions */
  pixel_buffer_start = *pixel_ctrl_ptr;
  clear_screen();  // pixel_buffer_start points to the pixel buffer

  /* set back pixel buffer to Buffer 2 */
  *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
  pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // we draw on the back buffer
  clear_screen();  // pixel_buffer_start points to the pixel buffer

  /*START SCREEN LOOP*/
  while (1) {
    // if starting from menu screen
    if (start == true) {
      // remain on start menu until space is pressed
      while (!spacePressed) {
        draw_start();
        wait_for_vsync();  // swap front and back buffers on VGA vertical sync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
        processKeyInput();
      }

      clear_screen();    // clear screen again
      wait_for_vsync();  // swap front and back buffers on VGA vertical sync
      pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
      clear_screen();
    }

    /*MAIN GAME LOOP*/
    while (1) {
      display_score(p1_score, p2_score);
      // game over after first to 2
      int winningScore = 2;
      if (p1_score == winningScore || p2_score == winningScore) {
        winner = (p1_score == winningScore) ? 1 : 2;
        break;
      }

      /*DRAWING*/
      // erase ball
      erase_circle(ballOldOld.xc, ballOldOld.yc, BALL_RADIUS);
      // erase paddle1
      erase_circle(paddle1OldOld.xc, paddle1OldOld.yc, PADDLE_RADIUS);
      // erase paddle2
      erase_circle(paddle2OldOld.xc, paddle2OldOld.yc, PADDLE_RADIUS);

      // draw the balls
      draw_circle(ball.xc, ball.yc, BALL_RADIUS, BALL_COLOR);
      // draw paddle1
      draw_circle(paddle1.xc, paddle1.yc, PADDLE_RADIUS, PADDLE_COLOR);
      // draw paddle2
      draw_circle(paddle2.xc, paddle2.yc, PADDLE_RADIUS, PADDLE_COLOR);

      // draw dividing line
      drawLine();

      // p1 goal boundary
      drawRectangle(0, 160, 30, 80);
      // p2 goal boundary
      drawRectangle(320 - 30, 160, 30, 80);

      /*BALL*/
      // update each instance of the ball generations
      ballOldOld = ballOld;
      ballOld = ball;

      // update ball location
      // divide by 100 for fixed point arithmetic
      ball.xc += (int)(ball.dxBall / SCALE);
      ball.yc += (int)(ball.dyBall / SCALE);

      /*PADDLE*/
      // update each instance of the paddle generations
      paddle1OldOld = paddle1Old;
      paddle1Old = paddle1;

      paddle2OldOld = paddle2Old;
      paddle2Old = paddle2;

      // update paddle input
      processKeyInput();

      // check if either paddles are touching screen limits/middle wall
      screenLimits(&paddle1);
      screenLimits(&paddle2);

      // update paddle positions
      updatePaddlePosition(&paddle1);
      updatePaddlePosition(&paddle2);

      /*Collisions*/
      // check for collision between ball &paddles
      p1Hit = collidedWPaddle(&ball, &paddle1);
      p2Hit = collidedWPaddle(&ball, &paddle2);

      // Ball wall collisions, forces ball back into limits
      //  Top wall collision
      if (ball.yc - BALL_RADIUS < 0) {
        ball.dyBall *= -1;
        ball.yc = (BALL_RADIUS + 1);
      }

      // Bottom wall collision
      if (ball.yc + BALL_RADIUS > 240) {
        ball.dyBall *= -1;
        ball.yc = 240 - (BALL_RADIUS + 1);
      }

      // Left wall collision
      if (ball.xc - BALL_RADIUS < 0) {
        ball.dxBall *= -1;
        ball.xc = (BALL_RADIUS + 1);
        // CHECK IF GOAL
        if (ball.yc < 160 && ball.yc > 160 - 80) {
          p2_score += 1;
          ballBounceSound(10);
          resetObjects(&ball, &paddle1, &paddle2);
        }
      }

      // Right wall collision
      if (ball.xc + BALL_RADIUS > 320) {
        ball.dxBall *= -1;
        ball.xc = 320 - (BALL_RADIUS + 1);
        // CHECK IF GOAL
        if (ball.yc < 160 && ball.yc > 160 - 80) {
          p1_score += 1;
          ballBounceSound(10);
          resetObjects(&ball, &paddle1, &paddle2);
        }
      }

      // only bounce if the ball was not just bounced in previous frame,
      // prevents sticking
      if (p1Hit && !previouslyHit) {
        bounceVector(&ball, &paddle1);
        ballBounceSound(40);
        previouslyHit = true;
      }

      else if (p2Hit && !previouslyHit) {
        bounceVector(&ball, &paddle2);
        ballBounceSound(40);
        previouslyHit = true;
      }

      else {
        previouslyHit = false;
      }

      /*Friction*/
      double friction = 3;
      double currentSpeed = currentBallSpeed(&ball);

      double newSpeed = currentSpeed - friction;
      if (newSpeed < 0) {
        newSpeed = 0;
      }

      if (currentSpeed > 0) {
        ball.dxBall = (ball.dxBall / currentSpeed) * newSpeed;
        ball.dyBall = (ball.dyBall / currentSpeed) * newSpeed;
      }

      /*Check if paused*/
      // if p key pressed once
      if (pPressed) {
        // poll until p key is pressed again
        while (pPressed) {
          processKeyInput();
          printf("pPressed: %d\n", pPressed);
        }
        // reset pause state to false so that it only enters pause state after
        // being pressed again
        pPressed = false;
        // dprintf("pPressed: %d\n", pPressed);
      }

      wait_for_vsync();  // swap front and back buffers on VGA vertical sync
      pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
    }  // end of game loop

    /*END SCREEN LOOP*/
    while (!spacePressed && !mPressed) {
      // draw end screen
      draw_end(winner);
      wait_for_vsync();  // swap front and back buffers on VGA vertical
                         // sync
      pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
      winningSound();
      processKeyInput();
      // reset everything (ball and paddle position, scores, timer)
      resetObjects(&ball, &paddle1, &paddle2);

      p1_score = 0;
      p2_score = 0;
      if (mPressed) {
        // to go back to the start menu
        start = true;
      } else if (spacePressed) {
        start = false;  // skips the start menu and starts the game again
      }
    }

    clear_screen();    // clear screen again
    wait_for_vsync();  // swap front and back buffers on VGA vertical sync
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
    clear_screen();

  }  // end of main fsm loop
}