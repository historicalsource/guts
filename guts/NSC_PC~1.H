/****************************************************************************
    Copyright (C) 1995 National Semiconductor Corp.  All Rights Reserved
*****************************************************************************
*
*   File:               PCI_CFIG.H
*
*   Purpose:            Header file for PCI configuration manipulation code.
*
*   Update History:      1/10/95 GDV Created
*
****************************************************************************/

#ifndef PCI_CFIG_H
#define PCI_CFIG_H

#define PCI_DEFVAL_BUSMHZ   33          /* PCI bus speed in Mhz */

typedef unsigned long       DEVHANDLE;  /* handle to a PCI device */
					/*  to be used for access */

extern DEVHANDLE PCI_FindDevice		/* returns a handle for dev access */
(					/*  NULL is the device is not found */
    SBOOL *         usebios,            /* IN: TRUE to use the PCI BIOS */
					/* OUT: TRUE if using the PCI BIOS */
    DWORD           deviceID            /* PCI device ID to search for */
);

extern int PCI_ReadConfigByte      /* returns value of PCI byte register */
(
    DEVHANDLE       devhand,            /* handle for device access */
    BYTE            regaddr             /* register address to read from */
);

extern int PCI_ReadConfigWord      /* returns value of PCI word register */
(
    DEVHANDLE       devhand,            /* handle for device access */
    BYTE            regaddr             /* register address to read from */
);

extern unsigned long PCI_ReadConfigDWord      /* returns value of PCI DWord register */
(
    DEVHANDLE       devhand,            /* handle for device access */
    BYTE            regaddr             /* register address to read from */
);

extern void PCI_WriteConfigByte     /* writes single byte PCI register value */
(
    DEVHANDLE       devhand,            /* handle for device access */
    BYTE            regaddr,            /* register address to write to */
    BYTE            regval              /* value to write to that address */
);

extern void PCI_WriteConfigWord     /* writes single word PCI register value */
(
    DEVHANDLE       devhand,            /* handle for device access */
    BYTE            regaddr,            /* register address to write to */
    WORD            regval              /* value to write to that address */
);

extern void PCI_WriteConfigDword    /* writes double word PCI register value */
(
    DEVHANDLE       devhand,            /* handle for device access */
    BYTE            regaddr,            /* register address to write to */
    DWORD           regval              /* value to write to that address */
);

#endif

