#include <SDL2/SDL.h>
#include <vector>
#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int TILE_SIZE = 40;
const int PLAYER_SIZE = 30;
const float MOVE_SPEED = 8.0f; // Tiles per second
const float INPUT_BUFFER_TIME = 0.15f; // Seconds before accepting held input as continuous

// Dungeon is larger than screen
const int DUNGEON_WIDTH = 40;
const int DUNGEON_HEIGHT = 30;

enum GameState {
    MAIN_MENU,
    PLAYING,
    PAUSED,
    QUIT
};

enum TileType {
    WALL = 1,
    FLOOR = 0,
    CORRIDOR = 2
};

enum Direction {
    NONE = 0,
    UP = 1,
    DOWN = 2,
    LEFT = 4,
    RIGHT = 8,
    UP_LEFT = UP | LEFT,
    UP_RIGHT = UP | RIGHT,
    DOWN_LEFT = DOWN | LEFT,
    DOWN_RIGHT = DOWN | RIGHT
};

// Room structure
struct Room {
    int x, y;        // Top-left position in dungeon grid
    int width, height;
};

// Camera for scrolling
struct Camera {
    float x, y;
    int width, height;

    Camera() : x(0), y(0), width(SCREEN_WIDTH), height(SCREEN_HEIGHT) {}

    void followPlayer(float playerX, float playerY, int dungeonPixelWidth, int dungeonPixelHeight) {
        // Center camera on player
        x = playerX + PLAYER_SIZE / 2 - width / 2;
        y = playerY + PLAYER_SIZE / 2 - height / 2;

        // Clamp camera to dungeon bounds
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > dungeonPixelWidth - width) x = dungeonPixelWidth - width;
        if (y > dungeonPixelHeight - height) y = dungeonPixelHeight - height;
    }
};

struct Player {
    // Grid position (in tiles)
    int gridX, gridY;

    // Pixel position (for rendering during movement)
    float pixelX, pixelY;

    // Movement state
    bool isMoving;
    Direction movingDirection;
    float moveProgress; // 0.0 to 1.0

    // Target grid position (where we're moving to)
    int targetGridX, targetGridY;

    // Input timing for continuous movement
    Direction lastInputDirection;
    float inputHoldTime; // How long current input has been held
    bool continuousMoveEnabled; // Whether we've held long enough for continuous movement

    Player() : gridX(0), gridY(0), pixelX(0), pixelY(0),
               isMoving(false), movingDirection(NONE), moveProgress(0.0f),
               targetGridX(0), targetGridY(0), lastInputDirection(NONE),
               inputHoldTime(0.0f), continuousMoveEnabled(false) {}

    void setGridPosition(int x, int y) {
        gridX = x;
        gridY = y;
        targetGridX = x;
        targetGridY = y;
        pixelX = x * TILE_SIZE + (TILE_SIZE - PLAYER_SIZE) / 2;
        pixelY = y * TILE_SIZE + (TILE_SIZE - PLAYER_SIZE) / 2;
    }

    void startMove(Direction dir, int destGridX, int destGridY) {
        isMoving = true;
        movingDirection = dir;
        moveProgress = 0.0f;
        targetGridX = destGridX;
        targetGridY = destGridY;
    }

