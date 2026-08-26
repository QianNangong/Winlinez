#ifndef RESOURCE_H
#define RESOURCE_H

/* Resource IDs recovered from the original binary (decimal = original values) */

#define IDI_ICON1        3000   /* 0xBB8 */
#define IDR_MENU1        1100   /* 0x44C */
#define IDR_ACCEL1       1100

#define IDD_RULES        1000   /* 0x3E8 */
#define IDD_SCORES       1001   /* 0x3E9 */
#define IDD_NAME         1002   /* 0x3EA */
#define IDD_ABOUT        1003   /* 0x3EB */
#define IDD_STATS        1004   /* 0x3EC */

#define IDB_ABOUT_KING   2000   /* 0x7D0 */
#define IDB_ABOUT_MASK   2001   /* 0x7D1 */

/* Menu commands */
#define CM_NEW           0x100
#define CM_SCORES        0x101
#define CM_EXIT          0x102
#define CM_NEXT          0x200
#define CM_STATS         0x201
#define CM_RULES         0x300
#define CM_ABOUT         0x301

/* Dialog controls */
#define IDOK             1
#define IDC_NAME         0x101

#endif
