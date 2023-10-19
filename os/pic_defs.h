/*
 * Result codes from PIC functions.
 */

#define SUCCESS	0x00
#define FAILURE	0x80

/*
 * The offsets to the BCD data are as follows:
 *
 *  Offset      Meaning         Range
 *    0         Second          00-59
 *    1         Minute          00-59
 *    2         Hour            00-23
 *    3         Day Of Week     01-07
 *    4         Day Of Month    01-31
 *    5         Month           01-12
 *    6         Year            00-99
 *
 */

#define SECONDS         0
#define MINUTES         1
#define HOURS           2
#define DAY_OF_WEEK     3
#define DAY_OF_MONTH    4
#define MONTH           5
#define YEAR            6


#define SERIAL_NUMBER_SIZE 16
