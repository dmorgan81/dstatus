/**
 * Which modules to include in the status bar.
 *
 * The order here determines the order they are displayed. If you change
 * anything here you'll probably want to change STATUS_FMT below.
 */
#define MODULES cpu, batt, time

/**
 * Format string for the status bar. man printf(3) for formats.
 */
#define STATUS_FMT "%s | %s | %s"

/**
 * Bar settings
 *
 * If bars are enabled these control the length and look of the bar. Example:
 * C:=====-----
 */
#define BAR_LEN 10
#define BAR_CHAR_ON '='
#define BAR_CHAR_OFF '-'

/**
 * Date/time settings
 *
 * man strftime(3) for formats. Note that your format must fit inside 
 * DATE_TIME_MAX_LEN
 * */
#define DATE_TIME_FMT "%F %I:%M %p"
#define DATE_TIME_MAX_LEN 64

/**
 * CPU settings
 *
 * Remove CPU_USE_BAR to display percentage instead of bar.
 */
#define CPU_FMT "C:%s"
#define CPU_USE_BAR

/**
 * Battery settings
 *
 * Remove BATT_USE_BAR to display percentage instead of bar.
 */
#define BATT_FMT "B:%c:%s"
#define BATT_USE_BAR
#define BATT_PATH "/sys/class/power_supply/BAT0"
#define BATT_CHARGE_NOW BATT_PATH "/charge_now"
#define BATT_CHARGE_FULL BATT_PATH "/charge_full"
#define BATT_STATUS BATT_PATH "/status"
