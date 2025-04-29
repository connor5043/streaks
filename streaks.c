#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h> // For access(), unlink(), rmdir(), rename()
#include <strings.h> // For strcasecmp (case-insensitive comparison)

// Helper function prototypes
void get_date_with_offset(char *date_buffer, size_t buffer_size, int offset);
void toggle_date_file_in_folder(const char *folder_path, const char *date);
int list_folders_in_data_directory(const char *data_directory, char *folders[], int max_folders);
void create_folder_in_data_directory(const char *data_directory, const char *folder_name);
void build_streak_string(const char *folder_path, char *streak_string);
int calculate_streak(const char *folder_path);
int delete_folder(const char *folder_path);
int rename_folder(const char *old_path, const char *new_path);
void create_streak_files(const char *folder_path, int days);
int is_day_in_days_file(const char *folder_path, const char *day);
void handle_days_file(const char *folder_path, const char *weekday_values);

// Comparison function for case-insensitive sorting
int case_insensitive_compare(const void *a, const void *b) {
    const char **str_a = (const char **)a;
    const char **str_b = (const char **)b;
    return strcasecmp(*str_a, *str_b); // Use strcasecmp for case-insensitive comparison
}

// Helper function to get the date in YYYY-MM-DD format for a given offset in days
void get_date_with_offset(char *date_buffer, size_t buffer_size, int offset) {
    time_t t = time(NULL) + offset * 86400; // Offset in seconds (1 day = 86400 seconds)
    struct tm tm = *localtime(&t);
    snprintf(date_buffer, buffer_size, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

// Helper function to parse a date string from user input
void parse_date_input(const char *input_date, char *parsed_date) {
    int year, month, day;
    if (sscanf(input_date, "%d-%d-%d", &year, &month, &day) == 3) {
        // Full YYYY-MM-DD format
        snprintf(parsed_date, 11, "%04d-%02d-%02d", year, month, day);
    } else if (sscanf(input_date, "%d-%d", &month, &day) == 2) {
        // MM-DD format, infer the current year
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        year = tm.tm_year + 1900;
        snprintf(parsed_date, 11, "%04d-%02d-%02d", year, month, day);
    } else {
        fprintf(stderr, "Error: Invalid date format. Use YYYY-MM-DD or MM-DD.\n");
        exit(1);
    }
}

// Helper function to toggle a date file in a folder
void toggle_date_file_in_folder(const char *folder_path, const char *date) {
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, date);

    // Check if the file already exists
    if (access(file_path, F_OK) == 0) {
        // File exists, delete it
        if (unlink(file_path) != 0) {
            perror("Error deleting file");
        }
    } else {
        // File does not exist, create it
        FILE *file = fopen(file_path, "w");
        if (!file) {
            perror("Error creating date file");
        } else {
            fclose(file);
        }
    }
}

// Helper function to create a folder in the specified directory
void create_folder_in_data_directory(const char *data_directory, const char *folder_name) {
    char folder_path[512];
    snprintf(folder_path, sizeof(folder_path), "%s/%s", data_directory, folder_name);

    // Create the folder under the data directory
    if (mkdir(folder_path, 0700) == -1) {
        if (errno != EEXIST) {
            perror("Error creating folder");
        }
    }
}

// Helper function to list folders in the data directory and return their count
int list_folders_in_data_directory(const char *data_directory, char *folders[], int max_folders) {
    DIR *dir;
    struct dirent *entry;
    int folder_count = 0;

    // Open the data directory
    dir = opendir(data_directory);
    if (!dir) {
        perror("Error opening data directory");
        return -1;
    }

    // Read the contents of the directory
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            folders[folder_count] = strdup(entry->d_name); // Store folder name
            folder_count++;
            if (folder_count >= max_folders) break;
        }
    }
    closedir(dir);

    // Sort the folder names alphabetically (case-insensitive)
    qsort(folders, folder_count, sizeof(char *), case_insensitive_compare);

    return folder_count;
}

