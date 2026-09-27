#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Platform detection
#ifdef _WIN32
#include <windows.h>
#define IS_WINDOWS 1
#else
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#define IS_WINDOWS 0
#endif

// Constants
#define MAX_HISTORY 10
#define MAX_INPUT_SIZE 1024
#define MAX_OUTPUT_SIZE 4096
#define MORSE_TABLE_SIZE 54

// Morse code table structure
typedef struct {
    char character;
    const char *morse;
} MorseEntry;

// Extended morse code table including symbols
const MorseEntry morseTable[MORSE_TABLE_SIZE] = {
    // Letters A-Z
    {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."},
    {'F', "..-."}, {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"},
    {'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."}, {'O', "---"},
    {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
    {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"},
    {'Z', "--.."},
    // Numbers 0-9
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."},
    // Punctuation and symbols
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."},
    {'!', "-.-.--"}, {'/', "-..-."}, {'(', "-.--."}, {')', "-.--.-"},
    {'&', ".-..."}, {':', "---..."}, {';', "-.-.-."}, {'=', "-...-"},
    {'+', ".-.-."}, {'-', "-....-"}, {'_', "..--.-"}, {'"', ".-..-."},
    {'$', "...-..-"}, {'@', ".--.-."}
};

// Translation history structure
typedef struct {
    char input[MAX_INPUT_SIZE];
    char output[MAX_OUTPUT_SIZE];
    int type;
} TranslationRecord;

// Global variables
TranslationRecord history[MAX_HISTORY];
int historyCount = 0;
int historyIndex = 0;
int soundEnabled = 1;
int lightsEnabled = 1;
int translationSpeed = 2;
int isFirstRun = 1;

// Speed configurations
int dotDurations[3] = {300, 200, 100};
int dashDurations[3] = {900, 600, 300};
int shortDelays[3] = {300, 200, 100};
int mediumDelays[3] = {600, 400, 200};
int longDelays[3] = {1200, 800, 400};

// ==================== Platform Functions ====================

void crossSleep(int ms) {
    if (ms < 0 || ms > 10000) return;
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void clearScreen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void crossBeep(int duration) {
    if (duration < 0 || duration > 5000) return;
#ifdef _WIN32
    Beep(800, duration);
#else
    printf("\a");
    fflush(stdout);
    crossSleep(duration);
#endif
}

void copyToClipboard(const char *text) {
    if (text == NULL || strlen(text) == 0) {
        printf("\n[X] Nothing to copy.\n");
        return;
    }

#ifdef _WIN32
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        size_t len = strlen(text) + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            char *dst = (char *)GlobalLock(hMem);
            if (dst) {
                memcpy(dst, text, len);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
                CloseClipboard();
                printf("\n[OK] Copied to clipboard!\n");
                return;
            }
        }
        CloseClipboard();
    }
    printf("\n[X] Failed to copy to clipboard.\n");
#else
    FILE *pipe = popen("xclip -selection clipboard 2>/dev/null || pbcopy 2>/dev/null", "w");
    if (pipe) {
        fprintf(pipe, "%s", text);
        int result = pclose(pipe);
        if (result == 0) {
            printf("\n[OK] Copied to clipboard!\n");
        } else {
            printf("\n[X] Clipboard tool not found. Install 'xclip' (Linux) or use macOS.\n");
        }
    } else {
        printf("\n[X] Failed to copy to clipboard.\n");
    }
#endif
}

// ==================== Lighting System ====================

#ifdef _WIN32
void setConsoleBackgroundColor(int isWhite) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD saved_attributes;

    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    saved_attributes = consoleInfo.wAttributes;

    if (isWhite) {
        // White background, black text
        SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);
    } else {
        // Black background, white text
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    // Clear and redraw
    system("cls");
}
#else
void setConsoleBackgroundColor(int isWhite) {
    if (isWhite) {
        // White background, black text
        printf("\033[47m\033[30m");
    } else {
        // Black background, white text
        printf("\033[40m\033[37m");
    }
    // Clear screen
    printf("\033[2J\033[H");
    fflush(stdout);
}
#endif

void openLightWindow(void) {
    // Nothing needed - using same console
}

void setLightColor(int isWhite) {
    setConsoleBackgroundColor(isWhite);
}

void closeLightWindow(void) {
    // Reset to normal colors
    setConsoleBackgroundColor(0);
}

const char* getMorseForChar(char c) {
    c = toupper(c);
    for (int i = 0; i < MORSE_TABLE_SIZE; i++) {
        if (morseTable[i].character == c) {
            return morseTable[i].morse;
        }
    }
    return NULL;
}

char getCharForMorse(const char *morse) {
    for (int i = 0; i < MORSE_TABLE_SIZE; i++) {
        if (strcmp(morseTable[i].morse, morse) == 0) {
            return morseTable[i].character;
        }
    }
    return '\0';
}

void clearInputBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void safeCopy(char *dest, const char *src, int maxSize) {
    if (dest == NULL || src == NULL || maxSize <= 0) return;
    strncpy(dest, src, maxSize - 1);
    dest[maxSize - 1] = '\0';
}

int getDotDuration(void) {
    if (translationSpeed < 1 || translationSpeed > 3) translationSpeed = 2;
    return dotDurations[translationSpeed - 1];
}

int getDashDuration(void) {
    if (translationSpeed < 1 || translationSpeed > 3) translationSpeed = 2;
    return dashDurations[translationSpeed - 1];
}

int getShortDelay(void) {
    if (translationSpeed < 1 || translationSpeed > 3) translationSpeed = 2;
    return shortDelays[translationSpeed - 1];
}

int getMediumDelay(void) {
    if (translationSpeed < 1 || translationSpeed > 3) translationSpeed = 2;
    return mediumDelays[translationSpeed - 1];
}

int getLongDelay(void) {
    if (translationSpeed < 1 || translationSpeed > 3) translationSpeed = 2;
    return longDelays[translationSpeed - 1];
}

int isValidInput(const char *str) {
    if (str == NULL) return 0;
    int len = strlen(str);
    if (len == 0 || len >= MAX_INPUT_SIZE) return 0;
    return 1;
}

// ==================== Morse Display Helper ====================

void playMorseWithSound(const char *morse) {
    if (morse == NULL || strlen(morse) == 0) return;

    for (int i = 0; morse[i] != '\0'; i++) {
        if (morse[i] == '.') {
            printf(".");
            fflush(stdout);

            if (lightsEnabled) {
                setLightColor(1); // White
            }
            if (soundEnabled) {
                crossBeep(getDotDuration());
            }
            if (!soundEnabled) {
                crossSleep(getDotDuration());
            }
            if (lightsEnabled) {
                setLightColor(0); // Black
            }
            crossSleep(getShortDelay());
        }
        else if (morse[i] == '-') {
            printf("-");
            fflush(stdout);

            if (lightsEnabled) {
                setLightColor(1); // White
            }
            if (soundEnabled) {
                crossBeep(getDashDuration());
            }
            if (!soundEnabled) {
                crossSleep(getDashDuration());
            }
            if (lightsEnabled) {
                setLightColor(0); // Black
            }
            crossSleep(getShortDelay());
        }
        else if (morse[i] == '|') {
            printf(" | ");
            fflush(stdout);
            crossSleep(getMediumDelay());
        }
        else if (morse[i] == ' ') {
            printf(" ");
            fflush(stdout);
        }
    }
}

// ==================== Translation Functions ====================

void englishToMorse(const char *input, char *output) {
    if (!isValidInput(input) || output == NULL) {
        if (output) output[0] = '\0';
        return;
    }

    int outputPos = 0;
    output[0] = '\0';

    clearScreen();
    printf("\n=== ENGLISH TO MORSE CODE ===\n");
    printf("Input: %s\n\n", input);
    printf("Translation:\n\n");

    // Open light window if enabled
    if (lightsEnabled) {
        openLightWindow();
    }

    char currentWord[256] = {0};
    char wordMorseDisplay[1024] = {0};
    int wordPos = 0;
    int wordMorseDisplayPos = 0;
    int inWord = 0;

    for (int i = 0; input[i] != '\0' && outputPos < MAX_OUTPUT_SIZE - 10; i++) {
        char c = input[i];

        if (c == ' ' || c == '\n' || c == '\t') {
            if (inWord) {
                currentWord[wordPos] = '\0';
                wordMorseDisplay[wordMorseDisplayPos] = '\0';

                printf("[%s] = ", currentWord);
                fflush(stdout);
                playMorseWithSound(wordMorseDisplay);
                printf("\n");
                fflush(stdout);

                if (outputPos < MAX_OUTPUT_SIZE - 3) {
                    output[outputPos++] = '/';
                    output[outputPos++] = ' ';
                }

                inWord = 0;
                wordPos = 0;
                wordMorseDisplayPos = 0;
            }
            continue;
        }

        const char *morse = getMorseForChar(c);
        if (morse != NULL) {
            if (!inWord) {
                inWord = 1;
                wordPos = 0;
                wordMorseDisplayPos = 0;
            }

            if (wordPos < 255) {
                currentWord[wordPos++] = toupper(c);
            }

            // Add separator for display
            if (wordMorseDisplayPos > 0) {
                wordMorseDisplay[wordMorseDisplayPos++] = '|';
                wordMorseDisplay[wordMorseDisplayPos++] = ' ';
            }

            // Add morse to display
            int morseLen = strlen(morse);
            for (int j = 0; j < morseLen && wordMorseDisplayPos < 1020; j++) {
                wordMorseDisplay[wordMorseDisplayPos++] = morse[j];
                wordMorseDisplay[wordMorseDisplayPos++] = ' ';
            }

            // Add to output (no spaces between dots/dashes)
            for (int j = 0; j < morseLen && outputPos < MAX_OUTPUT_SIZE - 5; j++) {
                output[outputPos++] = morse[j];
            }
            output[outputPos++] = ' ';
        }
    }

    // Handle last word
    if (inWord) {
        currentWord[wordPos] = '\0';
        wordMorseDisplay[wordMorseDisplayPos] = '\0';

        printf("[%s] = ", currentWord);
        fflush(stdout);
        playMorseWithSound(wordMorseDisplay);
        printf("\n");
        fflush(stdout);
    }

    // Close light window
    if (lightsEnabled) {
        closeLightWindow();
    }

    output[outputPos] = '\0';
}

void morseToEnglish(const char *input, char *output) {
    if (!isValidInput(input) || output == NULL) {
        if (output) output[0] = '\0';
        return;
    }

    int outputPos = 0;
    output[0] = '\0';
    int i = 0;

    printf("\nTranslating: ");

    while (input[i] != '\0' && outputPos < MAX_OUTPUT_SIZE - 2) {
        // Skip whitespace
        while (input[i] == ' ' || input[i] == '\t' || input[i] == '\n') {
            i++;
        }

        if (input[i] == '\0') break;

        // Check for word separator
        if (input[i] == '/') {
            output[outputPos++] = ' ';
            printf(" [SPACE] ");
            fflush(stdout);
            i++;
            continue;
        }

        // Check if it's a morse code character (dot or dash)
        if (input[i] == '.' || input[i] == '-') {
            char morseCode[50] = {0};
            int morsePos = 0;

            // Extract morse code sequence
            while (input[i] != '\0' && (input[i] == '.' || input[i] == '-') && morsePos < 49) {
                morseCode[morsePos++] = input[i];
                i++;
            }
            morseCode[morsePos] = '\0';

            if (morsePos > 0) {
                char decoded = getCharForMorse(morseCode);
                if (decoded != '\0') {
                    output[outputPos++] = decoded;
                    printf("[%c]", decoded);
                } else {
                    output[outputPos++] = '?';
                    printf("[?]");
                }
                fflush(stdout);
            }
        } else {
            // Regular character (not morse code)
            output[outputPos++] = input[i];
            printf("[%c]", input[i]);
            fflush(stdout);
            i++;
        }
    }

    output[outputPos] = '\0';
    printf("\n");
}

// ==================== Display Functions ====================

void displayMorseWithBeeps(const char *morse) {
    if (morse == NULL || strlen(morse) == 0) return;

    // Open light window for replay
    if (lightsEnabled) {
        openLightWindow();
    }

    for (int i = 0; morse[i] != '\0'; i++) {
        if (morse[i] == '.') {
            printf(". ");
            fflush(stdout);

            if (lightsEnabled) {
                setLightColor(1); // White
            }
            if (soundEnabled) {
                crossBeep(getDotDuration());
            }
            if (!soundEnabled) {
                crossSleep(getDotDuration());
            }
            if (lightsEnabled) {
                setLightColor(0); // Black
            }
            crossSleep(getShortDelay());
        }
        else if (morse[i] == '-') {
            printf("- ");
            fflush(stdout);

            if (lightsEnabled) {
                setLightColor(1); // White
            }
            if (soundEnabled) {
                crossBeep(getDashDuration());
            }
            if (!soundEnabled) {
                crossSleep(getDashDuration());
            }
            if (lightsEnabled) {
                setLightColor(0); // Black
            }
            crossSleep(getShortDelay());
        }
        else if (morse[i] == '/') {
            printf("/ ");
            crossSleep(getLongDelay());
        }
        else if (morse[i] == ' ') {
            if (i > 0 && morse[i-1] == ' ') {
                crossSleep(getMediumDelay());
            }
        }
        else {
            printf("%c", morse[i]);
        }
    }
    printf("\n");

    // Close light window after replay
    if (lightsEnabled) {
        closeLightWindow();
    }
}

// ==================== History Functions ====================

void addToHistory(const char *input, const char *output, int type) {
    if (!isValidInput(input) || !isValidInput(output)) return;

    int idx = historyIndex % MAX_HISTORY;

    safeCopy(history[idx].input, input, MAX_INPUT_SIZE);
    safeCopy(history[idx].output, output, MAX_OUTPUT_SIZE);
    history[idx].type = type;

    historyIndex++;
    if (historyCount < MAX_HISTORY) {
        historyCount++;
    }
}

void viewAndReplayHistory(void) {
    clearScreen();
    printf("\n=== Translation History ===\n");
    printf("(Last %d translations)\n\n", historyCount);

    if (historyCount == 0) {
        printf("No translations in history yet.\n");
        printf("\nPress Enter to continue...");
        clearInputBuffer();
        return;
    }

    int startIdx = historyIndex - historyCount;
    if (startIdx < 0) startIdx = 0;

    for (int i = 0; i < historyCount; i++) {
        int idx = (startIdx + i) % MAX_HISTORY;

        printf("%d. ", i + 1);
        if (history[idx].type == 1) {
            printf("[English -> Morse] ");
        } else {
            printf("[Morse -> English] ");
        }

        if (strlen(history[idx].input) > 40) {
            printf("%.40s...\n", history[idx].input);
        } else {
            printf("%s\n", history[idx].input);
        }
    }

    printf("\n0. Back to Main Menu\n");
    printf("\nEnter number to view/replay (0-%d): ", historyCount);

    int choice;
    if (scanf("%d", &choice) != 1) {
        clearInputBuffer();
        printf("Invalid input.\n");
        crossSleep(1500);
        return;
    }
    clearInputBuffer();

    if (choice == 0) {
        return;
    }

    if (choice < 1 || choice > historyCount) {
        printf("Invalid choice.\n");
        crossSleep(1500);
        return;
    }

    int idx = (startIdx + choice - 1) % MAX_HISTORY;

    clearScreen();
    printf("\n=== Translation #%d ===\n", choice);
    printf("Type: %s\n\n",
           history[idx].type == 1 ? "English to Morse" : "Morse to English");

    printf("Input:\n%s\n", history[idx].input);
    printf("\nOutput:\n%s\n", history[idx].output);

    if (history[idx].type == 1) {
        printf("\n--- Options ---\n");
        printf("1. Replay with sound\n");
        printf("2. Copy output to clipboard\n");
        printf("3. Back to history\n");
        printf("Enter choice (1-3): ");

        int replayChoice;
        if (scanf("%d", &replayChoice) == 1) {
            clearInputBuffer();
            if (replayChoice == 1) {
                printf("\nReplaying...\n");
                displayMorseWithBeeps(history[idx].output);
                printf("\n[OK] Replay completed!\n");
            } else if (replayChoice == 2) {
                copyToClipboard(history[idx].output);
            }
        } else {
            clearInputBuffer();
        }
    } else {
        printf("\n--- Options ---\n");
        printf("1. Copy output to clipboard\n");
        printf("2. Back to history\n");
        printf("Enter choice (1-2): ");

        int replayChoice;
        if (scanf("%d", &replayChoice) == 1 && replayChoice == 1) {
            clearInputBuffer();
            copyToClipboard(history[idx].output);
        } else {
            clearInputBuffer();
        }
    }

    printf("\nPress Enter to continue...");
    clearInputBuffer();
}

// ==================== Setup Wizard ====================

int setupWizard(void) {
    int tempSound = 1;
    int tempLights = 1;
    int tempSpeed = 2;
    int step = 1;
    int choice;

    while (1) {
        clearScreen();

        if (step == 1) {
            // Welcome screen
            printf("\n");
            printf("================================\n");
            printf("   MORSE CODE TRANSLATOR\n");
            printf("================================\n\n");
            printf("Welcome! Let's get you set up first.\n\n");
            printf("Press Enter to continue...");
            getchar();
            step = 2;
        }
        else if (step == 2) {
            // Sound setup
            clearScreen();
            printf("\n=== Setup: Sound ===\n\n");
            printf("Do you want to enable sound effects?\n");
            printf("(Sound plays beeps for dots and dashes)\n\n");
            printf("1. Yes\n");
            printf("2. No\n");
            printf("\nEnter choice (1-2): ");

            if (scanf("%d", &choice) == 1) {
                clearInputBuffer();
                if (choice == 1) {
                    tempSound = 1;
                    step = 3;
                } else if (choice == 2) {
                    tempSound = 0;
                    step = 3;
                } else {
                    printf("\n[X] Invalid choice.\n");
                    crossSleep(1000);
                }
            } else {
                clearInputBuffer();
                printf("\n[X] Invalid input.\n");
                crossSleep(1000);
            }
        }
        else if (step == 3) {
            // Lights setup
            clearScreen();
            printf("\n=== Setup: Visual Effects ===\n\n");
            printf("Do you want to enable visual lights?\n");
            printf("(Shows visual indicators during morse playback)\n\n");
            printf("1. Yes\n");
            printf("2. No\n");
            printf("3. Go back\n");
            printf("\nEnter choice (1-3): ");

            if (scanf("%d", &choice) == 1) {
                clearInputBuffer();
                if (choice == 1) {
                    tempLights = 1;
                    step = 4;
                } else if (choice == 2) {
                    tempLights = 0;
                    step = 4;
                } else if (choice == 3) {
                    step = 2;
                } else {
                    printf("\n[X] Invalid choice.\n");
                    crossSleep(1000);
                }
            } else {
                clearInputBuffer();
                printf("\n[X] Invalid input.\n");
                crossSleep(1000);
            }
        }
        else if (step == 4) {
            // Speed setup
            clearScreen();
            printf("\n=== Setup: Translation Speed ===\n\n");
            printf("Select translation speed:\n\n");
            printf("1. Slow\n");
            printf("2. Medium\n");
            printf("3. Fast\n");
            printf("4. Go back\n");
            printf("\nEnter choice (1-4): ");

            if (scanf("%d", &choice) == 1) {
                clearInputBuffer();
                if (choice >= 1 && choice <= 3) {
                    tempSpeed = choice;
                    step = 5;
                } else if (choice == 4) {
                    step = 3;
                } else {
                    printf("\n[X] Invalid choice.\n");
                    crossSleep(1000);
                }
            } else {
                clearInputBuffer();
                printf("\n[X] Invalid input.\n");
                crossSleep(1000);
            }
        }
        else if (step == 5) {
            // Confirmation
            clearScreen();
            printf("\n=== Setup Summary ===\n\n");
            printf("Your settings:\n");
            printf("  - Sound: %s\n", tempSound ? "ON" : "OFF");
            printf("  - Visual Lights: %s\n", tempLights ? "ON" : "OFF");
            printf("  - Speed: ");
            if (tempSpeed == 1) printf("SLOW\n");
            else if (tempSpeed == 2) printf("MEDIUM\n");
            else printf("FAST\n");
            printf("\n1. Continue\n");
            printf("2. Go back to change settings\n");
            printf("\nEnter choice (1-2): ");

            if (scanf("%d", &choice) == 1) {
                clearInputBuffer();
                if (choice == 1) {
                    // Save settings
                    soundEnabled = tempSound;
                    lightsEnabled = tempLights;
                    translationSpeed = tempSpeed;
                    isFirstRun = 0;

                    printf("\n[OK] Setup completed!\n");
                    crossSleep(1500);
                    return 1;
                } else if (choice == 2) {
                    step = 2;
                } else {
                    printf("\n[X] Invalid choice.\n");
                    crossSleep(1000);
                }
            } else {
                clearInputBuffer();
                printf("\n[X] Invalid input.\n");
                crossSleep(1000);
            }
        }
    }

    return 0;
}

// ==================== Settings Functions ====================

void settingsMenu(void) {
    int choice;

    while (1) {
        clearScreen();
        printf("\n=== Settings ===\n\n");

        printf("1. Sound: %s\n", soundEnabled ? "ON" : "OFF");
        printf("2. Visual Lights: %s\n", lightsEnabled ? "ON" : "OFF");
        printf("3. Translation Speed: ");
        if (translationSpeed == 1) {
            printf("SLOW\n");
        } else if (translationSpeed == 2) {
            printf("MEDIUM\n");
        } else {
            printf("FAST\n");
        }
        printf("4. Back to Main Menu\n");

        printf("\nEnter your choice (1-4): ");

        if (scanf("%d", &choice) != 1) {
            clearInputBuffer();
            printf("Invalid input.\n");
            crossSleep(1500);
            continue;
        }
        clearInputBuffer();

        if (choice == 1) {
            soundEnabled = !soundEnabled;
            printf("\n[OK] Sound is now: %s\n", soundEnabled ? "ON" : "OFF");
            crossSleep(1000);
        }
        else if (choice == 2) {
            lightsEnabled = !lightsEnabled;
            printf("\n[OK] Visual Lights are now: %s\n", lightsEnabled ? "ON" : "OFF");
            crossSleep(1000);
        }
        else if (choice == 3) {
            printf("\nSelect Speed:\n");
            printf("1. Slow\n");
            printf("2. Medium\n");
            printf("3. Fast\n");
            printf("Enter choice (1-3): ");

            int speedChoice;
            if (scanf("%d", &speedChoice) == 1 &&
                speedChoice >= 1 && speedChoice <= 3) {
                translationSpeed = speedChoice;
                printf("\n[OK] Speed set to: ");
                if (speedChoice == 1) printf("SLOW\n");
                else if (speedChoice == 2) printf("MEDIUM\n");
                else printf("FAST\n");
            } else {
                printf("\n[X] Invalid choice. Speed unchanged.\n");
            }
            clearInputBuffer();
            crossSleep(1000);
        }
        else if (choice == 4) {
            return;
        }
        else {
            printf("\n[X] Invalid choice. Please try again.\n");
            crossSleep(1500);
        }
    }
}

// ==================== Main Menu ====================

int main(void) {
    int choice;
    char input[MAX_INPUT_SIZE];
    char output[MAX_OUTPUT_SIZE];

#if !IS_WINDOWS
    printf("Note: On Linux/macOS, audio uses terminal bell.\n");
    printf("For clipboard: Install 'xclip' on Linux or use macOS.\n");
    printf("Press Enter to continue...");
    getchar();
#endif

    // Run setup wizard on first run
    if (isFirstRun) {
        setupWizard();
    }

    while (1) {
        clearScreen();
        printf("\n");
        printf("================================\n");
        printf("   MORSE CODE TRANSLATOR\n");
        printf("================================\n\n");
        printf("1. English to Morse Code\n");
        printf("2. Morse Code to English\n");
        printf("3. History (View & Replay)\n");
        printf("4. Settings\n");
        printf("5. Exit\n");
        printf("\nEnter your choice (1-5): ");

        if (scanf("%d", &choice) != 1) {
            clearInputBuffer();
            printf("\n[X] Invalid input. Please enter a number 1-5.\n");
            crossSleep(1500);
            continue;
        }
        clearInputBuffer();

        if (choice == 1) {
            clearScreen();
            printf("\n=== ENGLISH TO MORSE CODE ===\n");
            printf("Enter text: ");

            if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
                printf("\n[X] Error reading input.\n");
                crossSleep(1500);
                continue;
            }
            input[strcspn(input, "\n")] = '\0';

            if (!isValidInput(input)) {
                printf("\n[X] Invalid input. Please try again.\n");
                crossSleep(1500);
                continue;
            }

            englishToMorse(input, output);

            printf("\n[OK] Translation completed!\n");
            printf("\nFull Output:\n%s\n", output);

            printf("\nCopy to clipboard? (1=Yes, 0=No): ");
            int copyChoice;
            if (scanf("%d", &copyChoice) == 1 && copyChoice == 1) {
                copyToClipboard(output);
            } else if (copyChoice != 0) {
                printf("[X] Invalid choice.\n");
            }
            clearInputBuffer();

            addToHistory(input, output, 1);
        }
        else if (choice == 2) {
            clearScreen();
            printf("\n=== MORSE CODE TO ENGLISH ===\n");
            printf("Enter Morse code (spaces between letters, / for word breaks)\n");
            printf("Example: .... . .-.. .-.. --- / .-- --- .-. .-.. -..\n");
            printf("\nInput: ");

            if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
                printf("\n[X] Error reading input.\n");
                crossSleep(1500);
                continue;
            }
            input[strcspn(input, "\n")] = '\0';

            if (!isValidInput(input)) {
                printf("\n[X] Invalid input. Please try again.\n");
                crossSleep(1500);
                continue;
            }

            morseToEnglish(input, output);

            printf("\n--- English Translation ---\n%s\n", output);
            printf("\n[OK] Translation completed!\n");

            printf("\nCopy to clipboard? (1=Yes, 0=No): ");
            int copyChoice;
            if (scanf("%d", &copyChoice) == 1 && copyChoice == 1) {
                copyToClipboard(output);
            } else if (copyChoice != 0) {
                printf("[X] Invalid choice.\n");
            }
            clearInputBuffer();

            addToHistory(input, output, 2);
        }
        else if (choice == 3) {
            viewAndReplayHistory();
            continue;
        }
        else if (choice == 4) {
            settingsMenu();
            continue;
        }
        else if (choice == 5) {
            clearScreen();
            printf("Quitting...\n\n");
            break;
        }
        else {
            printf("\n[X] Invalid choice. Please enter 1-5.\n");
            crossSleep(1500);
            continue;
        }

        printf("\nPress Enter to continue...");
        clearInputBuffer();
    }

    return 0;
}