    void updateMovement(float deltaTime) {
        if (!isMoving) return;

        moveProgress += MOVE_SPEED * deltaTime;

        if (moveProgress >= 1.0f) {
            // Movement complete
            moveProgress = 1.0f;
            gridX = targetGridX;
            gridY = targetGridY;
            pixelX = gridX * TILE_SIZE + (TILE_SIZE - PLAYER_SIZE) / 2;
            pixelY = gridY * TILE_SIZE + (TILE_SIZE - PLAYER_SIZE) / 2;
            isMoving = false;
            movingDirection = NONE;
        } else {
            // Interpolate position
            float startPixelX = gridX * TILE_SIZE + (TILE_SIZE - PLAYER_SIZE) / 2;
            float startPixelY = gridY * TILE_SIZE + (TILE_SIZE - PLAYER_SIZE) / 2;
            float endPixelX = targetGridX * TILE_SIZE + (TILE_SIZE - PLAYER_SIZE) / 2;
            float endPixelY = targetGridY * TILE_SIZE + (TILE_SIZE - PLAYER_SIZE) / 2;

            // Smooth easing (optional - makes movement feel better)
            float t = moveProgress;
            // Ease out cubic for smoother stop
            t = 1 - pow(1 - t, 3);

            pixelX = startPixelX + (endPixelX - startPixelX) * t;
            pixelY = startPixelY + (endPixelY - startPixelY) * t;
        }
    }

    void updateInputTiming(Direction currentInput, float deltaTime) {
        if (currentInput == NONE) {
            // No input - reset everything
            lastInputDirection = NONE;
            inputHoldTime = 0.0f;
            continuousMoveEnabled = false;
        } else if (currentInput == lastInputDirection) {
            // Same direction held - accumulate time
            inputHoldTime += deltaTime;
            if (inputHoldTime >= INPUT_BUFFER_TIME) {
                continuousMoveEnabled = true;
            }
        } else {
            // Direction changed - reset timer
            lastInputDirection = currentInput;
            inputHoldTime = 0.0f;
            continuousMoveEnabled = false;
        }
    }

    bool shouldAcceptInput() const {
        // Accept input if:
        // 1. Not currently moving, OR
        // 2. Continuous movement is enabled (held long enough)
        return !isMoving || continuousMoveEnabled;
    }
};

struct Button {
    SDL_Rect rect;
    std::string text;
    bool hovered;
};

// Dungeon map
int dungeon[DUNGEON_HEIGHT][DUNGEON_WIDTH];
std::vector<Room> rooms;

// Generate a random room
Room generateRoom(int minSize, int maxSize) {
    Room room;
    room.width = minSize + rand() % (maxSize - minSize + 1);
    room.height = minSize + rand() % (maxSize - minSize + 1);
    return room;
}

// Check if room overlaps with existing rooms
bool roomOverlaps(const Room& newRoom, const std::vector<Room>& existingRooms, int padding = 2) {
    for (const auto& room : existingRooms) {
        if (newRoom.x < room.x + room.width + padding &&
            newRoom.x + newRoom.width + padding > room.x &&
            newRoom.y < room.y + room.height + padding &&
            newRoom.y + newRoom.height + padding > room.y) {
            return true;
        }
    }
    return false;
}

// Carve a room into the dungeon
void carveRoom(const Room& room) {
    for (int y = room.y; y < room.y + room.height; y++) {
        for (int x = room.x; x < room.x + room.width; x++) {
            if (y >= 0 && y < DUNGEON_HEIGHT && x >= 0 && x < DUNGEON_WIDTH) {
                dungeon[y][x] = FLOOR;
            }
        }
    }
}

// Create horizontal corridor
void carveHorizontalCorridor(int x1, int x2, int y) {
    int startX = std::min(x1, x2);
    int endX = std::max(x1, x2);
    for (int x = startX; x <= endX; x++) {
        if (x >= 0 && x < DUNGEON_WIDTH && y >= 0 && y < DUNGEON_HEIGHT) {
            if (dungeon[y][x] == WALL) {
                dungeon[y][x] = CORRIDOR;
            }
        }
    }
}

// Create vertical corridor
void carveVerticalCorridor(int y1, int y2, int x) {
    int startY = std::min(y1, y2);
    int endY = std::max(y1, y2);
    for (int y = startY; y <= endY; y++) {
        if (x >= 0 && x < DUNGEON_WIDTH && y >= 0 && y < DUNGEON_HEIGHT) {
            if (dungeon[y][x] == WALL) {
                dungeon[y][x] = CORRIDOR;
            }
        }
    }
}