// Helper function to build the 7-character string for a folder based on existing date files
void build_streak_string(const char *folder_path, char *streak_string) {
    char *days[] = {"su", "m", "tu", "w", "th", "f", "sa"};
    for (int i = -6; i <= 0; i++) { // Check from 6 days ago to today
        char date[11]; // Buffer for YYYY-MM-DD format
        get_date_with_offset(date, sizeof(date), i);

        // Determine the day of the week
        time_t t = time(NULL) + i * 86400;
        struct tm tm = *localtime(&t);
        const char *day_of_week = days[tm.tm_wday];

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, date);

        // Check if the file exists
        if (is_day_in_days_file(folder_path, day_of_week)) {
            streak_string[i + 6] = '/'; // Day exists in days.txt
        } else if (access(file_path, F_OK) == 0) {
            streak_string[i + 6] = 'X'; // Map offset (-6 to 0) to index (0 to 6)
        } else {
            streak_string[i + 6] = ' '; // File does not exist and day is not in days.txt
        }
    }
    streak_string[7] = '\0'; // Null terminate the string
}

int calculate_streak(const char *folder_path) {
    int streak = 0;
    char date[11]; // Buffer for YYYY-MM-DD format
    char *days[] = {"su", "m", "tu", "w", "th", "f", "sa"};

    for (int offset = -1;; offset--) {
        // Get the date for the current offset
        get_date_with_offset(date, sizeof(date), offset);

        // Determine the day of the week
        time_t t = time(NULL) + offset * 86400;
        struct tm tm = *localtime(&t);
        const char *day_of_week = days[tm.tm_wday];

        // Check if the day is listed in days.txt
        if (is_day_in_days_file(folder_path, day_of_week)) {
            // Skip this date if the day is in days.txt
            continue;
        }

        // Build the file path for the current date
        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, date);

        // Check if the file exists for the current date
        if (access(file_path, F_OK) == 0) {
            streak++; // File exists, increment the streak
        } else {
            break; // File does not exist, end the streak
        }
    }

    // Check if there is a file for today
    get_date_with_offset(date, sizeof(date), 0);
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, date);
    if (access(file_path, F_OK) == 0) {
        streak++; // Include today in the streak
    }

    return streak;
}

// Helper function to delete a folder and its contents
int delete_folder(const char *folder_path) {
    DIR *dir = opendir(folder_path);
    if (!dir) {
        perror("Error opening folder for deletion");
        return -1;
    }

    struct dirent *entry;
    char file_path[512];
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip special entries
        }
        snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, entry->d_name);
        if (entry->d_type == DT_DIR) {
            // Recursively delete subfolders
            delete_folder(file_path);
        } else {
            // Delete files
            if (unlink(file_path) != 0) {
                perror("Error deleting file");
            }
        }
    }
    closedir(dir);

    // Remove the folder itself
    if (rmdir(folder_path) != 0) {
        perror("Error deleting folder");
        return -1;
    }
    return 0;
}

// Helper function to rename a folder
int rename_folder(const char *old_path, const char *new_path) {
    if (rename(old_path, new_path) != 0) {
        perror("Error renaming folder");
        return -1;
    }
    return 0;
}

void create_streak_files(const char *folder_path, int days) {
    char date[11]; // Buffer for YYYY-MM-DD format
    char file_path[512];

    for (int i = 0; i < days; i++) {
        // Calculate the date for today, yesterday, the day before, etc.
        get_date_with_offset(date, sizeof(date), -i);

        // Build the full path for the file
        snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, date);

        // Create the file if it doesn't already exist
        FILE *file = fopen(file_path, "w");
        if (!file) {
            perror("Error creating streak file");
        } else {
            fclose(file);
        }
    }
}

int is_day_in_days_file(const char *folder_path, const char *day) {
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/days.txt", folder_path);

    // Open the file if it exists
    FILE *file = fopen(file_path, "r");
    if (!file) {
        return 0; // File does not exist or cannot be opened
    }

    char line[16];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;

        // Check if the day matches
        if (strcasecmp(line, day) == 0) {
            fclose(file);
            return 1; // Day is found in the file
        }
    }

    fclose(file);
    return 0; // Day is not in the file
}

void handle_days_file(const char *folder_path, const char *weekday_values) {
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/days.txt", folder_path);

    if (weekday_values == NULL || strlen(weekday_values) == 0) {
        // No weekday values provided, delete the file if it exists
        if (access(file_path, F_OK) == 0) {
            if (unlink(file_path) != 0) {
                perror("Error deleting days.txt file");
            }
        }
        return;
    }

    // Write the provided weekday values to the days.txt file
    FILE *file = fopen(file_path, "w");
    if (!file) {
        perror("Error creating or overwriting days.txt file");
        return;
    }

    // Split the weekday_values string by commas and write each value on a new line
    char *values = strdup(weekday_values);
    char *token = strtok(values, ",");
    while (token) {
        fprintf(file, "%s\n", token);
        token = strtok(NULL, ",");
    }
    free(values);

    fclose(file);
}

