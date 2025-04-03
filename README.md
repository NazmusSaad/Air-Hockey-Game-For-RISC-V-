# Nios V Air Hockey Game 🎮

A two-player air hockey game implemented on the Nios V processor. The game uses a VGA display, PS/2 keyboard for paddle movement, HEX displays to show the score, and sound effects.

## Features
- Two-player game (WASD for Player 1, Arrow Keys for Player 2)
- VGA output with animated paddles and puck
- Score display using HEX displays
- Pause/unpause feature with 'P'
- End screen with victory announcement
- Sound effects on paddle bounce, goal, and win

## How to Play
- Press **Space** to start the game
- Player 1 (Left): **WASD** to move
- Player 2 (Right): **Arrow Keys** to move
- Press **P** to pause/unpause
- First to **2 goals** wins
- Press **Space** to restart or **M** to return to the main menu

## Build Instructions
1. Ensure you have the RISC-V toolchain installed.
2. Use the included Makefile to compile the project.
3. Program and run on a DE1-SoC board with VGA and PS/2 keyboard connected.
