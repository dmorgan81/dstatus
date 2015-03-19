/**
 * Which modules to include in the status bar.
 *
 * The order here determines the order they are displayed.
 */
#define MODULES time

/**
 * Separate modules by this string
 */
#define SEPARATOR " | "

/**
 * Bar settings
 *
 * If bars are enabled these control the length and look of the bar. Example:
 * C:=====-----
 */
#define BAR_LEN 10
#define BAR_CHAR_ON =
#define BAR_CHAR_OFF -

/**
 * Date/time settings
 *
 * man strftime(3) for formats. Note that your format must fit inside 
 * DATE_TIME_MAX_LEN
 * */
#define DATE_TIME_FMT "%F %I:%M %p"
#define DATE_TIME_MAX_LEN 64