int main(int argc, char *argv[]) {
    char data_directory[512]; // Buffer to store the data directory path
    const int max_folders = 256;
    char *folders[max_folders];
    int folder_count = 0;

    // Determine the data directory based on the operating system
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set.\n");
        return 1;
    }
#ifdef __APPLE__
    snprintf(data_directory, sizeof(data_directory), "%s/Library/Application Support/streaks", home);
#else
    snprintf(data_directory, sizeof(data_directory), "%s/.local/share/streaks", home);
#endif

    // Create the data directory if it doesn't exist
    struct stat st = {0};
    if (stat(data_directory, &st) == -1) {
        if (mkdir(data_directory, 0700) == -1) {
            perror("Error creating data directory");
            return 1;
        }
    }

    // Generate the list of folders for all operations
    folder_count = list_folders_in_data_directory(data_directory, folders, max_folders);
    if (folder_count < 0) {
        fprintf(stderr, "Error: Unable to list folders.\n");
        return 1;
    }

    // Check for "a" or "add" arguments to create a folder
    if (argc >= 3 && (strcmp(argv[1], "a") == 0 || strcmp(argv[1], "add") == 0)) {
        const char *folder_name = argv[2];
        create_folder_in_data_directory(data_directory, folder_name);
        for (int i = 0; i < folder_count; i++) {
            free(folders[i]); // Free the allocated memory
        }
        return 0;
    }

    // Check for "rm" or "delete" arguments to delete a folder
    if (argc >= 3 && (strcmp(argv[1], "rm") == 0 || strcmp(argv[1], "del") == 0 || strcmp(argv[1], "delete") == 0)) {
        int folder_index = atoi(argv[2]) - 1; // Convert to zero-based index
        if (folder_index < 0 || folder_index >= folder_count) {
            fprintf(stderr, "Error: Invalid folder number.\n");
            for (int i = 0; i < folder_count; i++) {
                free(folders[i]); // Free the allocated memory
            }
            return 1;
        }

        // Get the folder path to delete
        char folder_path[512];
        snprintf(folder_path, sizeof(folder_path), "%s/%s", data_directory, folders[folder_index]);

        // Delete the folder
        if (delete_folder(folder_path) == 0) {
            printf("Deleted %s\n", folders[folder_index]);
        }

        // Free the allocated memory for folder names
        for (int i = 0; i < folder_count; i++) {
            free(folders[i]);
        }

        return 0;
    }

    // Check for "r" or "rename" arguments to rename a folder
    if (argc >= 4 && (strcmp(argv[1], "r") == 0 || strcmp(argv[1], "rename") == 0)) {
        int folder_index = atoi(argv[2]) - 1; // Convert to zero-based index
        if (folder_index < 0 || folder_index >= folder_count) {
            fprintf(stderr, "Error: Invalid folder number.\n");
            for (int i = 0; i < folder_count; i++) {
                free(folders[i]); // Free the allocated memory
            }
            return 1;
        }

        // Get the folder paths for renaming
        char old_path[512], new_path[512];
        snprintf(old_path, sizeof(old_path), "%s/%s", data_directory, folders[folder_index]);
        snprintf(new_path, sizeof(new_path), "%s/%s", data_directory, argv[3]);

        // Rename the folder
        if (rename_folder(old_path, new_path) == 0) {
            printf("Renamed folder: %s -> %s\n", folders[folder_index], argv[3]);
        }

        // Free the allocated memory for folder names
        for (int i = 0; i < folder_count; i++) {
            free(folders[i]);
        }

        return 0;
    }

	// Check for "s" or "since" arguments to create streak files for a range of days
	if (argc >= 4 && (strcmp(argv[1], "s") == 0 || strcmp(argv[1], "since") == 0)) {
		int folder_index = atoi(argv[2]) - 1; // Convert to zero-based index
		int days = atoi(argv[3]); // Number of days to create files for

		if (folder_index < 0 || folder_index >= folder_count || days <= 0) {
		    fprintf(stderr, "Error: Invalid folder number or number of days.\n");
		    for (int i = 0; i < folder_count; i++) {
		        free(folders[i]); // Free the allocated memory
		    }
		    return 1;
		}

		// Get the folder path
		char folder_path[512];
		snprintf(folder_path, sizeof(folder_path), "%s/%s", data_directory, folders[folder_index]);

		// Create streak files for the specified number of days
		create_streak_files(folder_path, days);

		// Free the allocated memory for folder names
		for (int i = 0; i < folder_count; i++) {
		    free(folders[i]);
		}

		return 0;
	}
	
	// Check for "d" or "days" arguments to manage the days.txt file
	if (argc >= 3 && (strcmp(argv[1], "days") == 0)) {
		int folder_index = atoi(argv[2]) - 1; // Convert to zero-based index
		const char *weekday_values = (argc >= 4) ? argv[3] : NULL; // Comma-separated weekday values or NULL

		if (folder_index < 0 || folder_index >= folder_count) {
		    fprintf(stderr, "Error: Invalid folder number.\n");
		    for (int i = 0; i < folder_count; i++) {
		        free(folders[i]); // Free the allocated memory
		    }
		    return 1;
		}

		// Get the folder path
		char folder_path[512];
		snprintf(folder_path, sizeof(folder_path), "%s/%s", data_directory, folders[folder_index]);

		// Handle the days.txt file
		handle_days_file(folder_path, weekday_values);

		// Free the allocated memory for folder names
		for (int i = 0; i < folder_count; i++) {
		    free(folders[i]);
		}

		return 0;
	}
	
    // Check for "t" or "toggle" arguments to handle date files
    if (argc >= 3 && (strcmp(argv[1], "t") == 0 || strcmp(argv[1], "toggle") == 0)) {
        int folder_index = atoi(argv[2]) - 1; // Convert to zero-based index
        if (folder_index < 0 || folder_index >= folder_count) {
            fprintf(stderr, "Error: Invalid folder number.\n");
            for (int i = 0; i < folder_count; i++) {
                free(folders[i]); // Free the allocated memory
            }
            return 1;
        }

        // Get the selected folder path
        char folder_path[512];
        snprintf(folder_path, sizeof(folder_path), "%s/%s", data_directory, folders[folder_index]);

        // Parse the date input if provided
        char date[11]; // Buffer for YYYY-MM-DD format
        if (argc >= 4) {
            parse_date_input(argv[3], date);
        } else {
            get_date_with_offset(date, sizeof(date), 0); // Default to today's date
        }

        // Toggle the date file in the selected folder
        toggle_date_file_in_folder(folder_path, date);

        // Free the allocated memory for folder names
        for (int i = 0; i < folder_count; i++) {
            free(folders[i]);
        }

        return 0;
    }

	// Check for "h" or "help" arguments to display help information
	if (argc >= 2 && (strcmp(argv[1], "h") == 0 || strcmp(argv[1], "help") == 0)) {
		printf("Usage: streaks [command] [arguments]\n\n");
		printf("Commands:\n");
		printf("  h(elp)                     Show this help message\n");
		printf("  a(dd) [name]               Add a new streak with the given name\n");
		printf("  rm / del(ete) [number]     Delete a streak\n");
		printf("  r(ename) [number] [name]   Rename a streak\n");
		printf("  t(oggle) [number] [date]   Toggle streak completion for a date (default: today)\n");
		printf("  s(ince) [number] [days]    For importing an existing streak\n");
		printf("  days [number] [values]   Days to exclude (e.g., 'f,sa,su')\n\n");
		printf("Default Behavior:\n");
		printf("  Without arguments, the program lists all streaks.\n");
		printf("\n");
		return 0;
	}

    // Default behavior: Print the streak sequence and list folders with streak strings
    char *days = "SMTWTFS";
    char streak[8]; // Array to store the streak sequence (7 days + null terminator)
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    int today = tm.tm_wday;

    // Create the streak sequence, making the current day appear last
    for (int i = 0; i < 7; i++) {
        streak[i] = days[(today + i + 1) % 7];
    }
    streak[6] = days[today]; // Place the current day as the last character
    streak[7] = '\0'; // Null terminate the string

    // Print the streak sequence
    printf("   %s\n", streak);

    // List and print folders in the data directory with their streak strings
    for (int i = 0; i < folder_count; i++) {
        char folder_path[512];
        snprintf(folder_path, sizeof(folder_path), "%s/%s", data_directory, folders[i]);

        char streak_string[8]; // Buffer for the 7-character streak string
        build_streak_string(folder_path, streak_string);

        // Calculate the streak for the folder
        int streak = calculate_streak(folder_path);

        printf("%d. %s %dd %s\n", i + 1, streak_string, streak, folders[i]);
        free(folders[i]); // Free the allocated memory
    }

    return 0;
}
