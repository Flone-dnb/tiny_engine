#pragma once

// Returns path to the directory to store game config files.
// The path ends with a forward slash.
// Do not free/destroy returned pointer.
const char* paths_get_config_dir(void);

// Returns path of the file to write game logs.
// Do not free/destroy returned pointer.
const char* paths_get_log_file(void);
