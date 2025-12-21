#ifndef PARSE_H
#define PARSE_H

#include <unistd.h>
#include "game.h"
#include "board.h"

#define CONTINUE_PLAY 0

/**
 * @brief Reads the entire content of a file descriptor into a dynamically allocated buffer.
 * @param fd The file descriptor to read from.
 * @param byte_count Pointer to store the number of bytes read.
 * @return A pointer to the buffer containing the file content, or NULL on failure.
 */
char* readFile(int fd, ssize_t* byte_count);

/**
 * @brief Splits a buffer into lines and returns an array of pointers to each line.
 * @param buffer The buffer containing the file content.
 * @param byte_count The number of bytes in the buffer.
 * @param line_count Pointer to store the number of lines found.
 * @return An array of pointers to each line in the buffer.
 */
char** getBufferLines(char* buffer, ssize_t byte_count, int* line_count);

/**
 * @brief Parses a monster configuration file and returns a pointer to a ghost_t structure.
 * @param filename The name of the monster configuration file.
 * @param dirpath The directory path where the file is located.
 * @return A pointer to the parsed ghost_t structure, or NULL on failure.
 */
ghost_t* parseMonster(char* filename, char* dirpath);

/**
 * @brief Parses a pacman configuration file and returns a pointer to a pacman_t structure.
 * @param filename The name of the pacman configuration file.
 * @param dirpath The directory path where the file is located.
 * @return A pointer to the parsed pacman_t structure, or NULL on failure.
 */
pacman_t* parsePacman(char* filename, char* dirpath);

/**
 * @brief Parses a level configuration file and returns a pointer to a board_t structure.
 * @param filename The name of the level configuration file.
 * @param dirpath The directory path where the file is located.
 * @return A pointer to the parsed board_t structure, or NULL on failure.
 */
board_t* parseLvl(char* filename, char* dirpath);

/**
 * @brief Handles pacman if is the user input for controlling Pacman.
 * @param board Pointer to the current level's board.
 */
void userPacman(board_t* board);

/**
 * @brief Reads all level files from a specified directory and returns an array of board_t pointers.
 * @param dirpath The directory path containing the level files.
 * @return An array of pointers to board_t structures representing the levels.
 */
board_t** handle_files(char* dirpath);

#endif