// Generate dungeon with rooms and corridors
void generateDungeon() {
    // Initialize all as walls
    for (int y = 0; y < DUNGEON_HEIGHT; y++) {
        for (int x = 0; x < DUNGEON_WIDTH; x++) {
            dungeon[y][x] = WALL;
        }
    }

    rooms.clear();

    // Generate 8-12 rooms
    int numRooms = 8 + rand() % 5;
    int attempts = 0;
    int maxAttempts = 100;

    while (rooms.size() < numRooms && attempts < maxAttempts) {
        Room newRoom = generateRoom(4, 9);

        // Try to place room randomly
        newRoom.x = 1 + rand() % (DUNGEON_WIDTH - newRoom.width - 2);
        newRoom.y = 1 + rand() % (DUNGEON_HEIGHT - newRoom.height - 2);

        if (!roomOverlaps(newRoom, rooms)) {
            carveRoom(newRoom);

            // Connect to previous room with L-shaped corridor
            if (!rooms.empty()) {
                Room& prevRoom = rooms.back();
                int prevCenterX = prevRoom.x + prevRoom.width / 2;
                int prevCenterY = prevRoom.y + prevRoom.height / 2;
                int newCenterX = newRoom.x + newRoom.width / 2;
                int newCenterY = newRoom.y + newRoom.height / 2;

                // Random L-shape direction
                if (rand() % 2 == 0) {
                    carveHorizontalCorridor(prevCenterX, newCenterX, prevCenterY);
                    carveVerticalCorridor(prevCenterY, newCenterY, newCenterX);
                } else {
                    carveVerticalCorridor(prevCenterY, newCenterY, prevCenterX);
                    carveHorizontalCorridor(prevCenterX, newCenterX, newCenterY);
                }
            }

            rooms.push_back(newRoom);
        }
        attempts++;
    }
}

bool isWalkable(int gridX, int gridY) {
    if (gridX < 0 || gridX >= DUNGEON_WIDTH || gridY < 0 || gridY >= DUNGEON_HEIGHT)
        return false;
    return dungeon[gridY][gridX] != WALL;
}

// Get direction from keyboard state
Direction getDirectionFromInput(const Uint8* keyState) {
    int dirFlags = NONE;

    if (keyState[SDL_SCANCODE_UP] || keyState[SDL_SCANCODE_W]) {
        dirFlags |= UP;
    }
    if (keyState[SDL_SCANCODE_DOWN] || keyState[SDL_SCANCODE_S]) {
        dirFlags |= DOWN;
    }
    if (keyState[SDL_SCANCODE_LEFT] || keyState[SDL_SCANCODE_A]) {
        dirFlags |= LEFT;
    }
    if (keyState[SDL_SCANCODE_RIGHT] || keyState[SDL_SCANCODE_D]) {
        dirFlags |= RIGHT;
    }

    return static_cast<Direction>(dirFlags);
}

// Get target grid position from direction
void getTargetFromDirection(Direction dir, int currentX, int currentY, int& targetX, int& targetY) {
    targetX = currentX;
    targetY = currentY;

    if (dir & UP) targetY -= 1;
    if (dir & DOWN) targetY += 1;
    if (dir & LEFT) targetX -= 1;
    if (dir & RIGHT) targetX += 1;
}

void drawText(SDL_Renderer* renderer, const std::string& text, int x, int y, int size) {
    SDL_Rect textBg = {x - 5, y - 5, static_cast<int>(text.length() * size), size + 10};
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(renderer, &textBg);
}

void drawButton(SDL_Renderer* renderer, const Button& button) {
    if (button.hovered) {
        SDL_SetRenderDrawColor(renderer, 100, 150, 200, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 100, 140, 255);
    }
    SDL_RenderFillRect(renderer, &button.rect);

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &button.rect);

    drawText(renderer, button.text,
             button.rect.x + 20,
             button.rect.y + button.rect.h / 2 - 10,
             20);
}

