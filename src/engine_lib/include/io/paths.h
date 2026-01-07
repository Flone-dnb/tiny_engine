#pragma once

/**
 * Returns pointer to a static string that stores path to the directory to store game config files.
 * The path ends with a forward slash.
 *
 * @return Path.
 */
const char* paths_get_config_dir(void);

/**
 * Returns pointer to a static string that stores path to the file to store game log.
 *
 * @return Path.
 */
const char* paths_get_log_file(void);
