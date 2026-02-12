// pong_game.h - Simple Pong Game
#ifndef PONG_GAME_H
#define PONG_GAME_H

#include <stdint.h>
#include "drivers/display.h"
#include "drivers/rotary_pcnt.h"

#define PADDLE_WIDTH 3
#define PADDLE_HEIGHT 20
#define BALL_SIZE 3
#define SCORE_LIMIT 5

typedef struct {
    int16_t x, y;
    int8_t dx, dy;
} Ball;

typedef struct {
    int16_t y;
    uint8_t score;
} Paddle;

typedef struct {
    Ball ball;
    Paddle left;
    Paddle right;
    uint8_t game_over;
    uint8_t winner;
} PongGame;

static PongGame pong;

static inline void pong_init(void) {
    // Ball starts in center
    pong.ball.x = WIDTH / 2;
    pong.ball.y = HEIGHT / 2;
    pong.ball.dx = 2;
    pong.ball.dy = 1;
    
    // Paddles
    pong.left.y = HEIGHT / 2 - PADDLE_HEIGHT / 2;
    pong.left.score = 0;
    pong.right.y = HEIGHT / 2 - PADDLE_HEIGHT / 2;
    pong.right.score = 0;
    
    pong.game_over = 0;
    pong.winner = 0;
}

static inline void pong_update_ball(void) {
    // Move ball
    pong.ball.x += pong.ball.dx;
    pong.ball.y += pong.ball.dy;
    
    // Top/bottom bounce
    if (pong.ball.y <= 0 || pong.ball.y >= HEIGHT - BALL_SIZE) {
        pong.ball.dy = -pong.ball.dy;
    }
    
    // Left paddle collision
    if (pong.ball.x <= 5 + PADDLE_WIDTH && 
        pong.ball.y + BALL_SIZE >= pong.left.y && 
        pong.ball.y <= pong.left.y + PADDLE_HEIGHT) {
        pong.ball.dx = -pong.ball.dx;
        pong.ball.x = 5 + PADDLE_WIDTH;
        
        // Add spin based on where ball hits paddle
        int16_t hit_pos = (pong.ball.y + BALL_SIZE/2) - (pong.left.y + PADDLE_HEIGHT/2);
        pong.ball.dy = hit_pos / 4;
    }
    
    // Right paddle collision
    if (pong.ball.x + BALL_SIZE >= WIDTH - 5 - PADDLE_WIDTH && 
        pong.ball.y + BALL_SIZE >= pong.right.y && 
        pong.ball.y <= pong.right.y + PADDLE_HEIGHT) {
        pong.ball.dx = -pong.ball.dx;
        pong.ball.x = WIDTH - 5 - PADDLE_WIDTH - BALL_SIZE;
        
        // Add spin
        int16_t hit_pos = (pong.ball.y + BALL_SIZE/2) - (pong.right.y + PADDLE_HEIGHT/2);
        pong.ball.dy = hit_pos / 4;
    }
    
    // Score (ball goes off sides)
    if (pong.ball.x < 0) {
        pong.right.score++;
        if (pong.right.score >= SCORE_LIMIT) {
            pong.game_over = 1;
            pong.winner = 2;
        } else {
            pong_init();
            pong.left.score = pong.left.score;
            pong.right.score = pong.right.score;
        }
    } else if (pong.ball.x > WIDTH) {
        pong.left.score++;
        if (pong.left.score >= SCORE_LIMIT) {
            pong.game_over = 1;
            pong.winner = 1;
        } else {
            pong_init();
            pong.left.score = pong.left.score;
            pong.right.score = pong.right.score;
        }
    }
}

static inline void pong_ai_update(void) {
    // Simple AI - follow ball
    int16_t paddle_center = pong.right.y + PADDLE_HEIGHT / 2;
    int16_t ball_center = pong.ball.y + BALL_SIZE / 2;
    
    if (ball_center < paddle_center - 2) {
        pong.right.y -= 2;
    } else if (ball_center > paddle_center + 2) {
        pong.right.y += 2;
    }
    
    // Keep paddle on screen
    if (pong.right.y < 0) pong.right.y = 0;
    if (pong.right.y > HEIGHT - PADDLE_HEIGHT) pong.right.y = HEIGHT - PADDLE_HEIGHT;
}

static inline void pong_draw(void) {
    display_clear();
    
    // Center line
    for (uint8_t i = 0; i < HEIGHT; i += 8) {
        draw_vline(WIDTH / 2, i, 4, 1);
    }
    
    // Left paddle
    fill_rect(5, pong.left.y, PADDLE_WIDTH, PADDLE_HEIGHT, 1);
    
    // Right paddle
    fill_rect(WIDTH - 5 - PADDLE_WIDTH, pong.right.y, PADDLE_WIDTH, PADDLE_HEIGHT, 1);
    
    // Ball
    fill_rect(pong.ball.x, pong.ball.y, BALL_SIZE, BALL_SIZE, 1);
    
    // Scores
    set_font(FONT_TOMTHUMB);
    set_cursor(WIDTH/2 - 20, 10);
    char score[8];
    snprintf(score, sizeof(score), "%d", pong.left.score);
    print(score);
    
    set_cursor(WIDTH/2 + 15, 10);
    snprintf(score, sizeof(score), "%d", pong.right.score);
    print(score);
    
    display_show();
}

static inline void pong_game_over_screen(void) {
    display_clear();
    set_font(FONT_TOMTHUMB);
    
    set_cursor(WIDTH/2 - 20, HEIGHT/2 - 10);
    println("GAME OVER");
    
    set_cursor(WIDTH/2 - 30, HEIGHT/2 + 5);
    if (pong.winner == 1) {
        println("YOU WIN!");
    } else {
        println("AI WINS!");
    }
    
    set_cursor(WIDTH/2 - 35, HEIGHT/2 + 20);
    println("Press to exit");
    
    display_show();
}

static inline void pong_play(RotaryPCNT *encoder) {
    pong_init();
    
    while (!pong.game_over) {
        // Player input
        int8_t dir = rotary_pcnt_read(encoder);
        if (dir > 0) {
            pong.left.y -= 3;
        } else if (dir < 0) {
            pong.left.y += 3;
        }
        
        // Keep paddle on screen
        if (pong.left.y < 0) pong.left.y = 0;
        if (pong.left.y > HEIGHT - PADDLE_HEIGHT) pong.left.y = HEIGHT - PADDLE_HEIGHT;
        
        // Exit on button
        if (rotary_pcnt_button_pressed(encoder)) {
            break;
        }
        
        // Update AI
        pong_ai_update();
        
        // Update ball
        pong_update_ball();
        
        // Draw
        pong_draw();
        
        vTaskDelay(pdMS_TO_TICKS(30)); // ~33 FPS
    }
    
    // Show game over if someone won
    if (pong.game_over) {
        pong_game_over_screen();
        
        while (!rotary_pcnt_button_pressed(encoder)) {
            rotary_pcnt_read(encoder);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

#endif