bool isPointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

void renderMainMenu(SDL_Renderer* renderer, std::vector<Button>& buttons, int mouseX, int mouseY) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
    SDL_RenderClear(renderer);

    SDL_Rect titleBg = {SCREEN_WIDTH / 2 - 150, 100, 300, 80};
    SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
    SDL_RenderFillRect(renderer, &titleBg);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &titleBg);

    for (auto& button : buttons) {
        button.hovered = isPointInRect(mouseX, mouseY, button.rect);
        drawButton(renderer, button);
    }

    SDL_RenderPresent(renderer);
}

void renderPauseMenu(SDL_Renderer* renderer, std::vector<Button>& buttons, int mouseX, int mouseY) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);

    SDL_Rect menuBg = {SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2 - 200, 400, 400};
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_RenderFillRect(renderer, &menuBg);
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_RenderDrawRect(renderer, &menuBg);

    SDL_Rect titleBg = {SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 180, 200, 50};
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    SDL_RenderFillRect(renderer, &titleBg);

    for (auto& button : buttons) {
        button.hovered = isPointInRect(mouseX, mouseY, button.rect);
        drawButton(renderer, button);
    }

    SDL_RenderPresent(renderer);
}

void renderGame(SDL_Renderer* renderer, const Player& player, const Camera& camera) {
    SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
    SDL_RenderClear(renderer);

    // Draw dungeon tiles (only visible ones)
    int startCol = static_cast<int>(camera.x) / TILE_SIZE;
    int endCol = static_cast<int>(camera.x + camera.width) / TILE_SIZE + 1;
    int startRow = static_cast<int>(camera.y) / TILE_SIZE;
    int endRow = static_cast<int>(camera.y + camera.height) / TILE_SIZE + 1;

    // Clamp to dungeon bounds
    startCol = std::max(0, startCol);
    endCol = std::min(DUNGEON_WIDTH, endCol);
    startRow = std::max(0, startRow);
    endRow = std::min(DUNGEON_HEIGHT, endRow);

    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            SDL_Rect tile = {
                col * TILE_SIZE - static_cast<int>(camera.x),
                row * TILE_SIZE - static_cast<int>(camera.y),
                TILE_SIZE,
                TILE_SIZE
            };

            if (dungeon[row][col] == WALL) {
                SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
                SDL_RenderFillRect(renderer, &tile);
            } else if (dungeon[row][col] == FLOOR) {
                SDL_SetRenderDrawColor(renderer, 30, 35, 40, 255);
                SDL_RenderFillRect(renderer, &tile);
                // Room floor border
                SDL_SetRenderDrawColor(renderer, 45, 50, 55, 255);
                SDL_RenderDrawRect(renderer, &tile);
            } else if (dungeon[row][col] == CORRIDOR) {
                SDL_SetRenderDrawColor(renderer, 35, 40, 45, 255);
                SDL_RenderFillRect(renderer, &tile);
            }
        }
    }

    // Draw player using pixel position (smooth movement)
    SDL_Rect playerRect = {
        static_cast<int>(player.pixelX - camera.x),
        static_cast<int>(player.pixelY - camera.y),
        PLAYER_SIZE,
        PLAYER_SIZE
    };
    SDL_SetRenderDrawColor(renderer, 255, 200, 50, 255);
    SDL_RenderFillRect(renderer, &playerRect);

    // Player outline
    SDL_SetRenderDrawColor(renderer, 255, 230, 100, 255);
    SDL_RenderDrawRect(renderer, &playerRect);

    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    srand(static_cast<unsigned>(time(nullptr)));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Mystery Dungeon Style Game",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH,
                                          SCREEN_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        std::cout << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cout << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Initialize camera
    Camera camera;

    // Initialize player
    Player player;

    std::vector<Button> mainMenuButtons = {
        {{SCREEN_WIDTH / 2 - 100, 250, 200, 50}, "Start Dungeon", false},
        {{SCREEN_WIDTH / 2 - 100, 320, 200, 50}, "Options", false},
        {{SCREEN_WIDTH / 2 - 100, 390, 200, 50}, "Quit", false}
    };

    std::vector<Button> pauseMenuButtons = {
        {{SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 80, 200, 50}, "Resume", false},
        {{SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 10, 200, 50}, "New Dungeon", false},
        {{SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 60, 200, 50}, "Main Menu", false}
    };

    GameState gameState = MAIN_MENU;
    bool running = true;
    SDL_Event event;

    Uint32 lastTime = SDL_GetTicks();
    int mouseX = 0, mouseY = 0;

    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_MOUSEMOTION) {
                mouseX = event.motion.x;
                mouseY = event.motion.y;
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (gameState == MAIN_MENU) {
                    for (size_t i = 0; i < mainMenuButtons.size(); i++) {
                        if (mainMenuButtons[i].hovered) {
                            if (i == 0) { // Start Dungeon
                                generateDungeon();
                                // Spawn player in first room center
                                if (!rooms.empty()) {
                                    int spawnX = rooms[0].x + rooms[0].width / 2;
                                    int spawnY = rooms[0].y + rooms[0].height / 2;
                                    player.setGridPosition(spawnX, spawnY);
                                }
                                gameState = PLAYING;
                            } else if (i == 1) { // Options
                                std::cout << "Options clicked" << std::endl;
                            } else if (i == 2) { // Quit
                                running = false;
                            }
                        }
                    }
                } else if (gameState == PAUSED) {
                    for (size_t i = 0; i < pauseMenuButtons.size(); i++) {
                        if (pauseMenuButtons[i].hovered) {
                            if (i == 0) { // Resume
                                gameState = PLAYING;
                            } else if (i == 1) { // New Dungeon
                                generateDungeon();
                                if (!rooms.empty()) {
                                    int spawnX = rooms[0].x + rooms[0].width / 2;
                                    int spawnY = rooms[0].y + rooms[0].height / 2;
                                    player.setGridPosition(spawnX, spawnY);
                                }
                                gameState = PLAYING;
                            } else if (i == 2) { // Main Menu
                                gameState = MAIN_MENU;
                            }
                        }
                    }
                }
            }

            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    if (gameState == PLAYING) {
                        gameState = PAUSED;
                    } else if (gameState == PAUSED) {
                        gameState = PLAYING;
                    }
                }
            }
        }

        if (gameState == PLAYING) {
            std::print("Playing\n");
            // Update player movement animation
            player.updateMovement(deltaTime);

            // Get current input direction
            const Uint8* keyState = SDL_GetKeyboardState(NULL);
            Direction currentInput = getDirectionFromInput(keyState);

            // Update input timing to track hold duration
            player.updateInputTiming(currentInput, deltaTime);

            // Only try to move if player should accept input
            if (currentInput != NONE && player.shouldAcceptInput()) {
                int targetX, targetY;
                getTargetFromDirection(currentInput, player.gridX, player.gridY, targetX, targetY);

                // Check if target tile is walkable
                if (isWalkable(targetX, targetY)) {
                    player.startMove(currentInput, targetX, targetY);
                }
            }

            // Update camera to follow player (using pixel position for smooth follow)
            camera.followPlayer(player.pixelX, player.pixelY,
                              DUNGEON_WIDTH * TILE_SIZE,
                              DUNGEON_HEIGHT * TILE_SIZE);
        }

        if (gameState == MAIN_MENU) {
            renderMainMenu(renderer, mainMenuButtons, mouseX, mouseY);
        } else if (gameState == PLAYING) {
            renderGame(renderer, player, camera);
        } else if (gameState == PAUSED) {
            renderGame(renderer, player, camera);
            renderPauseMenu(renderer, pauseMenuButtons, mouseX, mouseY);
        }

        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}