/**
 * Which modules to include in the status bar.
 *
 * The order here determines the order they are displayed. If you change
 * anything here you'll probably want to change STATUS_FMT below.
 */
#define MODULES wifi, cpu, batt, backlight, vol, time

/**
 * Format string for the status bar. man printf(3) for formats.
 */
#define STATUS_FMT "%s | %s | %s | %s | %s | %s"

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

/**
 * Volume settings
 *
 * Remove VOL_USE_BAR to display percentage instead of bar.
 */
#define VOL_FMT "V:%s"
#define VOL_USE_BAR
#define VOL_MIXER "Master"
#define VOL_MUTED "[off]     "

/**
 * Backlight settings
 *
 * Remove BKLT_USE_BAR to display percentage instead of bar.
 */
#define BKLT_FMT "L:%s"
#define BKLT_USE_BAR
#define BKLT_PATH "/sys/class/backlight/radeon_bl0"
#define BKLT_MAX BKLT_PATH "/max_brightness"
#define BKLT_CURRENT BKLT_PATH "/actual_brightness"

/**
 * Wifi settings
 */
#define WIFI_FMT "W:%s"
#define WIFI_IFACE "wlan0"
#define WIFI_NO_CONNECT "------"

/**
 * ACPID socket location
 *
 * Unlikely that you'll have to change these.
 */
#define ACPID_SOCKET "/var/run/acpid.socket"
#define ACPID_EVENTS { "LID", "BRTUP", "BRTDN" }
